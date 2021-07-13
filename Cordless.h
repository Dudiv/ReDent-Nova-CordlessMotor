/**
 * Structs and Enums for cordless motor programm
 *
 */
#ifndef CordlessBoard_H
#define CordlessBoard_H

#ifdef __cplusplus
extern "C" {
#endif

#include "nrf_gpio.h"
#include "custom_board.h"

#define SET_RED_ON (nrf_gpio_pin_set(RED), nrf_gpio_pin_clear(GREEN))
#define RESET_RED_ON nrf_gpio_pin_clear(RED)

#define SET_GREEN_ON (nrf_gpio_pin_set(GREEN), nrf_gpio_pin_clear(RED))
#define RESET_GREEN_ON nrf_gpio_pin_clear(GREEN)

#define SET_BLUE_ON nrf_gpio_pin_set(BLUE)
#define RESET_BLUE_ON nrf_gpio_pin_clear(BLUE)
#define TOGGLE_BLUE nrf_gpio_pin_toggle(BLUE)

#define TOGGLE_HOLD_POWER nrf_gpio_pin_toggle(HOLD_POWER)

#define CLEAR_BOOST_EN nrf_gpio_pin_set(BOOST_EN)
#define SET_BOOST_EN nrf_gpio_pin_clear(BOOST_EN)

typedef enum{
  OFF = 0,
  ON,
}LEVELS;

typedef enum{
   BATTERY_LOW,
   BATTERY_OK,
}BATTERY_STATE;


typedef enum{
	MOVE_TO_IDEL,
	IDEL,
	MOVE_TO_ACTIVE,
	ACTIVE
}STATE_MACHINE;



#ifdef __cplusplus
}
#endif

#endif // PCA10040_H
