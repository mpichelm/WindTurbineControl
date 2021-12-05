#ifndef LINEAR_ACTUATOR
#define LINEAR_ACTUATOR

/******************************************** INCLUDES ********************************************/
/* System includes */

/* Custom includes */


/* 
- NOTE1: the term "gear turn" used here does not exactly corresponds with the physical turn. It 
relates to te gear angle turned to reach the next magnet and get a positive reading from the Hall
sensor 
- NOTE2: Relays have inverse logic. If not signal is set, the are "normally open", which makes them 
consume more energy. We will adapt to this so they are not signaled when servo is stopped 
*/

/******************************************* CONSTANTS ********************************************/
const int  MAX_TURNS_ERROR_UL      = 2; 	/**< Max gear turns difference allowed between requested and actual servo position */
const long CALIBRATION_TIME_MS_ULL = 1500; /**< Calibration time, in milliseconds        

/********************************************* TYPES **********************************************/
/***********************************************************************************************//**
 * \enum ServoState_e
 * \brief Current state of the servo
 **************************************************************************************************/
enum ServoState_e : int16_t
{
    SERVOSTATE_STOPPED    = 0, /**< Servo is stopped    */
    SERVOSTATE_RETRACTING = 1, /**< Servo is retracting */
    SERVOSTATE_EXTENDING  = 2, /**< Servo is extending  */
}; 

/* Declare as volatile variables used in interrupts */
volatile static int          ulCurrentTurns_      = 0;                  /**< Current number of gear turns          */
volatile static long         ulLastInterruptTime_ = 0;                  /**< Time of the last interrupt activation */
volatile static ServoState_e eServoState_         = SERVOSTATE_STOPPED; /**< Current state of the servo servo   */
volatile static int 		 ulMaxTurns_          = 0; 				   /**< Number of turns to total extension    */

/********************************************* CLASS **********************************************/

/***********************************************************************************************//**
 * \class LinearServo_cl
 * \brief This class allows to use a linear servo as a servo
 **************************************************************************************************/
class LinearServo_cl 
{
public:
	/*******************************************************************************************//**
	* \brief Class constructor
	***********************************************************************************************/
	LinearServo_cl();

	/*******************************************************************************************//**
	* \brief Setup the class. Method to be called during the setup of Arduino
	* \param[in] ulRelayRetractionPin: Pin to activate the relay that retracts the servo
	* \param[in] ulRelayExtensionPin: Pin to activate the relay that extends the servo
	* \param[in] ulHallSensorPin: Pin for the hall sensor that detects gear turns
	* \param[in] fServoTotalLenght: Total extension length of the servo
	* \param[in] fServoUsedLength: Length of the servo that wants to be used. 100% extension will be
	* this value
	* \param[in] ulMaxTurns: Total number of gear turns to completely extend servo
	***********************************************************************************************/
	void vSetup(const int   ulRelayRetractionPin, 
				const int   ulRelayExtensionPin, 
				const int   ulHallSensorPin, 
				const float fServoTotalLenght, 
				const float fServoUsedLength, 
				const int   ulMaxTurns); 
	
	/*******************************************************************************************//**
	* \brief This functions needs to be invoked in every step of the main Arduino loop to check for
	* extensions or retractions
	***********************************************************************************************/
	void vOperate();

	/*******************************************************************************************//**
	* \brief This function extends the servo to a desired length
	* \param[in] fLengthMilimeter: Requested length, in millimeters
	***********************************************************************************************/
	void vSetExtensionLength(const float fLengthMillimeter);

	/*******************************************************************************************//**
	* \brief This function sets the extension of the servo in a desired percetage
	* \param[in] fPercent: Requested extension percentage
	***********************************************************************************************/
	void vSetExtensionPercentage(const float fPercent);

	/*******************************************************************************************//**
	* \brief This function performs the calibration of the servo. Not like the intial calibration. 
	* This retracts servo to 0% to ensure any error is removed
	***********************************************************************************************/
	void vCalibrate();

	/*******************************************************************************************//**
	* \brief This function gets the current extension percentage
	* \return Current extension percentage
	***********************************************************************************************/
	float fGetExtensionPercentage();

private:
	/*******************************************************************************************//**
	* \brief This function gets the current number of gear turns
	* \return Number of current gear turns
	***********************************************************************************************/
	int ulGetCurrentTurns();

	/*******************************************************************************************//**
	* \brief This function sets the current number of gear turns
	* \param[in] ulCurrentTurns: Number of gear turns to set
	***********************************************************************************************/
	void vSetCurrentTurns(const int ulCurrentTurns);

	/*******************************************************************************************//**
	* \brief This functions extends the servo
	***********************************************************************************************/
	void vExtendServo();

	/*******************************************************************************************//**
	* \brief This functions retracts the servo
	***********************************************************************************************/
	void vRetractServo();

	/*******************************************************************************************//**
	* \brief This functions stops the servo
	***********************************************************************************************/
	void vStopActuador();

	/***************************************** ATTRIBUTES *****************************************/
	int   ulRelayRetractionPin_;    /**< Relay retraction pin. Relay uses inverse logic, LOW means "normally open" is set */ 
	int   ulRelayExtensionPin_;     /**< Relay extension pin. Relay uses inverse logic, LOW means "normally open" is set  */
	int   ulHallSensorPin_;         /**< Pin of the hall sensor that reads gear turns                                     */
	float fServoTotalLenght_;       /**< Maximum physical extension length of the servo                                   */
	float fServoUsedLength_;        /**< Maximum length that the user wants to use actively                               */
	int   ulTargetTurns_; 		    /**< Objective turns count to reach desired position                                  */
	float fExtensionTurnRatio_;     /**< Extension of the actuator for every gear turn                                    */
	bool  bCalibrating_; 		    /**< Servo is calibrating                                                             */
	long  ullCalibrationStartTime_; /**< Calibration start time                                                           */
	float fRequestedLength_; 		/**< Extension requested by the user (millimeters)						              */
};

/***********************************************************************************************//**
* \brief Funtion triggered when an interruption happens. Increments/decrements gear turns count
***************************************************************************************************/
static void vReadHallSensor();

#endif // LINEAR_ACTUATOR

