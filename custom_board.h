/**
 * 
 *
 */
#ifndef CUSTOM_BOARD_H
#define CUSTOM_BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "nrf_gpio.h"

#define USER_BUTTON 11 // Was 3 on EYSHSNZWZ module   // Digital input - User button, HIGH if pressed
#define HOLD_POWER  2   // Digital output - Hold power after UB switch released
#define HANDPIECE_PIN   20  // Digital input - Indication if handpiece connection - LOW if connected
#define PWM         28  // Digital output - PWM output to motor driver
#define BOOST_EN    29  // It was 6 on EYSHSNZWZ module   // Digital output - Enable of BOOST inverter - 
#define MOTOR_FREQ_PIN  4   // Digital input - Frequency proportional to motor speed
#define BAT_VOLT    5   // Analog input - Battery voltage ( divided by 2 )
#define BLUE        8   // Digital output - BLUE LED, HIGH led light
#define RED         17 // Was 3 on EYSHSNZWZ module 18   // Digital output - RED LED, HIGH led light
#define GREEN       19 // Was 3 on EYSHSNZWZ module 7   // Digital output - GREEN LED, HIGH led light


#ifdef __cplusplus
}
#endif

#endif // CUSTOM_BOARD_H
