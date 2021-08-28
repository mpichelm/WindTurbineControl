#ifndef TYPES_H_
#define TYPES_H_

/******************************************** INCLUDES ********************************************/
/* System includes */

/* Custom includes */


/********************************************** TYPES *********************************************/
/***********************************************************************************************//**
 * \enum UserMaster_e
 * \brief Indicates who is controlling the wind turbine
 **************************************************************************************************/
enum UserMaster_e
{
    USERMASTER_ARDUINO = 0, /**< The wind turbine is being controlled from the Arduino     */
    USERMASTER_ANDROID = 1, /**< The wind turbine is being controlled from the Android App */
}; 

#endif /* TYPES_H_ */