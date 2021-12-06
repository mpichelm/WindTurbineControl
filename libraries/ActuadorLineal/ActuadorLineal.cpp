/******************************************** INCLUDES ********************************************/
/* System includes */

/* Custom includes */
#include <arduino.h> // TODO: just to use Serial.print
#include "ActuadorLineal.h"


/******************************************** FUNCTION *****************************************//**
***************************************************************************************************/
LinearServo_cl::LinearServo_cl()
{

}

/******************************************** FUNCTION *****************************************//**
***************************************************************************************************/
void LinearServo_cl::vSetup(const int   ulRelayRetractionPin, 
							const int   ulRelayExtensionPin, 
							const int   ulHallSensorPin, 
							const float fServoTotalLenght, 
							const float fServoUsedLength, 
							const int   ulMaxTurns)
{
	/* Store input variables */
	ulRelayRetractionPin_ = ulRelayRetractionPin;
	ulRelayExtensionPin_ = ulRelayExtensionPin;
	ulHallSensorPin_ = ulHallSensorPin;
	fServoTotalLenght_ = fServoTotalLenght;
	fServoUsedLength_ = fServoUsedLength;
	ulMaxTurns_ = ulMaxTurns;

	/* Attach the interrupt function */
	attachInterrupt(digitalPinToInterrupt(ulHallSensorPin), vReadHallSensor, RISING);

	/* Declare pint for relays and hall sensor */
	pinMode(ulRelayRetractionPin_, OUTPUT); 
	pinMode(ulRelayExtensionPin_,  OUTPUT); 
	pinMode(ulHallSensorPin_,      INPUT);

	/* Retract actuator completely, to start from a known position */
	Serial.println("Initial retraction...");
	vRetractServo();
	delay(CALIBRATION_TIME_MS_ULL);
 	ulCurrentTurns_ = 0;

	/* Compute extension/turns ratio */
	fExtensionTurnRatio_ = fServoTotalLenght / ulMaxTurns;	

	/* Set to false the calibration boolean */
	bCalibrating_ = false;	
}

/******************************************** FUNCTION *****************************************//**
***************************************************************************************************/
void LinearServo_cl::vOperate() 
{
	/* Check if it is in calibration mode or not */
	if (!bCalibrating_) 
	{ 
		/* Compute the target number of turns */
		ulTargetTurns_ = fRequestedLength_ / fExtensionTurnRatio_;

		/* Compare current position with target position */
		if (ulTargetTurns_ > ulCurrentTurns_ &&
			abs(ulTargetTurns_ - ulCurrentTurns_) > MAX_TURNS_ERROR_UL) 
		{
			/* Need to extend */
			vExtendServo();
		}
		else if (ulTargetTurns_ < ulCurrentTurns_ &&
				 abs(ulTargetTurns_ - ulCurrentTurns_) > MAX_TURNS_ERROR_UL) 
		{
			/* Need to retract */
			vRetractServo();
		}
		else 
		{
			/* Need to stop */
			vStopActuador();
		}
	}
	/* In calibration mode... */
	else 
	{ 	
		/* Control the duration of the calibration */
		if (millis() - ullCalibrationStartTime_ > CALIBRATION_TIME_MS_ULL) 
		{ 
			bCalibrating_ = false; /* After the calibration time passed, the boolean is disabled */
			ulCurrentTurns_ = 0;   /* Servo is fully retracted */
		}
	}
}

/******************************************** FUNCTION *****************************************//**
***************************************************************************************************/
void LinearServo_cl::vSetExtensionLength(const float fLengthMillimeter) 
{
	fRequestedLength_ = min(fLengthMillimeter, fServoUsedLength_);
}

/******************************************** FUNCTION *****************************************//**
***************************************************************************************************/
void LinearServo_cl::vSetExtensionPercentage(const float fPercent) 
{
	fRequestedLength_ = fPercent / 100.0f * fServoUsedLength_;
}

/******************************************** FUNCTION *****************************************//**
***************************************************************************************************/
float LinearServo_cl::fGetExtensionPercentage() 
{
	return ulCurrentTurns_ * fExtensionTurnRatio_ / fServoUsedLength_ * 100.0f;
}

/******************************************** FUNCTION *****************************************//**
***************************************************************************************************/
void LinearServo_cl::vExtendServo()
{
	vStopActuador();
	eServoState_ = SERVOSTATE_EXTENDING;
	digitalWrite(ulRelayRetractionPin_, HIGH);
	digitalWrite(ulRelayExtensionPin_, LOW);
}

/******************************************** FUNCTION *****************************************//**
***************************************************************************************************/
void LinearServo_cl::vRetractServo()
{
	vStopActuador();
	eServoState_ = SERVOSTATE_RETRACTING;
	digitalWrite(ulRelayRetractionPin_, LOW);
	digitalWrite(ulRelayExtensionPin_, HIGH);
}

/******************************************** FUNCTION *****************************************//**
***************************************************************************************************/
void LinearServo_cl::vStopActuador()
{
	eServoState_ = SERVOSTATE_STOPPED;
	digitalWrite(ulRelayExtensionPin_, HIGH);
	digitalWrite(ulRelayRetractionPin_, HIGH);
}

/******************************************** FUNCTION *****************************************//**
***************************************************************************************************/
int LinearServo_cl::ulGetCurrentTurns()
{
	return ulCurrentTurns_;
}

/******************************************** FUNCTION *****************************************//**
***************************************************************************************************/
void LinearServo_cl::vSetCurrentTurns(const int ulCurrentTurns)
{
	ulCurrentTurns_ = ulCurrentTurns;
}

/******************************************** FUNCTION *****************************************//**
***************************************************************************************************/
void LinearServo_cl::vCalibrate() 
{
	/* Activate calibration mode if not in calibration mode */
	if (!bCalibrating_) 
	{ 
		bCalibrating_ = true;
		ullCalibrationStartTime_ = millis();
		vRetractServo();
	}
}

/******************************************** FUNCTION *****************************************//**
***************************************************************************************************/
static void vReadHallSensor() 
{
	/* This if avoids triggering the interrupt multiple times for the same detection */
	if (millis() - ulLastInterruptTime_ > 10) 
	{
		if (eServoState_ == SERVOSTATE_EXTENDING) 
		{
			ulCurrentTurns_ = ulCurrentTurns_ + 1;
		}
		else if (eServoState_ == SERVOSTATE_RETRACTING) 
		{
			ulCurrentTurns_ = ulCurrentTurns_ - 1;
		}
		
		/* Saturate between min and max value */
		ulCurrentTurns_ = min(ulCurrentTurns_, ulMaxTurns_); 

		/* Update time of last activation */
		ulLastInterruptTime_ = millis();
	}
}

