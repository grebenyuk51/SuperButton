/**
* @file	superbutton_rtos_defs.h
* @date	2022-08-12
* @version	v0.0.1
*
*/

/*! @file superbutton_rtos_defs.h
 * @brief Supper button
 */

/*!
 * @defgroup HTU31D SENSOR API
 * @brief
 */
#ifndef SUPERBUTTON_RTOS_DEFS_H_
#define SUPERBUTTON_RTOS_DEFS_H_

/********************************************************/
/* header includes */
#include <stdint.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <driver/gpio.h>

#define SUPER_BUTTON_DEBOUNCE_MS (TickType_t)(25)
#define SUPER_BUTTON_MULTI_CLICK_GAP_MS (TickType_t)(200)
#define SUPER_BUTTON_LONG_PRESS_START_GAP_MS (TickType_t)(400)

/********************************************************/

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
    SUPER_BUTTON_BUTTON_EMPTY = 0x00,
    SUPER_BUTTON_BUTTON_DOWN = 0x01,
    SUPER_BUTTON_BUTTON_UP = 0x02,
    SUPER_BUTTON_SINGLE_CLICK = 0x04,
    SUPER_BUTTON_MULTI_CLICK = 0x08,
    SUPER_BUTTON_LONG_CLICK = 0x10,
    SUPER_BUTTON_LONG_PRESS_START = 0x20
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
} super_button_button_t;


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

extern QueueHandle_t super_button_queue;

#endif /* HTU31D_DEFS_H_ */
/** @}*/
/** @}*/