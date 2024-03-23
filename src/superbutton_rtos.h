/**
* @file	superbutton_rtos.h
* @date	2023-08-12
* @version	v0.0.1
*
*/

/*! @file superbutton_rtos.h
 * @brief SuperButton library for FreeRTOS
 */

/*!
 * @defgroup SuperButton
 */
#ifndef SUPERBUTTON_RTOS_H_
#define SUPERBUTTON_RTOS_H_

/*! CPP guard */
#ifdef __cplusplus
extern "C" {
#endif

/* Header includes */
#include "freertos/FreeRTOS.h"
#include <driver/gpio.h>

/*
     ___________________________________________
____|                                           |_____________
     <----long-click min-->
                           ^                    ^
                           |_Long press start   | long click
Explanation: When the button is held down for at least the "long-click min" duration, the long press action starts
             A long click is detected when the button is released after being held down for the specified duration.
     
     ______     _____         _____
____|      |___|     |__...__|     |_________________________
                                    <----->  ^
                                       ^     |_N-le click
                                       |_Multi click gap                                       
     ______     _____
____|      |___|     |_________________________
                      <------->
                          ^   ^
                          |   |_Double click
                          |_Multi click gap max
Explanation: A triple click is detected when the button is clicked three times in succession within
             a certain time frame (multi click gap max). A double click is detected if two clicks
             occur within a shorter time frame (double click gap max) but are separated by more
             than the multi click gap.

     ______
____|      |___________________________________
           <------->
              ^    ^
              |    |_Single click detected
              |_Multi click gap max.
Explanation: A single click is detected when the button is pressed and released
             within the time frame defined as the multi click gap max.
*/


typedef enum
{
    SUPER_BUTTON_BUTTON_EMPTY = 0,
    SUPER_BUTTON_BUTTON_DOWN = 1 << 0,
    SUPER_BUTTON_BUTTON_UP = 1 << 1,
    SUPER_BUTTON_SINGLE_CLICK = 1 << 2,
    SUPER_BUTTON_MULTI_CLICK = 1 << 3,
    SUPER_BUTTON_LONG_CLICK = 1 << 4,
    SUPER_BUTTON_LONG_PRESS_START = 1 << 5
} super_button_click_type_t;

typedef enum
{
    SUPER_BUTTON_PULL_MODE_CHIP,
    SUPER_BUTTON_PULL_MODE_HW
} super_button_pull_mode_t;

typedef enum
{
    SUPER_BUTTON_PULL_UP,
    SUPER_BUTTON_PULL_DOWN
} super_button_pull_direction_t;

typedef struct
{
    gpio_num_t button_gpio_num;
    void * user_data;
} super_button_button_t;

typedef struct
{
    TickType_t debounce_ms;
    TickType_t multi_click_gap_ms;
    TickType_t long_press_start_gap_ms;
} super_button_config_t;


typedef struct
{
    super_button_button_t button;
    super_button_click_type_t click_type;
    uint8_t click_count;
    uint32_t duration_ms;
} super_button_click_event_args_t;

typedef enum
{
    SUPPER_BUTTON_UP,
    SUPPER_BUTTON_DOWN,
    SUPPER_BUTTON_UNDEF
} button_state_t;

void superbutton_init(super_button_button_t buttons[], uint8_t len, super_button_pull_mode_t pull_mode, super_button_pull_direction_t pull_direction, QueueHandle_t queue, super_button_config_t config);
super_button_config_t superbutton_create_default_config();

#ifdef __cplusplus
}
#endif /* End of CPP guard */
#endif /* SUPERBUTTON_RTOS_H_ */
/** @}*/
