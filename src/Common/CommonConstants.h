#ifndef COMMON_CONSTANTS_H_
#define COMMON_CONSTANTS_H_

/******************************************** INCLUDES ********************************************/
/* System includes */

/* Custom includes */


/******************************************* CONSTANTS ********************************************/
/* COMMUNICATIONS CONSTANTS */
const unsigned int  MESSAGE_PREAMBLE_UL    = 0xAABBCCDD;  /**< Message preamble to identify start of a new message */
const unsigned char NUM_CHECKSUM_BYTES_UC  = sizeof(int); /**< Number of bytes for the checksum                    */
const float         COMMS_PERIOD_MS_F      = 500.0f;      /**< Period for the communications loop                  */
const unsigned int  BAUD_RATE_UL           = 9600;        /**< Baud rate for serial communications                 */
const unsigned int  INPUT_BUFFER_LENGTH_UL = 1000;        /**< Length of the input buffer                          */ 

/* CONVERSION FACTORS */
const float MILLIS_TO_SECONDS_F = 1000.0f; /**< Converstion factor from milliseconds to seconds */


#endif /* COMMON_CONSTANTS_H_ */