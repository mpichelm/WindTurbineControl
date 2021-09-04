#ifndef CONSTANTS_H_
#define CONSTANTS_H_

/******************************************** INCLUDES ********************************************/
/* System includes */

/* Custom includes */


/******************************************* CONSTANTS ********************************************/

/* ARDUINS PINS */
/* Arduino Mega pins that allow interrupts: 2, 3, 18, 19, 20, 21 */ 
const char HC12_MODE_PIN           = 44; /**< Arduino pin to select HC12 mode: LOW = AT commands, HIGH = transparent mode (we use this) */
const char ANEMOMETER_HALL_PIN     = 2;  /**< Digital pin for the anemomenter hall sensor                                               */
const char TACOMETER_HALL_PIN      = 21; /**< Digital pin for the tacometer (rotor rpm) hall sensor                                     */
const char DHT_22_PIN              = 40; /**< Digital pin for the DHT22 temperature/humidity sensor                                     */ 
const char ENABLE_BREAK_RELAY_PIN  = 32; /**< Digital pin to enable break                                                               */
const char DISABLE_BREAK_RELAY_PIN = 30; /**< Digital pin to disable break                                                              */
const char BLADE_RETRACTION_PIN    = 31; /**< Pin that controls the "servo" retraction for blade pitch control                          */
const char BLADE_EXTENSION_PIN     = 33; /**< Pin that controls the "servo" extension for blade pitch control                           */
const char SERVO_HALL_PIN          = 3;  /**< Pin used to convert the linear actuator in a controllable servo                           */

/* SENSORS CONSTANTS */
const unsigned int  HC12_INITIALIZATION_DELAY_MS_UL = 80; /**< Required time before HC12 initialization                        */
const unsigned long HALL_MIN_DELAY_MS_ULL           = 2;  /**< Minimum time (millis) to get a new reading and avoid "bouncing" */

/* COMMUNICATIONS CONSTANTS */
const float COMMS_PERIOD_MS = 500.0; /**< Period for the communications loop  */
const int BAUD_RATE         = 9600;  /**< Baud rate for serial communications */

/* TEMPERATURE/HUMIDITY SENSORS */
const float READ_PERIOD_MS = 10000.0; /**< Time interval between data measurements */

/* VARIABLE PITCH CONTROL */
const float SERVO_LENGHT_MM                                  = 200.0; /**< Maximum extension length for the servo responsible for the pitch control  */
const float SERVO_USABLE_LENGTH_MM                           = 50.0;  /**< Maximum extension required for the full blade pitch control               */
const short SERVO_TURNS_TO_FULL_EXTENSION                    = 104;   /**< Number of gear turns to fully extend the actuator                         */
const short PITCH_ANGLE_CALIBRATION_POINTS_UC                = 5;     /**< Number of points for the calibration curve                                */
float PITCH_CONTROL_BETA_ANGLE_F[PITCH_ANGLE_CALIBRATION_POINTS_UC] = /**< Aerodinamic angle beta at which the actuator extension is calibrated      */
							{0.0f, 0.4f, 0.8f, 1.2f, 1.5708f};
float PITCH_CONTROL_EXTENSION_F[PITCH_ANGLE_CALIBRATION_POINTS_UC]  = /**< Extension of the actuator to achieve angles in PITCH_CONTROL_BETA_ANGLE_F */						
							{0.0f, 25.0f , 50.0f,  75.0f, 100.0f };

/* TURBINE BREAK */
const float TIME_BREAK_ACT_OP_EXTENSION_MS = 1000;                                 /**< Milliseconds to operational extension of the break actuator                                                                   */
const float BREAK_RETRACTION_TIME_MS       = TIME_BREAK_ACT_OP_EXTENSION_MS + 500; /**< Time for the full retraction of the break actuator (extra time is added to the full extension time to ensure full retraction) */ 
const float BREAK_FULL_RETRACTION_TIME_MS  = 10000;                                /**< Time for the full retraction of the break actuator (extra time is added to the full extension time to ensure full retraction) */ 
const float BREAK_MIN_ENABLED_TIME_MS      = 5000;                                 /**< Min time for the break to be active once triggered                                                                            */

/* TACOMETER */
const unsigned char TACOMETER_NUM_MAGNETS  = 3; /**< Number of magnets in that hall sensor reads in a complete turn for the tacometer  */

/* WIND MEASUREMENTS CONSTANTS */
const unsigned int NUM_AVERAGE_WIND_SPEED_SAMPLES_UL                    = 60;   /**< Number of samples to compute the average wind speed                                   */
const unsigned int WIND_SPEED_SAMPLE_INTERVAL_MS_UL                     = 1000; /**< Interval between wind speed samples                                                   */
const unsigned char ANEMOMETER_NUM_MAGNETS                              = 3;    /**< Number of magnets in that hall sensor reads in a complete turn for the anemometer     */
const unsigned char ANEM_CALIBRATION_POINTS_UC                          = 5;    /**< Number of points for the calibration curve                                            */
float ANEM_CALIBRATION_ANGULAR_SPEED_DATA_F[ANEM_CALIBRATION_POINTS_UC] =       /**< Angular speed (rad/s) of the anemometer at which equivalence with wind speed is known */
							{0.0f, 50.0f, 100.0f, 150.0f, 200.0f};
float ANEM_CALIBRATION_WIND_DATA_F[ANEM_CALIBRATION_POINTS_UC]          =       /**< Wind speeds for each point in ANEM_CALIBRATION_ANGULAR_SPEED_DATA_F                   */						
							{0.0f, 5.0f , 10.0f,  15.0f,  20.0f };

#endif // CONSTANTS_H_