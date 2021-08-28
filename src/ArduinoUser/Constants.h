#ifndef CONSTANTS_H_
#define CONSTANTS_H_

/******************************************** INCLUDES ********************************************/
/* System includes */

/* Custom includes */
#include "Types.h"


/******************************************* CONSTANTS ********************************************/

/* ARDUINS PINS */
/* Arduino Mega pins that allow interrupts: 2, 3, 18, 19, 20, 21 */ 
const int MAX_RPM_POT_PIN_UL          = 0;  /**< Analogic input pin for the potentiometer that controls max RPM                                   */
const int MAX_WIND_POT_UL             = 1;  /**< Analogic input pin for the max wind speed                                                        */ 
const int MANUAL_BREAK_PIN_UL         = 24; /**< Digital input pin for the manual break switch                                                    */
const int BREAK_LED_GREEN_PIN_UL      = 3;  /**< Digital output pin for the LED indicating break disabled                                         */
const int BREAK_LED_BLUE_PIN_UL       = 4;  /**< Digital output pin for the LED indicating break is moving                                        */
const int BREAK_LED_RED_PIN_UL        = 2;  /**< Digital output pin for the LED indicating break is enabled                                       */
const int PITCH_CONTROL_SWITCH_PIN_UL = 26; /**< Digital input pin for the switch that controls variable step mode                                */
const int HC12_MODE_PIN_UL            = 22; /**< Digital output pin to select HC12 mode (LOW = AT commands, HIGH = transparent mode (we use this) */
const int SLIDER_PITCH_PIN_UL         = 2;  /**< Analogic input pin for the linear potentiometer that controls pitch angle                        */

/* MAXIMUM BREAK LIMITS  */
const float MAX_RPM_F  = 300.0f; /**< Max RPM that can be configured with the potentiometer        */ 
const float MAX_WIND_F = 30.0f;  /**< Max wind speed that can be configured with the potentiometer */ 

/* GENERIC CONSTANTS */
const int          LCD_REFRESH_TIME_MS_UL    = 500;                /**< Refresh time for the LCD screen (milliseconds)                            */
const int          HC12_SEND_PERIOD_MS_UL    = 500;                /**< Time period between sending messages through HC12                         */
const int          ESP8266_SEND_PERIOD_MS_UL = 500;                /**< Time period between sending messages through ESP8266                      */
const UserMaster_e DEFAULT_USER_MASTER_E     = USERMASTER_ARDUINO; /**< Default control master                                                    */
const int          ANDROID_TIMEOUT_MS_UL     = 30000;              /**< Timeout to transition from Android to Arduino if no messages are received */
const int          COMMS_BAUD_RATE_UL        = 9600;               /**< Baud rate for serial communications                                       */

#endif // CONSTANTS_H_