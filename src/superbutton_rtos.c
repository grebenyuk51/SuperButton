#include "superbutton_rtos.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "string.h"

static QueueHandle_t buttonQueue;
static QueueHandle_t eventButtonQueue;

typedef struct
{
	gpio_num_t button;
	button_state_t current_state;
    button_state_t last_true_state;
	TickType_t current_tick_count;
    TickType_t last_true_state_tick_count;
    uint16_t click_count;
    super_button_click_type_t click_type;
    bool is_busy;
    bool is_from_queue;
    void *user_data;
} button_press_info_t;
typedef struct
{
    uint8_t button_info_index;
    int raw_level;
    TickType_t tick_count;
} button_args_t;

static super_button_pull_direction_t global_pull_direction = SUPER_BUTTON_PULL_DOWN;
static super_button_config_t global_config;
static button_press_info_t *button_info;
static uint8_t button_info_len;
static button_args_t *button_args;
static uint8_t button_args_len;

static void button_isr_handler(void* arg);
void process_button_events_after_interrupt(void* pvParameters);
void send_event(button_press_info_t *args);
button_state_t map_button_state_vs_raw_level(int raw_level);

void superbutton_init(super_button_button_t *buttons, uint8_t len, super_button_pull_mode_t pull_mode, super_button_pull_direction_t pull_direction, QueueHandle_t queue, super_button_config_t config)
{
    global_config = config;
    eventButtonQueue = queue;
    buttonQueue = xQueueCreate(20 * len, sizeof(button_args_t));
    //vTaskSuspendAll();
    button_info_len = len;
    button_info = (button_press_info_t *)calloc(sizeof(button_press_info_t) * button_info_len, button_info_len);
    //memset(button_info, 0, sizeof(button_press_info_t) * button_info_len);
    button_args = (button_args_t *)calloc(sizeof(button_args_t) * len, len);
    //memset(button_args, 0, sizeof(button_args_t) * len);
    //xTaskResumeAll();

    gpio_num_t gpio = 0;
    uint64_t pin_bit_mask = 0;
    for (uint8_t i = 0; i < len; i++)
    {
        gpio = buttons[i].button_gpio_num;
        pin_bit_mask |= (1ULL << gpio);

        button_press_info_t a =
        {
            .button = gpio,
            .current_tick_count = 0,
            .last_true_state_tick_count = 0,
            .click_type = 0,
            .is_busy = false,
            .is_from_queue = false,
            .user_data = buttons[i].user_data,
        };
        button_info[i] = a;
        button_args[i].button_info_index = i;
        if(pull_direction == SUPER_BUTTON_PULL_UP)
        {
            a.current_state = SUPPER_BUTTON_UP;
            a.last_true_state = SUPPER_BUTTON_UP;
        }
        else
        {
            a.current_state = SUPPER_BUTTON_DOWN;
            a.last_true_state = SUPPER_BUTTON_DOWN;
        }
    }
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.pin_bit_mask = pin_bit_mask;
    io_conf.mode = GPIO_MODE_INPUT;

    if(pull_mode == SUPER_BUTTON_PULL_MODE_HW)
    {
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
	    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    }
    else
    {
        if(pull_direction == SUPER_BUTTON_PULL_UP)
        {
            io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        }
        else
        {
            io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
            io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
        }
    }
    global_pull_direction = pull_direction;
 
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    for (int i = 0; i < len; i++)
    {
        gpio_isr_handler_add(buttons[i].button_gpio_num, button_isr_handler, (void *)i);
    }

    xTaskCreate(process_button_events_after_interrupt, "Button Event Task",
    3072, NULL, 1, NULL);
    //configMINIMAL_STACK_SIZE, NULL, 1, NULL);
}

static void IRAM_ATTR button_isr_handler(void* arg) 
{
    static button_args_t args;
    static BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    static int level;
    static int b_info_index;

    b_info_index = (int)arg;    
    args.button_info_index = b_info_index;
    args.raw_level = gpio_get_level(button_info[b_info_index].button);  //bad choice. bad buttons could get false state readings (down..up[read]..down.........)
    args.tick_count = xTaskGetTickCountFromISR();
    
    xQueueSendFromISR(buttonQueue, &args, &xHigherPriorityTaskWoken);

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    // if (xHigherPriorityTaskWoken) 
    // {
    //     portYIELD_FROM_ISR();
    // }
}

void process_button_events_after_interrupt(void* pvParameters) 
{
    QueueHandle_t current_queue = buttonQueue;
    TickType_t prev_event_tick = 0;
    static TickType_t current_period = 0;
    static TickType_t current_tick = 0;
    static TickType_t queue_receive_delay = portMAX_DELAY;
    static bool isbusy = 0;    

    button_press_info_t *current_button_info;

    while(true)
    {
        button_args_t args;
        BaseType_t err = xQueueReceive(current_queue, &args, queue_receive_delay);

        if(err == pdPASS)
        {
            TickType_t debounce_ticks = args.tick_count - button_info[args.button_info_index].last_true_state_tick_count;
            if(debounce_ticks < pdMS_TO_TICKS(global_config.debounce_ms))
            {
                //ESP_LOGI("Debounce", "gpio: %d level: %d, debounce_ticks: %lu, tick: %lu", button_info[args.button_info_index].button, map_button_state_vs_raw_level(args.raw_level), debounce_ticks, args.tick_count);
                continue;
            }
            else
            {
                button_info[args.button_info_index].current_state = map_button_state_vs_raw_level(args.raw_level);
                button_info[args.button_info_index].current_tick_count = args.tick_count;
                button_info[args.button_info_index].is_busy = true;
                button_info[args.button_info_index].is_from_queue = true;
            }
        }

        isbusy = false;
        for(int i = args.button_info_index; i < button_info_len + args.button_info_index; i++)
        {
            current_button_info = &button_info[i % button_info_len];

            if(!current_button_info->is_busy)
            {
                continue;
            }

            if(button_info[args.button_info_index].is_from_queue)
            {
                current_tick = current_button_info->current_tick_count;
            }
            else
            {
                current_tick = xTaskGetTickCount();
            }

            current_period = current_tick - current_button_info->last_true_state_tick_count;
            //todo: current_period != click_period. maybe should measure down period as well 

            ESP_LOGI("Super Button Internal Event", "button=%d, level=%d, current_tick_count=%lu", current_button_info->button, current_button_info->current_state, current_tick);

            if(current_button_info->current_state != current_button_info->last_true_state)
            {
                current_button_info->click_type = current_button_info->current_state == SUPPER_BUTTON_UP ? SUPER_BUTTON_BUTTON_UP : SUPER_BUTTON_BUTTON_DOWN;
                send_event(current_button_info);
            }

            if(current_button_info->current_state == SUPPER_BUTTON_UP && current_button_info->last_true_state == SUPPER_BUTTON_DOWN)
            {
                current_button_info->click_count++;
            }

            if(current_period >= pdMS_TO_TICKS(global_config.long_press_start_gap_ms))
            {
                if(current_button_info->current_state == SUPPER_BUTTON_DOWN && current_button_info->last_true_state == SUPPER_BUTTON_DOWN)
                {
                    //todo: boot with button down
                    current_button_info->click_type = SUPER_BUTTON_LONG_PRESS_START;
                    send_event(current_button_info);
                    current_button_info->click_count = 0;
                    current_button_info->is_busy = false;
                }
                else if(current_button_info->click_count == 1)
                {
                    current_button_info->click_type = SUPER_BUTTON_LONG_CLICK;
                    send_event(current_button_info);
                    current_button_info->click_count = 0;
                    current_button_info->is_busy = false;
                }
            }
            else if(current_period >= pdMS_TO_TICKS(global_config.multi_click_gap_ms))
            {
                if(current_button_info->click_count > 0)
                {
                    current_button_info->click_type = current_button_info->click_count == 1 ? SUPER_BUTTON_SINGLE_CLICK : SUPER_BUTTON_MULTI_CLICK;
                    send_event(current_button_info);
                    current_button_info->click_count = 0;
                }
                else
                {
                    
                }
                if(current_button_info->current_state == SUPPER_BUTTON_UP)  //if button down --> long_press
                {
                    current_button_info->is_busy = false;
                }
            }

            // if(current_button_info->current_state == SUPPER_BUTTON_UP && current_button_info->last_true_state == SUPPER_BUTTON_UNDEF)
            // {
            //     //this situation could be when we booted with button pressed. maybe should read pin at start
            //     current_button_info->is_busy = 0;
            // }

            if(current_button_info->current_state != current_button_info->last_true_state)
            {
                current_button_info->last_true_state = current_button_info->current_state;
                current_button_info->last_true_state_tick_count = current_button_info->current_tick_count;
            }
            
            isbusy |= current_button_info->is_busy;
            button_info[args.button_info_index].is_from_queue = false;
        }
        if(isbusy)
        {
            queue_receive_delay = pdMS_TO_TICKS(global_config.multi_click_gap_ms);
        }
        else
        {
            queue_receive_delay = portMAX_DELAY;
        }
    }
}

void send_event(button_press_info_t *args)
{
    static super_button_click_event_args_t result;
    result.button.button_gpio_num = args->button;
    result.button.user_data = args->user_data;
    result.click_count = args->click_count;
    result.click_type = args->click_type;
    xQueueSend(eventButtonQueue, &result, 0);
}

super_button_config_t superbutton_create_default_config()
{
    super_button_config_t c =
    {
        .debounce_ms = 25,
        .multi_click_gap_ms = 180,
        .long_press_start_gap_ms = 800
    };
    return c;
}

button_state_t get_button_state(super_button_button_t button)
{
    return get_button_state_by_gpio(button.button_gpio_num);
}

button_state_t get_button_state_by_gpio(gpio_num_t gpio)
{
    for (int i = 0; i < button_info_len; i++)
    {
        if(button_info[i].button == gpio)
        {
            return button_info[i].current_state;
        }
    }
    return SUPPER_BUTTON_UNDEF;
}

button_state_t map_button_state_vs_raw_level(int raw_level)
{
    button_state_t bs = SUPPER_BUTTON_UNDEF;
    if(global_pull_direction == SUPER_BUTTON_PULL_DOWN)
    {
        bs = raw_level == 0 ? SUPPER_BUTTON_UP : SUPPER_BUTTON_DOWN;
    }
    else
    {
        bs = raw_level == 0 ? SUPPER_BUTTON_DOWN : SUPPER_BUTTON_UP;
    }
    return bs;
}