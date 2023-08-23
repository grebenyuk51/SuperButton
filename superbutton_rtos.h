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
#include "superbutton_rtos_defs.h"

void superbutton_init(super_button_button_t buttons[], uint8_t len, super_button_pull_mode_t pull_mode, super_button_pull_direction_t pull_direction);


#ifdef __cplusplus
}
#endif /* End of CPP guard */
#endif /* SUPERBUTTON_RTOS_H_ */
/** @}*/
