#include "superbutton_rtos.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"

static QueueHandle_t buttonQueue;

typedef struct
{
	gpio_num_t button;
	button_state_t current_level;
    button_state_t last_true_level;
	TickType_t current_tick_count;
    TickType_t last_true_level_tick_count;
    uint16_t click_count;
    super_button_click_type_t click_type;
    uint8_t is_busy;
    void *user_data;
} button_press_info_t;
typedef struct
{
    uint8_t button_info_index;
} button_args_t;


static super_button_pull_direction_t global_pull_direction = SUPER_BUTTON_PULL_DOWN;
static button_press_info_t *button_info;
static uint8_t button_info_len;
static button_args_t *button_args;
static uint8_t button_args_len;

static void button_isr_handler(void* arg);
void process_button_events_after_interrupt(void* pvParameters);
void send_event(button_press_info_t *args);

void superbutton_init(super_button_button_t *buttons, uint8_t len, super_button_pull_mode_t pull_mode, super_button_pull_direction_t pull_direction)
{
    buttonQueue = xQueueCreate(5 * len, sizeof(button_args_t));
    button_info_len = len;
    button_info = (button_press_info_t *)pvPortMalloc(sizeof(button_press_info_t) * button_info_len);
    button_args = (button_args_t *)pvPortMalloc(sizeof(button_args_t) * len);
    

    gpio_num_t gpio = 0;
    uint64_t pin_bit_mask = 0;
    for (uint8_t i = 0; i < len; i++)
    {
        gpio = buttons[i].button_gpio_num;
        pin_bit_mask |= (1ULL << gpio);

        button_press_info_t a =
        {
            .button = gpio,
            .current_level = SUPPER_BUTTON_UNDEF,
            .last_true_level = SUPPER_BUTTON_UNDEF,
            .current_tick_count = 0,
            .last_true_level_tick_count = 0,
            .click_type = 0,
            .is_busy = 0,
            .user_data = buttons[i].user_data,
        };
        button_info[i] = a;
        button_args[i].button_info_index = i;
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
    for (uint8_t i = 0; i < len; i++)
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
    static button_press_info_t *b_args;
    static BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    static int level;
    
    args.button_info_index = (uint8_t)arg;
	b_args = &button_info[args.button_info_index];
    level = gpio_get_level(b_args->button);
    if(global_pull_direction == SUPER_BUTTON_PULL_DOWN)
    {
        b_args->current_level = level == 0 ? SUPPER_BUTTON_UP : SUPPER_BUTTON_DOWN;
    }
    else
    {
        b_args->current_level = level == 0 ? SUPPER_BUTTON_DOWN : SUPPER_BUTTON_UP;
    }
	b_args->current_tick_count = xTaskGetTickCountFromISR();
    b_args->is_busy = 1;
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
    TickType_t current_tick = 0;
    static TickType_t queue_receive_delay = portMAX_DELAY;
    static uint8_t isbusy = 0;
    

    button_press_info_t *current_button_info;

    while(true)
    {
        button_args_t args;
        BaseType_t err = xQueueReceive(current_queue, &args, queue_receive_delay);

        isbusy = 0;
        for(int i = args.button_info_index; i < button_info_len + args.button_info_index; i++)
        {
            current_button_info = &button_info[i % button_info_len];
            if(current_button_info->is_busy == 0)
            {
                continue;
            }
            isbusy |= current_button_info->is_busy;
            if(err == errQUEUE_EMPTY)
            {
                current_tick = xTaskGetTickCount();
            }
            else
            {
                current_tick = current_button_info->current_tick_count;
            }

            TickType_t current_period = current_tick - current_button_info->last_true_level_tick_count;
            //todo: current_period != click_period. maybe should measure down period as well 

            ESP_LOGI("Super Button Internal Event", "button=%d, level=%d, current_tick_count=%lu", current_button_info->button, current_button_info->current_level, current_button_info->current_tick_count);

            if(current_period > pdMS_TO_TICKS(SUPER_BUTTON_DEBOUNCE_MS))
            {
                if(current_button_info->current_level != current_button_info->last_true_level)
                {
                    current_button_info->click_type = current_button_info->current_level == SUPPER_BUTTON_UP ? SUPER_BUTTON_BUTTON_UP : SUPER_BUTTON_BUTTON_DOWN;
                    send_event(current_button_info);
                    current_button_info->click_type = SUPER_BUTTON_BUTTON_EMPTY;
                }

                if(current_button_info->current_level == SUPPER_BUTTON_UP && current_button_info->last_true_level == SUPPER_BUTTON_DOWN)
                {
                    current_button_info->click_count++;
                }
                else if(current_button_info->current_level == SUPPER_BUTTON_UP && current_button_info->last_true_level == SUPPER_BUTTON_UNDEF)
                {
                    //this situation could be when we booted with button pressed. maybe should read pin at start
                    current_button_info->is_busy = 0;
                }

                if(current_period >= pdMS_TO_TICKS(SUPER_BUTTON_MULTI_CLICK_GAP_MS) && current_period < pdMS_TO_TICKS(SUPER_BUTTON_LONG_PRESS_START_GAP_MS))
                {
                    if(current_button_info->click_count > 0)
                    {
                        current_button_info->click_type = current_button_info->click_count == 1 ? SUPER_BUTTON_SINGLE_CLICK : SUPER_BUTTON_MULTI_CLICK;
                        send_event(current_button_info);
                        current_button_info->click_count = 0;
                        current_button_info->click_type = SUPER_BUTTON_BUTTON_EMPTY;
                    }
                    if(current_button_info->current_level == SUPPER_BUTTON_UP)  //if button down --> long_press
                    {
                        current_button_info->is_busy = 0;
                    }
                }
                else if(current_period >= pdMS_TO_TICKS(SUPER_BUTTON_LONG_PRESS_START_GAP_MS))
                {
                    if(current_button_info->current_level == SUPPER_BUTTON_DOWN && current_button_info->last_true_level == SUPPER_BUTTON_DOWN &&  current_button_info->click_type == SUPER_BUTTON_BUTTON_EMPTY)
                    {
                        //todo: boot with button down
                        current_button_info->click_type = SUPER_BUTTON_LONG_PRESS_START;
                        send_event(current_button_info);
                        current_button_info->click_count = 0;
                        current_button_info->is_busy = 0;
                    }
                    else if(current_button_info->click_count == 1)
                    {
                        current_button_info->click_type = SUPER_BUTTON_LONG_CLICK;
                        send_event(current_button_info);
                        current_button_info->click_count = 0;
                        current_button_info->click_type = SUPER_BUTTON_BUTTON_EMPTY;
                        current_button_info->is_busy = 0;
                    }
                }



                if(current_button_info->current_level != current_button_info->last_true_level)
                {
                    current_button_info->last_true_level = current_button_info->current_level;
                    current_button_info->last_true_level_tick_count = current_button_info->current_tick_count;
                }
            }
            else
            {
                vPortYield();
                continue;
            }
        }
        if(isbusy)
        {
            queue_receive_delay = pdMS_TO_TICKS(200);
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

    xQueueSend(super_button_queue, &result, 0);
}