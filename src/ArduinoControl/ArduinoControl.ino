/******************************************** INCLUDES ********************************************/
/* System includes */
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <ActuadorLineal.h>
#include <Metro.h>
#include "CommonConstants.h"
#include "CommonTypes.h"
#include "CommsManager.h"

/* Custom includes */
#include "Constants.h"


/******************************************** GLOBALS *********************************************/
/* Communications variables */
const unsigned int MAX_MSG_SIZE_UL_ = sizeof(ControlParams_st) > sizeof(AeroData_st) ? 
									 sizeof(ControlParams_st) : sizeof(AeroData_st);
unsigned char aucReadingBuf_[MAX_MSG_SIZE_UL_];
CommsManager_cl clCommsManager_;

/* Sensors variables */
ActuadorLineal   clPitchControlServo_;  			  /**< Servo to control blade pitch angle                    */
DHT 		     clTempHRSensor_(DHT_22_PIN, DHT22);  /**< Temperature/Humidity sensor class                     */
Metro 		     clReadDHT22Timer_(READ_PERIOD_MS);   /**< Class to control perdic temperature/humidity readings */
Metro 		     clSenderHC12Timer_(COMMS_PERIOD_MS); /**< Class to control periodic message sends               */
AeroData_st      stAeroData_ = {};					  /**< Current data 										 */
ControlParams_st stControlParams_ = {};	    	      /**< Control requests by the user 	     				 */

/* Wind speed variables */
Metro clWindSampleTimer_(WIND_SPEED_SAMPLE_INTERVAL_MS_UL); /**< Class to control perdic wind speed readings                                                            */
float afWindSamples_[NUM_AVERAGE_WIND_SPEED_SAMPLES_UL];    /**< Buffer to store samples and compute average wind speed                                                 */
unsigned int ulWindSamplesIdx_ = 0;						    /**< Index of the last position where data was stored                                                       */
bool bWindBufferFull           = false; 				    /**< Variable used to check if the buffer has been filled with data, and new data overwrittes oldest values */

/* Break axiliary variables */
unsigned long ullBreakLastRequestTimeMs_    = 0; /**< Output of millis() function when the break activation was requested for the last time */
unsigned long ullBreakLastActivationTimeMs_ = 0; /**< Output of millis() function when the break activation was requested for the last time */
unsigned long ullBreakManeouverStartTimeMs_ = 0; /**< Start time for a retraction/extension of the break actuator */

/* Anemometer/tacometer auxiliary variables */
unsigned long ullAnemometerLastTimeMs = 0;
unsigned long ullTacometerLastTimeMs = 0;


/****************************************** FUNCTION *******************************************//**
* \brief Setup function for the Arduino board
***************************************************************************************************/
void setup() 
{
	/* Serial port initialization */
	delay(HC12_INITIALIZATION_DELAY_MS_UL); /* Initial delay necessary for the HC12 module */
	Serial.begin(BAUD_RATE); /* Initialization of serial port with the PC */
	Serial1.begin(BAUD_RATE); /* Initialization of serial port with the HC12 module */

	/* Set input/output pins */
	pinMode(HC12_MODE_PIN, OUTPUT); 
	pinMode(ANEMOMETER_HALL_PIN, INPUT); 
	pinMode(TACOMETER_HALL_PIN, INPUT); 
	pinMode(ENABLE_BREAK_RELAY_PIN, OUTPUT);
	pinMode(DISABLE_BREAK_RELAY_PIN, OUTPUT);
	pinMode(DHT_22_PIN, INPUT);

	/* Initialization of the pitch control servo */
	clPitchControlServo_ = ActuadorLineal(BLADE_RETRACTION_PIN, BLADE_EXTENSION_PIN, SERVO_HALL_PIN,
	 						 SERVO_LENGHT_MM, SERVO_USABLE_LENGTH_MM, SERVO_TURNS_TO_FULL_EXTENSION); 

	/* Initialization of the DHT22 Initialization of the DHT22 sensor (temperature and humidity) */
	clTempHRSensor_.begin();

	/* Pin state initialization */
	digitalWrite(ENABLE_BREAK_RELAY_PIN, HIGH);
	digitalWrite(DISABLE_BREAK_RELAY_PIN, HIGH);
	digitalWrite(HC12_MODE_PIN, HIGH);

	/* Make sure break actuactor is fully retracted */
	digitalWrite(ENABLE_BREAK_RELAY_PIN, LOW);
	delay(BREAK_FULL_RETRACTION_TIME_MS);
	digitalWrite(ENABLE_BREAK_RELAY_PIN, HIGH);

	/* Anemometer setup */
	attachInterrupt(digitalPinToInterrupt(ANEMOMETER_HALL_PIN), vReadAnemometerHallSensor, RISING);	

	/* Tacometer setup */
	attachInterrupt(digitalPinToInterrupt(TACOMETER_HALL_PIN), vReadTacometerHallSensor, RISING);
}

/****************************************** FUNCTION *******************************************//**
* \brief Loop function for the Arduino board
***************************************************************************************************/
void loop() 
{
	/* Read incoming data from the used Arduino */
	vReadDataHC12();

	/* Read temperature and humidity */
	vReadDHT22Sensor();

	/* Read current wind speed and compute average wind speed */
	stAeroData_.fWindSpeed = 5; // TODO: remove dummy value
	vAverageWindSpeed();

	/* Read rotor speed */
	stAeroData_.fRotorSpeedRPM = 15; // TODO: remove dummy value

	/* Break control */ 
	breakManagement(); /* Check for new necessary operations */
	vFinishBreakManoeuver(); /* Finish active operations, if necessary */

	/* Pitch control */
	vBladePitchControl();

	/* Send current turbine data to the user Arduino */
	vSendDataHC12();
}


/****************************************** FUNCTION *******************************************//**
* \brief Method that sends data from the Windturbine to the user arduino
***************************************************************************************************/
void vSendDataHC12() 
{
	// TODO: break status used to be updated here

	// Serial.println("-------");
	// Serial.println("Average wind = " + (String)stAeroData_.fAverageWindSpeed);
	// Serial.println("Blade percent = " + (String)stAeroData_.fBladePitchPercentage);
	// Serial.println("Rel humidity = " + (String)stAeroData_.fRelHumidity);
	// Serial.println("Rotor speed = " + (String)stAeroData_.fRotorSpeedRPM);
	// Serial.println("Temp celsius = " + (String)stAeroData_.fTempCelsius);
	// Serial.println("Wind speed = " + (String)stAeroData_.fWindSpeed);
	// Serial.println("Break status = " + (String)stAeroData_.stStatus.eBreakStatus);
	// Serial.println("Pitch mode = " + (String)stAeroData_.stStatus.ePitchMode);
	// Serial.println("-------");

	/* Check if it is time to send new data */
	if (clSenderHC12Timer_.check()) 
	{
		clCommsManager_.vSendMessage(stAeroData_, MESSAGEID_AERODATA, Serial1);
	}
}

/****************************************** FUNCTION *******************************************//**
* \brief Method that receives data from the user adruino
***************************************************************************************************/
void vReadDataHC12() 
{
	/* Create a variable for the message length and other one for the message ID */
	unsigned int ulMsgLength = 0;
	MessageID_e eMsgID = MESSAGEID_COUNT;

	/* Check if it is time to send new data */
	while (clCommsManager_.bReadInputMessage(Serial1, aucReadingBuf_, ulMsgLength, eMsgID))
	{
		Serial.println("Recibido: " + (String)ulMsgLength + "  ID: " + (String)eMsgID);
		/* Copy the buffer to the message */
		if ((eMsgID == MESSAGEID_CONTROLPARAMS) && (ulMsgLength == sizeof(ControlParams_st)))
		{
			memcpy(&stControlParams_, aucReadingBuf_, ulMsgLength);

			Serial.println("-------");
			Serial.println("Manual break = " + (String)stControlParams_.bManualBreak);
			Serial.println("Pitch mode = " + (String)stControlParams_.ePitchMode);
			Serial.println("pitch percent = " + (String)stControlParams_.fBladePitchPercentage);
			Serial.println("max rotor speed = " + (String)stControlParams_.fMaxRotorSpeedRPM);
			Serial.println("max wind speed = " + (String)stControlParams_.fMaxWindSpeed);
			Serial.println("-------");
		}
	}
}

/****************************************** FUNCTION *******************************************//**
* \brief Method that reads temperature and humidity from DHT22 sensor
***************************************************************************************************/
void vReadDHT22Sensor() {

	/* Read temperature and humidity */
	if (clReadDHT22Timer_.check()) 
	{
		stAeroData_.fTempCelsius = clTempHRSensor_.readTemperature();
		stAeroData_.fRelHumidity = clTempHRSensor_.readHumidity();
	}
}

/****************************************** FUNCTION *******************************************//**
* \brief Method that computes average wind speed
***************************************************************************************************/
void vAverageWindSpeed() 
{
	/* Check if it is time to take a new sample for the wind speed moving average */
	if (clWindSampleTimer_.check()) 
	{
		/* Store current wind speed */
		afWindSamples_[ulWindSamplesIdx_] = stAeroData_.fWindSpeed;

		/* Increment index for the next data save */
		if (++ulWindSamplesIdx_ >= NUM_AVERAGE_WIND_SPEED_SAMPLES_UL)
		{
			bWindBufferFull = true;
			ulWindSamplesIdx_ = 0;
		}
	
		/* Compute average */
		unsigned int ulNumSamples = bWindBufferFull ? 
				NUM_AVERAGE_WIND_SPEED_SAMPLES_UL : ulWindSamplesIdx_;
		stAeroData_.fAverageWindSpeed = 0.0;
		for (int ulIdx = 0; ulIdx < ulNumSamples; ulIdx++) {
			stAeroData_.fAverageWindSpeed += afWindSamples_[ulIdx];
		}
		stAeroData_.fAverageWindSpeed /= ulNumSamples;	
	}
}

/****************************************** FUNCTION *******************************************//**
* \brief This method manages the activation of the break system
***************************************************************************************************/
void breakManagement() {

	/* Check if it is necessary to break */
	if (stAeroData_.fAverageWindSpeed > stControlParams_.fMaxWindSpeed || /* Average wind speed over threshold */
		stAeroData_.fRotorSpeedRPM > stControlParams_.fMaxWindSpeed    || /* Rotor speed over threshold */
		stControlParams_.bManualBreak) /* Rotor break manually requested */
	{ 
		/* Break */
		vBreak(); 
	}
	/* If none of the conditions to activate the break is currently satisfied, release break */
	else 
	{
		if (stAeroData_.stStatus.eBreakStatus == BREAK_ENABLED)
		{
			vReleaseBreak();
		}
	}
}

/****************************************** FUNCTION *******************************************//**
* \brief This method activates the break system
***************************************************************************************************/
void vBreak() {

	/* Save time of the activation request */
	ullBreakLastRequestTimeMs_ = millis();

	// Antes de empezar un nuveo frenado, tenemos que esperar a que se termine de retraer 
	// completamente el actuador para evitar acumular retraccion
	/* Before to start a break operation, check if the required conditions are met */
	if (stAeroData_.stStatus.eBreakStatus == BREAK_DISABLED  && /* Break only if the break is disabled */
		stAeroData_.stStatus.eBreakStatus != BREAK_RELEASING && /* Do not interrupt when actuator is moving */
		stAeroData_.stStatus.eBreakStatus != BREAK_BREAKING )   /* If system es already breaking, there is no need to do it again */
	{
		/* Active break */
		digitalWrite(ENABLE_BREAK_RELAY_PIN, LOW);

		/* Store time of the maneouver start */
		ullBreakManeouverStartTimeMs_ = millis();

		/* Update the status variable */
		stAeroData_.stStatus.eBreakStatus = BREAK_BREAKING;
	}	
}

/****************************************** FUNCTION *******************************************//**
* \brief This method releases the break system
***************************************************************************************************/
void vReleaseBreak() 
{
	/* The break is released only if the break is fully enabled (not moving) and it has been breaked
	for a minimum time interval */
	if (stAeroData_.stStatus.eBreakStatus == BREAK_ENABLED && /* Break can only be released if the system is breaked */
		millis()-ullBreakLastRequestTimeMs_ > BREAK_MIN_ENABLED_TIME_MS ) /* Release only if passed some time from las activation request */
	{
		/* Release break */
		digitalWrite(DISABLE_BREAK_RELAY_PIN, LOW);

		/* Store time of the maneouver start */
		ullBreakManeouverStartTimeMs_ = millis();

		/* Update the status variable */
		stAeroData_.stStatus.eBreakStatus = BREAK_RELEASING;
	}
}

/****************************************** FUNCTION *******************************************//**
* \brief This method checks if the break actuator maneouver must be ended
***************************************************************************************************/
void vFinishBreakManoeuver() 
{
	/* Check if it is time to stop the actuator and finish break manoeuver */
	if (stAeroData_.stStatus.eBreakStatus == BREAK_BREAKING &&
		millis() - ullBreakManeouverStartTimeMs_ > TIME_BREAK_ACT_OP_EXTENSION_MS) 
	{
		/* Deactivate relay of the actuator */
		digitalWrite(ENABLE_BREAK_RELAY_PIN, HIGH); 

		/* Break is enabled */
		stAeroData_.stStatus.eBreakStatus = BREAK_ENABLED;
	}
	
	/* Check if it is time to stop the actuator and finish release manoeuver */
	if (stAeroData_.stStatus.eBreakStatus == BREAK_RELEASING && 
		millis() - ullBreakManeouverStartTimeMs_ > BREAK_RETRACTION_TIME_MS) 
	{
		/* Deactivate relay of the actuator */
		digitalWrite(DISABLE_BREAK_RELAY_PIN, HIGH);

		/* Break is disabled */
		stAeroData_.stStatus.eBreakStatus = BREAK_DISABLED;
	}
}

/****************************************** FUNCTION *******************************************//**
* \brief This method controls the pitch angle of the blades
***************************************************************************************************/
void vBladePitchControl() 
{
	/* If the system is breaked or breaking, use pitch angle to help breaking */
	if (stAeroData_.stStatus.eBreakStatus == BREAK_ENABLED ||
		stAeroData_.stStatus.eBreakStatus == BREAK_BREAKING)
	{
		clPitchControlServo_.SetExtensionPorcentaje(0);
	}
	/* If the system is not breaked, manage pitch angle */
	else 
	{ 
		/* Manual pitch angle control */
		if (stControlParams_.ePitchMode == PITCHMODE_MANUAL) 
		{ 
			clPitchControlServo_.SetExtensionPorcentaje(stControlParams_.fBladePitchPercentage);
		}
		/* Automatic pitch control */
		else if(stControlParams_.ePitchMode == PITCHMODE_AUTO) 
		{ 
			/* Compute the angle of the aerodynaimc velocity wrt rotor disc */
			float fAlphaWind = 0.0;
			
			/* If there is no wind, rpm should be 0, so there would be an indetermination */
			if (stAeroData_.fWindSpeed < 1)
			{ 
				fAlphaWind = PI / 2;
			}
			/* Compute angle os aerodinamic speed for a section at 0.3 adimensional distance 
			(aprox 33 cm) from the root. */
			else
			{
				/* Wind angle (composed from incident wind and blade rotation) relativo to the
				plane of blade rotation */
				fAlphaWind = atan2(stAeroData_.fWindSpeed,
								   stAeroData_.fRotorSpeedRPM * RPM_TO_RADSEC_F * 0.33 );
			} 
				
			/* The angle we have to rotate the blade is the angle of the wind less the torsion angle
			and less the angle to operate at maximum aerodynamic efficiency */
			float fBetaAngle = fAlphaWind - 22 * PI / 180; /* Torsion angle at section 0.3 is 12.75 deg. Angle of max efficiency is 9.25 deg. Total is 22 */
			float fActuatorExtensionPercent = 
				tInterp1D<float, PITCH_ANGLE_CALIBRATION_POINTS_UC>(PITCH_CONTROL_BETA_ANGLE_F, 
																	PITCH_CONTROL_EXTENSION_F, 
																	fBetaAngle);
			
			/* Command actuator extension */
			clPitchControlServo_.SetExtensionPorcentaje(fActuatorExtensionPercent);
		}
	}
	
	/* This function should be called allways in the main loop */
	clPitchControlServo_.Operar();
}

/****************************************** FUNCTION *******************************************//**
* \brief Interrupt function to read anemomenter hall sensor
***************************************************************************************************/
void vReadAnemometerHallSensor() 
{
	/* This if avoids reading several times the same magnet pass */ 
	if (millis() - ullAnemometerLastTimeMs > HALL_MIN_DELAY_MS_ULL) 
	{
		/* Get the angular speed of the anemometer and convert to wind speed */
		float fAnemAngularSpeed = 2 * PI / TACOMETER_NUM_MAGNETS / 
								  (millis() - ullAnemometerLastTimeMs) / MILLIS_TO_SECONDS_F; 
		stAeroData_.fWindSpeed = 
			tInterp1D<float, ANEM_CALIBRATION_POINTS_UC>(ANEM_CALIBRATION_ANGULAR_SPEED_DATA_F,
										   				 ANEM_CALIBRATION_WIND_DATA_F,
										   				 fAnemAngularSpeed);

		/* Update time of last reading */
		ullAnemometerLastTimeMs = millis();
	}
}

/****************************************** FUNCTION *******************************************//**
* \brief Interrupt function to read rotor hall sensor
***************************************************************************************************/
void vReadTacometerHallSensor() 
{
	/* This if avoids reading several times the same magnet pass */ 
	if (millis() - ullTacometerLastTimeMs > HALL_MIN_DELAY_MS_ULL) 
	{
		/* Get the angular speed of the tacometer */
		float fRotorAngularSpeed = 2 * PI / ANEMOMETER_NUM_MAGNETS / 
								   (millis() - ullTacometerLastTimeMs) / MILLIS_TO_SECONDS_F;

		/* Update time of last reading */
		ullTacometerLastTimeMs = millis();
	}
}

/****************************************** FUNCTION *******************************************//**
* \brief 1D linear interpolation/extrapolation
* \tparam Type_t: Type for the data to interpolate
* \tparam ulDataLength: Length of array of data to interpolate
* \param[in] atX: X coordinates of the interpolation function y=f(x)
* \param[in] atY: Value of the interpolation function in the atX points
* \param[in] tQueryX: Query point where the function wants to be known
* \return Value of the function at the query point
* \warning Values in atX must be sorted from lower to higher and values cannot repeat
***************************************************************************************************/
template<typename Type_t, unsigned int ulDataLength>
Type_t tInterp1D(const Type_t atX[ulDataLength], const Type_t atY[ulDataLength], const Type_t tQueryX)
{
	/* Find the index of the data that is right before the query point */
	char scPrevIndex = 0;
	for(scPrevIndex = 0; scPrevIndex < ulDataLength; scPrevIndex++)
	{
		if(tQueryX <= atX[scPrevIndex])
		{
			scPrevIndex = scPrevIndex - 1;
			break;
		}
	}

	/* The index can not be lower than 0 and greater than (ulDataLength-2) */
	scPrevIndex = min(max(0, scPrevIndex), ulDataLength-2);

	/* Perform linear interpolation using the previous and next point */
	Type_t tSlope = (atY[scPrevIndex+1] - atY[scPrevIndex]) / ((atX[scPrevIndex+1] - atX[scPrevIndex]));
	Type_t tResult = atY[scPrevIndex] + tSlope * (tQueryX - atX[scPrevIndex]);

	return tResult;
}


