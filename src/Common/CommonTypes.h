#ifndef COMMON_TYPES_H_
#define COMMON_TYPES_H_

/******************************************** INCLUDES ********************************************/
/* System includes */

/* Custom includes */


/********************************************** TYPES *********************************************/
/***********************************************************************************************//**
 * \enum BreakStatus_e
 * \brief Enum to indicate break status
 **************************************************************************************************/
enum BreakStatus_e
{
    BREAK_DISABLED  = 0, /**< Break is enabled                              */
    BREAK_ENABLED   = 1, /**< Break is disabled                             */
    BREAK_BREAKING  = 2, /**< Break is transitioning to a break position    */
    BREAK_RELEASING = 3, /**< Break is transitioning to a disabled position */
}; 

/***********************************************************************************************//**
 * \enum PitchMode_e
 * \brief Enum to indicate variable pitch mode
 **************************************************************************************************/
enum PitchMode_e
{ 
    PITCHMODE_MANUAL  = 0, /**< Blade pitch angle control is manual    */
    PITCHMODE_AUTO    = 1, /**< Blade pitch angle control is automatic */
}; 

/***********************************************************************************************//**
 * \struct AeroStatus_st
 * \brief This structure contains all data related to the wind turbine status
 **************************************************************************************************/
struct AeroStatus_st
{
    BreakStatus_e eBreakStatus; /**< Rotor break status indicator */
    PitchMode_e   ePitchMode;   /**< Pitch control mode           */
}; 

/***********************************************************************************************//**
 * \struct ControlParams_st
 * \brief Inputs directly set by the user
 **************************************************************************************************/
struct ControlParams_st
{
    float       fMaxRotorSpeedRPM;     /**< User defined limit for the rotor speed before automatic break */
    float       fMaxWindSpeed;         /**< User defined limit for the wind speed before automatic break  */
    float       fBladePitchPercentage; /**< Blade deflection percentage (only valid in manual mode)       */
    bool        bManualBreak;          /**< Manual break flag                                             */
    PitchMode_e ePitchMode;            /**< Pitch control mode                                            */
}; 

/***********************************************************************************************//**
 * \struct AeroData_st
 * \brief This structure contains all data collected from the wind turbine operation
 **************************************************************************************************/
struct AeroData_st
{
    float         fTempCelsius;          /**< Current ambient temperature [ºCelsius] */
    float         fRelHumidity;          /**< Relative humidity [%]                  */
    float         fWindSpeed;            /**< Current wind speed [m/s]               */
    float         fAverageWindSpeed;     /**< Current wind speed [m/s]               */
    float         fRotorSpeedRPM;        /**< Rotor angular speed [rpm]              */
    float         fBladePitchPercentage; /**< Blade deflection percentage            */
    AeroStatus_st stStatus;              /**< Status data of the turbine operation   */
}; 

/***********************************************************************************************//**
 * \enum MessageID_e
 * \brief Message identificators
 **************************************************************************************************/
enum MessageID_e
{
    MESSAGEID_AERODATA      = 0, /**< Message from the Control Arduino to the User Arduino */
    MESSAGEID_CONTROLPARAMS = 1, /**< Message from the User Arduino to the Control Arduino */
    MESSAGEID_COUNT         = 2, /**< Number of different messages                         */
}; 

/***********************************************************************************************//**
 * \struct MsgHeader_st
 * \brief Structure containing data in the message header
 **************************************************************************************************/
struct MsgHeader_st
{
    int         ulPreable; /**< Preamble data that identifies the start of a new message */
    MessageID_e eId;       /**< Message ID to know how to deserialize the message        */
    int         ulLength;  /**< Total message length (including header)                  */
}; 


#endif /* COMMON_TYPES_H_ */