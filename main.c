/**
 * Copyright (c) 2016 - 2019, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/** @file
 *
 * @defgroup nrf5_sdk_for_eddystone main.c
 * @{
 * @ingroup nrf5_sdk_for_eddystone
 * @brief Eddystone Beacon GATT Configuration Service + EID/eTLM sample application main file.
 *
 * This file contains the source code for an Eddystone
 * Beacon GATT Configuration Service + EID/eTLM sample application.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "bsp.h"
#include "ble_conn_params.h"
#include "ble_advertising.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "app_timer.h"
#include "es_app_config.h"
#include "app_scheduler.h"
#include "nrf_ble_es.h"
#include "nrf_ble_gatt.h"
#include "nrf_pwr_mgmt.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "Cordless.h"
#include "custom_board.h"
#include "nrf_drv_gpiote.h"
#include "nrf_drv_clock.h"
#include "nrf_delay.h"
#include "app_pwm.h"
#include "nrf_drv_timer.h"
#include "nrf_drv_saadc.h"



#define DEAD_BEEF                       0xDEADBEEF          //!< Value used as error code on stack dump, can be used to identify stack location on stack unwind.
#define NON_CONNECTABLE_ADV_LED_PIN     BSP_BOARD_LED_0     //!< Toggles when non-connectable advertisement is sent.
#define CONNECTED_LED_PIN               BSP_BOARD_LED_1     //!< Is on when device has connected.
#define CONNECTABLE_ADV_LED_PIN         BSP_BOARD_LED_2     //!< Is on when device is advertising connectable advertisements.

/**@brief   Priority of the application BLE event handler.
 * @note    You shouldn't need to modify this value.
 */
#define APP_BLE_OBSERVER_PRIO 3

/**************************************************
new code start
***************************************************/

volatile STATE_MACHINE stateMachine;	// State machine
volatile BATTERY_STATE batteryState;     // Battery state

APP_TIMER_DEF(twoSecTimer_id);     /**< Handler for 2 second button press application timer */
APP_TIMER_DEF(tenMinTimer_id);     /**< Handler for 10 minutes shutdown application timer */
APP_TIMER_DEF(oneSecTimer_id);     /**< Handler for 1 second to calculate motor cycles */
APP_TIMER_DEF(longPressTimer_id);  /**< Handler for long button press */

const nrf_drv_timer_t TIMER_2 = NRF_DRV_TIMER_INSTANCE(2);  // Timer to calculate motor PWM frequency

bool motorPWM_NotToggling = false;  // Flag signaling if any motor problem

volatile uint32_t LowToHighCount = 0;    // Counting motor PWM frequency
volatile uint16_t pulseCount = 0;        // Counting number of pulses
volatile uint16_t batteryVoltage = 0;    // Battery milivolt

volatile bool Motor_error = false;        // Flag indicating state of motor PWM output

volatile bool handPieceConnected = true;  // Flag indicating if hand piece connected
volatile bool powerOn = false;            // Flag indicating if power stable
volatile bool longPress = false;          // Flag if button pressed long time

#define DEFAULT_DUTY  65
volatile uint16_t duty = DEFAULT_DUTY;
volatile uint16_t motorFreq = 0;         // Holds the motor frequency in Hz
#define MOTOR_FREQUENCY 325     // Target motor frequency, gear of 3.... and 5000 RFP target

#define FREQ_US 100             // 1000us = 1ms
#define ONE_SEC 1000
#define TWO_SEC 2000
#define TEN_MIN 600000
#define LONG_PRESS  2000        // Timeout for long button pressing

#define nHANDPIECE              // If exist switch to identify handpiece presence 
#define BREADBOARD              // Pinout for Laird EVB
#define BLE                    // With or Without BLE transmission

#ifdef BREADBOARD
#define BUTTON_PRESSED 0UL   // Button pressed
#else
#define BUTTON_PRESSED 1UL   // Button pressed
#endif

#define nMAXON               // Use of Maxon motor driver



APP_PWM_INSTANCE(m_pwm, 1);   // PWM for motor driver, using timer1

/**************************************************
new code end
***************************************************/


NRF_BLE_GATT_DEF(m_gatt); //!< GATT module instance.

/**@brief Callback function for asserts in the SoftDevice.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in]   line_num   Line number of the failing ASSERT call.
 * @param[in]   file_name  File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}


/**@brief Function for handling BLE events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 * @param[in]   p_context   Unused.
 */
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
    ret_code_t err_code;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            // Pairing not supported
            err_code = sd_ble_gap_sec_params_reply(p_ble_evt->evt.common_evt.conn_handle,
                                                   BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP,
                                                   NULL,
                                                   NULL);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            // No system attributes have been stored.
            err_code = sd_ble_gatts_sys_attr_set(p_ble_evt->evt.common_evt.conn_handle, NULL, 0, 0);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GAP_EVT_CONNECTED:
            bsp_board_led_on(CONNECTED_LED_PIN);
            bsp_board_led_off(CONNECTABLE_ADV_LED_PIN);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            // LED indication will be changed when advertising starts.
            break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        {
            ble_gap_phys_t const phys =
            {
                .rx_phys = BLE_GAP_PHY_AUTO,
                .tx_phys = BLE_GAP_PHY_AUTO,
            };
            err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            APP_ERROR_CHECK(err_code);
        } break;

        default:
            // No implementation needed.
            break;
    }
}


/**@brief Function for the GAP initialization.
*
* @details This function will set up all the necessary GAP (Generic Access Profile) parameters of
*          the device. It also sets the permissions and appearance.
*/
static void gap_params_init(void)
{
   ret_code_t              err_code;
   ble_gap_conn_params_t   gap_conn_params;
   ble_gap_conn_sec_mode_t sec_mode;
   uint8_t                 device_name[] = APP_DEVICE_NAME;

   BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

   err_code = sd_ble_gap_device_name_set(&sec_mode,
                                         device_name,
                                         strlen((const char *)device_name));
   APP_ERROR_CHECK(err_code);

   memset(&gap_conn_params, 0, sizeof(gap_conn_params));

   gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
   gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
   gap_conn_params.slave_latency     = SLAVE_LATENCY;
   gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

   err_code = sd_ble_gap_ppcp_set(&gap_conn_params);

   APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the GATT module.
 */
static void gatt_init(void)
{
    ret_code_t err_code = nrf_ble_gatt_init(&m_gatt, NULL);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    ret_code_t err_code;

    err_code = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err_code);

    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_app_ram_start_get(&ram_start);
    APP_ERROR_CHECK(err_code);

    // Overwrite some of the default configurations for the BLE stack.
    ble_cfg_t ble_cfg;

    // Configure the maximum number of connections.
    memset(&ble_cfg, 0, sizeof(ble_cfg));
    ble_cfg.gap_cfg.role_count_cfg.periph_role_count  = 1;
#if !defined (S112)
    ble_cfg.gap_cfg.role_count_cfg.central_role_count = 0;
    ble_cfg.gap_cfg.role_count_cfg.central_sec_count  = 0;
#endif // !defined (S112)
    err_code = sd_ble_cfg_set(BLE_GAP_CFG_ROLE_COUNT, &ble_cfg, ram_start);
    APP_ERROR_CHECK(err_code);

    // Enable BLE stack.
    err_code = nrf_sdh_ble_enable(&ram_start);
    APP_ERROR_CHECK(err_code);

    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
}


/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void)
{
    ret_code_t             err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);

}


/**@brief Function for initializing power management.
 */
static void power_management_init(void)
{
    ret_code_t err_code;
    err_code = nrf_pwr_mgmt_init();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling the idle state (main loop).
 *
 * @details If there is no pending log operation, then sleep until next the next event occurs.
 */
static void idle_state_handle(void)
{
    app_sched_execute();
    nrf_pwr_mgmt_run();
}


/**@brief Function for handling Eddystone events.
 *
 * @param[in] evt Eddystone event to handle.
 */
static void on_es_evt(nrf_ble_es_evt_t evt)
{
    switch (evt)
    {
        case NRF_BLE_ES_EVT_ADVERTISEMENT_SENT:
            bsp_board_led_invert(NON_CONNECTABLE_ADV_LED_PIN);
            break;

        case NRF_BLE_ES_EVT_CONNECTABLE_ADV_STARTED:
            bsp_board_led_on(CONNECTABLE_ADV_LED_PIN);
            break;

        case NRF_BLE_ES_EVT_CONNECTABLE_ADV_STOPPED:
            bsp_board_led_off(CONNECTABLE_ADV_LED_PIN);
            break;

        default:
            break;
    }
}


/**@brief Function for handling button events from app_button IRQ
 *
 * @param[in] pin_no        Pin of the button for which an event has occured
 * @param[in] button_action Press or Release
 */
static void button_evt_handler(uint8_t pin_no, uint8_t button_action)
{
    if (button_action == APP_BUTTON_PUSH && pin_no == BUTTON_1)
    {
        nrf_ble_es_on_start_connectable_advertising();
    }
}


/**
 * @brief Function for initializing the registation button
 *
 * @retval Values returned by @ref app_button_init
 * @retval Values returned by @ref app_button_enable
 */
static void button_init(void)
{
    ret_code_t              err_code;
    const uint8_t           buttons_cnt  = 1;
    static app_button_cfg_t buttons_cfgs =
    {
        .pin_no         = BUTTON_REGISTRATION,
        .active_state   = APP_BUTTON_ACTIVE_LOW,
        .pull_cfg       = NRF_GPIO_PIN_PULLUP,
        .button_handler = button_evt_handler
    };

    err_code = app_button_init(&buttons_cfgs, buttons_cnt, APP_TIMER_TICKS(100));
    APP_ERROR_CHECK(err_code);

    err_code = app_button_enable();
    APP_ERROR_CHECK(err_code);
}

static void leds_init(void)
{
    ret_code_t err_code = bsp_init(BSP_INIT_LEDS, NULL);
    APP_ERROR_CHECK(err_code);
}


static void scheduler_init(void)
{
    APP_SCHED_INIT(SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
}

/****************************************
new code start
*****************************************/

/**@brief Function for initializing logging. */
static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}

void saadc_callback_handler(nrf_drv_saadc_evt_t const * p_event)
{
  // Empty handler
}

void saadc_init(void)
{
  ret_code_t err_code;
  nrf_drv_saadc_uninit(); // SAADC is initialized in the softdevice, and we now change the channel.

  nrf_saadc_channel_config_t channel_config = NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_AIN1); // Signel ended

  err_code = nrf_drv_saadc_init(NULL, saadc_callback_handler);
  APP_ERROR_CHECK(err_code);

  err_code = nrfx_saadc_channel_init(1, &channel_config);
  APP_ERROR_CHECK(err_code);

}

void BatteryTest()
{
  nrf_saadc_value_t adc_val;
  nrfx_saadc_sample_convert(0, &adc_val);
  
  //NRF_LOG_INFO(" Volts: " NRF_LOG_FLOAT_MARKER "\r\n", NRF_LOG_FLOAT((adc_val * 3600) / 1024));

  batteryVoltage = ((adc_val * 3600) / 1024);

  if (batteryVoltage < 500)
  {
    batteryState = BATTERY_LOW;
    SET_RED_ON;
  }
  else if ((batteryVoltage > 500) && (batteryVoltage < 1000))
  {
    batteryState = BATTERY_OK;
    SET_YELLOW_ON;
  }
  else
  {
    batteryState = BATTERY_OK;
    SET_GREEN_ON;
  }
}

/*
Start highfrequency timer - timeout in u seconds
*/
void start_HF_timer(uint32_t timeout)
{
    uint32_t time_ticks;    // Convertion from ms to peripheral clock ticks
    
    if (nrf_drv_timer_is_enabled(&TIMER_2) == false){
      motorPWM_NotToggling = false;     // flag signaling timeout elapsed
      time_ticks = nrf_drv_timer_us_to_ticks(&TIMER_2, timeout);

      nrf_drv_timer_extended_compare(
           &TIMER_2, NRF_TIMER_CC_CHANNEL0, time_ticks, NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK, true);
      
      nrf_drv_timer_enable(&TIMER_2);
      }
}

void MotorFreqHandler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
  if (action == GPIOTE_CONFIG_POLARITY_LoToHi)
  {
    pulseCount++;
    if (pulseCount == /*1*/ 3)   // 3 is done in order to slow the control update to ~10ms (according to HW posibilities)
                                  // This function execute every ~3ms (330hz motor speed)
    {
      pulseCount = 0;
      if(LowToHighCount > 0)
      {
        motorFreq = (uint16_t)(/*10000*/ 30000 / LowToHighCount);    // Number of HF timer between two motor pulses (HF is 100us)
                                                                      // Value of 30000 becasue of the 3 pulse count to slow control loop
        if ( motorFreq < (MOTOR_FREQUENCY - 2))
        {
          if (duty < 100)
            duty++;
        }
        else if ( motorFreq > (MOTOR_FREQUENCY + 2))
        {
          if (duty > 0)
            duty--;
        }
        app_pwm_channel_duty_set(&m_pwm, 0, duty);
      }
      LowToHighCount = 0;
    }
  }
}

#if HANDPIECE
void HandPieceHandler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
  if ( action == GPIOTE_CONFIG_POLARITY_LoToHi )
  {
    handPieceConnected = false;
    stateMachine = MOVE_TO_IDEL;
  }
  else
  {
    handPieceConnected = true;
  }
}
#endif

void UserButtonHandler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
  volatile uint32_t pinValue = nrf_gpio_pin_read(USER_BUTTON);
  if (pinValue == BUTTON_PRESSED)   // Button pressed
  {
    if (stateMachine == ACTIVE)
    {
      stateMachine = MOVE_TO_IDEL;
    }
    else if ((stateMachine == IDEL) && (batteryState == BATTERY_OK) && handPieceConnected == true)
    {
      app_timer_start(longPressTimer_id, APP_TIMER_TICKS(LONG_PRESS), NULL);
      stateMachine = MOVE_TO_ACTIVE;
    }
  }
  else              // Button release
  {
    app_timer_stop(longPressTimer_id);  // Stop timer of long press checking
    if (longPress == true)
    {
      longPress = false;
      stateMachine = MOVE_TO_IDEL;
    }
  }
}

static void twoSecTimerHandler(void * p_context)
{
  app_timer_stop(twoSecTimer_id);
  TOGGLE_HOLD_POWER;  // Toggle state of pin p0.02, for hold/release power
  powerOn = ((powerOn == false) ? true : false);  // Toggle flag state, for main loop running
}

static void tenMinTimerHandler(void)
{
  TOGGLE_HOLD_POWER;  // Toggle state of pin p0.02, for hold/release power
  powerOn = ((powerOn == false) ? true : false);  // Toggle flag state, for main loop running
}

static void oneSecTimerHandler(void)
{
  if (stateMachine == ACTIVE)
  {
    TOGGLE_BLUE;
  }
  NRF_LOG_INFO("Motor frequency is: %d, Duty is: %d", motorFreq, duty);
}

static void longPressTimerHandler(void)
{
  longPress = true;
}

void Init_GPIO()
{
    nrf_drv_gpiote_init();

    nrf_gpio_cfg_output(HOLD_POWER);
    nrf_gpio_cfg_output(BOOST_EN);
    nrf_gpio_cfg_output(BLUE);
    nrf_gpio_cfg_output(RED);
    nrf_gpio_cfg_output(GREEN);

    nrf_drv_gpiote_in_config_t in_config1 = GPIOTE_CONFIG_IN_SENSE_LOTOHI(true);
    //in_config1.pull = NRF_GPIO_PIN_PULLUP;
    nrf_drv_gpiote_in_init(MOTOR_FREQ_PIN, &in_config1, MotorFreqHandler);
#if HANDPIECE
    nrf_drv_gpiote_in_config_t in_config2 = GPIOTE_CONFIG_IN_SENSE_TOGGLE(true);
    in_config2.pull = NRF_GPIO_PIN_PULLUP;
    nrf_drv_gpiote_in_init(HANDPIECE_PIN, &in_config2, HandPieceHandler);
#endif // HANDPIECE

    nrf_drv_gpiote_in_config_t in_config3 = GPIOTE_CONFIG_IN_SENSE_TOGGLE(true);
    in_config3.pull = NRF_GPIO_PIN_PULLUP;
    nrf_drv_gpiote_in_init(USER_BUTTON, &in_config3, UserButtonHandler);
}

/*
Event handler for highfrequency timer
*/
void HF_timer_event_handler(nrf_timer_event_t event_type, void* p_context)
{
  // Count HF pulses for motor speed calculation
  LowToHighCount++;
}

/*
Stop highfrequency timer
*/
void stop_HF_timer(void)
{
  nrf_drv_timer_disable(&TIMER_2);
}

static void timers_init(void)
{
    ret_code_t err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);

    // Init and config high frequency timer to verify motor PWM toggling
    nrf_drv_timer_config_t timer_cfg = NRF_DRV_TIMER_DEFAULT_CONFIG;
    nrf_drv_timer_init(&TIMER_2, &timer_cfg, HF_timer_event_handler);
  
    // Create 3 application timers:
    // - 2 seconds timer to verify 'long press'
    // - 10 minutes timer to verify 10 minutes in IDEL
    // - 1 second timer to print duty cycle and motor PWM
    // - LongPress timer to start and hold motor
    //err_code = app_timer_create(&twoSecTimer_id, APP_TIMER_MODE_SINGLE_SHOT, twoSecTimerHandler);
    err_code = app_timer_create(&twoSecTimer_id, APP_TIMER_MODE_REPEATED, twoSecTimerHandler);
    APP_ERROR_CHECK(err_code);
    err_code = app_timer_create(&tenMinTimer_id, APP_TIMER_MODE_SINGLE_SHOT, tenMinTimerHandler);
    APP_ERROR_CHECK(err_code);
    err_code = app_timer_create(&oneSecTimer_id, APP_TIMER_MODE_REPEATED, oneSecTimerHandler);
    APP_ERROR_CHECK(err_code);
    err_code = app_timer_create(&longPressTimer_id, APP_TIMER_MODE_SINGLE_SHOT, longPressTimerHandler);
    APP_ERROR_CHECK(err_code);
  
}

/****************************************
new code end
*****************************************/

/**
 * @brief Function for application main entry.
 */
int main(void)

{
    ret_code_t err_code;

    // Initialize.
    log_init();
    timers_init();
#ifndef BLE
    err_code = nrf_drv_clock_init();
    APP_ERROR_CHECK(err_code);
    nrf_drv_clock_lfclk_request(NULL);
#endif
    leds_init();
    button_init();
#ifdef BLE
    scheduler_init();
    power_management_init();
    ble_stack_init();
    gap_params_init();
    gatt_init();
    conn_params_init();
    nrf_ble_es_init(on_es_evt);
#endif

    saadc_init();
// Init state machine
    stateMachine = MOVE_TO_IDEL;
  
    // Init input/output pins
    Init_GPIO();
    
    // Config PWM library
    app_pwm_config_t pwm_config = APP_PWM_DEFAULT_CONFIG_1CH(FREQ_US, PWM);
    pwm_config.pin_polarity[0] = APP_PWM_POLARITY_ACTIVE_HIGH;
    app_pwm_init(&m_pwm, &pwm_config, NULL);

    //app_pwm_enable(&m_pwm);
    //app_pwm_channel_duty_set(&m_pwm, 0, 0);   // PWM is started with dutycycle of 0

    // Start the 2 seconds timmer to verify long button press
    err_code = app_timer_start(twoSecTimer_id, APP_TIMER_TICKS(TWO_SEC), NULL);
    APP_ERROR_CHECK(err_code);
  
    // Wait for 2 second start up delay
    while(false == powerOn);  // DEBUG
    NRF_LOG_INFO("Power on after 2 seconds");

    // Enter main loop.
    for (;;)
    {
      BatteryTest();

      if (motorPWM_NotToggling == true)
      {
        NRF_LOG_INFO("Motor PWM constant");   // Message for debugging only once
        motorPWM_NotToggling = false;
      }

      switch(stateMachine)
      {
        case MOVE_TO_IDEL:
          
          CLEAR_BOOST_EN;
          nrf_delay_ms(50);
          // Disable PWM and set it to 0
          app_pwm_channel_duty_set(&m_pwm, 0, 0);
          app_pwm_disable(&m_pwm);
          duty = DEFAULT_DUTY;
          // Disable motor PWM output events
          nrf_drv_gpiote_in_event_disable(MOTOR_FREQ_PIN);
          // Start 10 minutes timer
          app_timer_start(tenMinTimer_id, APP_TIMER_TICKS(TEN_MIN), NULL);
          // Stop printing timer
          app_timer_stop(oneSecTimer_id);
          RESET_BLUE_ON;    // Reset BLUE led in case it left light
          stop_HF_timer();
          
          motorFreq = 1;    // Set to speed 0 for the last BLE transmission

          // es_adv_timing_stop();
          es_adv_stop();
          
          #ifdef MAXON
          nrf_gpio_pin_write(18, 0ul);
          #endif

          // Send motor speed 0 before stopping the BLE transmission
          NRF_LOG_INFO("Moving to IDEL state");

          nrf_drv_gpiote_in_event_enable(USER_BUTTON, true);
          stateMachine = IDEL;
        break;

        case IDEL:
          idle_state_handle();
        break;

        case MOVE_TO_ACTIVE:
          app_timer_stop(tenMinTimer_id);  // Stop the 10 minutes timer started in IDEL state
          SET_BOOST_EN;
          nrf_delay_ms(50);
          app_pwm_enable(&m_pwm);
          app_pwm_channel_duty_set(&m_pwm, 0, DEFAULT_DUTY);   // PWM is started with default dutycycle
          #ifdef MAXON
          nrf_gpio_pin_write(18, 1ul);
          #else
          // Enable events on motor PWM output
          nrf_drv_gpiote_in_event_enable(MOTOR_FREQ_PIN, true);
          #endif
          NRF_LOG_INFO("Moving to ACTIVE state");
          app_timer_start(oneSecTimer_id, APP_TIMER_TICKS(ONE_SEC), NULL);
          start_HF_timer(100);  // us - Timer use to calculate motor frequency
          
          es_adv_start_non_connctable_adv();
          //es_adv_timing_start();

          stateMachine = ACTIVE;
        break;

        case ACTIVE:
          // Insert motorFreq into Eddystone transmission
          // motorFreq 3 * 60 - Gear ratio and seconds in minute
          // Start PWM and control motor
        break;

        default:

        break;
      }
    }
}


/**
 * @}
 */
