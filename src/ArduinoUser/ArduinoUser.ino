/******************************************** INCLUDES ********************************************/
/* System includes */
#include <stdlib.h>
#include <Metro.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <CommonTypes.h>
#include <CommsManager.h>

/* Custom includes */
#include "Constants.h"
#include "Types.h"


/******************************************** GLOBALS *********************************************/
/* Operational data */
AeroData_st      stAeroData_      = {};	                   /**< Current data 				 */
ControlParams_st stControlParams_ = {};	                   /**< Control requests by the user */
UserMaster_e     eUserMaster_     = DEFAULT_USER_MASTER_E; /**< Default control master		 */

/* LCD screen */
LiquidCrystal_I2C clLCD_(0x27, 20, 4);             /**< Class for the LCD management */
Metro clLCDTimer_ = Metro(LCD_REFRESH_TIME_MS_UL); /**< LCD refresh timer            */

/* HC12 Variables */
Metro clSenderHC12Timer_ = Metro(HC12_SEND_PERIOD_MS_UL); /**< HC12 timer to send messages */

/* ESP8266 wifi module */
Metro clSenderESP8266Timer = Metro(ESP8266_SEND_PERIOD_MS_UL);  /**< ESP8266 timer to send messages */

/* Communications variables */
const unsigned int MAX_MSG_SIZE_UL_ = sizeof(ControlParams_st) > sizeof(AeroData_st) ? 
									  sizeof(ControlParams_st) : sizeof(AeroData_st);
unsigned char aucReadingBuf_[MAX_MSG_SIZE_UL_];
CommsManager_cl clCommsManager_;
unsigned long ulLastEsp8266Time_ = -ANDROID_TIMEOUT_MS_UL; /**< Time of the last message received from the wifi module */

/* Auxiliary variables */
bool bLastBreakSwitchReading_ =  0; /**< Variable to hold last reading of break switch        */
int ulLastMaxRPMReading_      = -1; /**< Variable to hold last reading of RPM potentiometer   */
int ulLastMaxWindReading_     = -1; /**< Variable to hold last reading of wind potentiometer  */
int ulLastPitchReading_       = -1; /**< Variable to hold last reading of pitch potentiometer */
int ulLastPitchModeReading_   = -1; /**< Variable to hold las reading of pitch mode switch    */


/****************************************** FUNCTION *******************************************//**
* \brief Setup function for the Arduino board
***************************************************************************************************/
void setup() 
{
	/* Start LCD screen */
	clLCD_.begin();
	clLCD_.backlight();
	clLCD_.clear(); /* Clear text */

	/* Set base text on the screen */
	clLCD_.setCursor(0, 0);
	clLCD_.print("Temp: xx.x C HR: xx%"); /* Line 1 */
	clLCD_.setCursor(0, 1);
	clLCD_.print("Wind:xxm/s Max:xxm/s"); /* Line 2 */
	clLCD_.setCursor(0, 2);
	clLCD_.print("RPM: xxx   Max: xxx"); /* Line 3 */
	clLCD_.setCursor(0, 3);
	clLCD_.print("Paso          xxx% "); /* Line 3 */

	/* Initialize Arduino pins */
	pinMode(MANUAL_BREAK_PIN_UL, INPUT);         /* Reading of break switch     */
	pinMode(BREAK_LED_BLUE_PIN_UL, OUTPUT);      /* Break moving led            */
	pinMode(BREAK_LED_GREEN_PIN_UL, OUTPUT);     /* Break disabled led          */
	pinMode(BREAK_LED_RED_PIN_UL, OUTPUT);       /* Break enabled led           */
	pinMode(HC12_MODE_PIN_UL, OUTPUT);           /* HC12 mode selection         */
	pinMode(PITCH_CONTROL_SWITCH_PIN_UL, INPUT); /* Manual/automatic pitch mode */

	/* Set HC12 in transparent mode */
	digitalWrite(HC12_MODE_PIN_UL, HIGH);

	/* Initialize serial communications */
	delay(80); /* Initial delay recomended for the HC12 */
	Serial.begin(COMMS_BAUD_RATE_UL);  /* Initialize serial port to communicate with the PC      */
	Serial2.begin(COMMS_BAUD_RATE_UL); /* Initialize serial port to communicate with the ESP8266 */
	Serial1.begin(COMMS_BAUD_RATE_UL); /* Initialize serial port to communicate with the HC12    */
}

/****************************************** FUNCTION *******************************************//**
* \brief Loop function for the Arduino board
***************************************************************************************************/
void loop() 
{
	/* Manage transition of the master control */
	vMasterTransitionManagement();

	/* Read user controls */
	vReadUserInputs();

	/* Read data from the Arduino Control */
	vReadDataHC12();

	/* Read data from the Android app */
	vReadDataESP8266();

	/* Updates screen data */
	vRefreshScreen();

	/* Send data to Arduino Control */
	vSendDataHC12();

	/* Send data to the Android app */
	vSendDataESP8266();

	/* Update the break led */
	vManageBreakLed();
}

/****************************************** FUNCTION *******************************************//**
* \brief This method manages who is in controll of the wind turbine
***************************************************************************************************/
void vMasterTransitionManagement() 
{
	/* Check time from the last message received from the ESP8266. If it is over a specific 
	threshold, give control to Arduino */
	if (millis() - ulLastEsp8266Time_ > ANDROID_TIMEOUT_MS_UL) 
	{
		eUserMaster_ = USERMASTER_ARDUINO; 
	}
	else 
	{
		eUserMaster_ = USERMASTER_ANDROID; 			
	}
}


/****************************************** FUNCTION *******************************************//**
* \brief This method reads all inputs the user can control from hardware elements
***************************************************************************************************/
void vReadUserInputs() 
{
	/* User inputs are read in every iteration of the main loop, but variables are updated only if
	a physical change is detected. The objetive is that, when transitioning from Android control to
	Arduino control, variables set using Android to not automatically change to values set in 
	potentiometers. Hold Android values until a physical change */
	
	/* Read data only if the Arduino is the master */
	if (eUserMaster_ == USERMASTER_ARDUINO) 
	{
		/* Break switch reading */
		if (static_cast<int>(bLastBreakSwitchReading_) != digitalRead(MANUAL_BREAK_PIN_UL)) 
		{
			stControlParams_.bManualBreak = static_cast<bool>(digitalRead(MANUAL_BREAK_PIN_UL));
			bLastBreakSwitchReading_ = stControlParams_.bManualBreak;
		}

		/* Max RPM potentiometer reading */
		int ulPotReading = analogRead(MAX_RPM_POT_PIN_UL);
		ulPotReading = map(ulPotReading, 0, 1023, 0, MAX_RPM_F);
		if (ulLastMaxRPMReading_ != ulPotReading) 
		{
			stControlParams_.fMaxRotorSpeedRPM = ulPotReading;
			ulLastMaxRPMReading_ = ulPotReading;
		}

		/* Max wind potentiometer reading */
		ulPotReading = analogRead(MAX_WIND_POT_UL);
		ulPotReading = map(ulPotReading, 0, 1023, 0, MAX_WIND_F);
		if (ulLastMaxWindReading_ != ulPotReading) 
		{
			stControlParams_.fMaxWindSpeed = ulPotReading;
			ulLastMaxWindReading_ = ulPotReading;
		}

		/* Pitch control potentiometer reading */
		ulPotReading = analogRead(SLIDER_PITCH_PIN_UL);
		ulPotReading = map(ulPotReading, 0, 1023, 100, 0);
		if (ulLastPitchReading_ != ulPotReading) 
		{
			stControlParams_.fBladePitchPercentage = ulPotReading;
			ulLastPitchReading_ = ulPotReading;
		}

		/* Pitch control mode reading */
		int switchReading = digitalRead(PITCH_CONTROL_SWITCH_PIN_UL);
		if (ulLastPitchModeReading_ != switchReading) 
		{
			stControlParams_.ePitchMode = static_cast<PitchMode_e>(switchReading);
			ulLastPitchModeReading_ = switchReading;
		}
	}
}

/****************************************** FUNCTION *******************************************//**
* \brief Method that receives data from the adruino control
***************************************************************************************************/
void vReadDataHC12() 
{
	/* Create a variable for the message length and other one for the message ID */
	unsigned int ulMsgLength = 0;
	MessageID_e eMsgID = MESSAGEID_COUNT;

	/* Check if there is input data */
	while (clCommsManager_.bReadInputMessage(Serial1, aucReadingBuf_, ulMsgLength, eMsgID))
	{
		/* Copy the buffer to the message */
		if ((eMsgID == MESSAGEID_AERODATA) && (ulMsgLength == sizeof(AeroData_st)))
		{
			memcpy(&stAeroData_, aucReadingBuf_, ulMsgLength);
		}
	}
}

/****************************************** FUNCTION *******************************************//**
* \brief Method that reads data coming from the ESP8266 wifi module
***************************************************************************************************/
void vReadDataESP8266() 
{
	/* Create a variable for the message length and other one for the message ID */
	unsigned int ulMsgLength = 0;
	MessageID_e eMsgID = MESSAGEID_COUNT;

	/* Check if there is input data */
	while (clCommsManager_.bReadInputMessage(Serial2, aucReadingBuf_, ulMsgLength, eMsgID))
	{
		/* Copy the buffer to the message */
		if ((eMsgID == MESSAGEID_CONTROLPARAMS) && (ulMsgLength == sizeof(AeroData_st)))
		{
			memcpy(&stControlParams_, aucReadingBuf_, ulMsgLength);

			/* Update reception time of last message */
			ulLastEsp8266Time_ = millis();
		}
	}
}

/****************************************** FUNCTION *******************************************//**
* \brief Method that writes information to the lcd screen
***************************************************************************************************/
void vRefreshScreen() 
{
	/* Check if it is time to refresh screen */
	if (clLCDTimer_.check()) {

		/* Declare auxiliary variable */
		char scAuxText[5];

		/* Update temperature. Convert to a char[]. We want 5 elements, including sign and decimal
		separator, and only one decimal value */
		dtostrf(stAeroData_.fTempCelsius, 5, 1, scAuxText);
		clLCD_.setCursor(5, 0);
		clLCD_.print(scAuxText[0]); /**< sign */
		clLCD_.print(scAuxText[1]); /**< tens */
		clLCD_.print(scAuxText[2]); /**< units */
		clLCD_.print(scAuxText[3]); /**< decimal separator */
		clLCD_.print(scAuxText[4]); /**< tenths */

		/* Update humidity. Convert to a char[]. We want 3 digits with no comma or sign */
		dtostrf(stAeroData_.fRelHumidity, 3, 0, scAuxText);
		clLCD_.setCursor(16, 0);
		clLCD_.print(scAuxText[0]); /**< hundreds */
		clLCD_.print(scAuxText[1]); /**< tens  */
		clLCD_.print(scAuxText[2]); /**< units */

		/* Update wind speed. Convert to a char[]. We want 2 digits with no comma or sign */
		dtostrf(stAeroData_.fWindSpeed, 2, 0, scAuxText);
		clLCD_.setCursor(5, 1);
		clLCD_.print(scAuxText[0]); /**< tens  */
		clLCD_.print(scAuxText[1]); /**< units */

		/* Update max wind speed. Convert to a char[]. We want 2 digits with no comma or sign */
		dtostrf(stControlParams_.fMaxWindSpeed, 2, 0, scAuxText);
		clLCD_.setCursor(15, 1);
		clLCD_.print(scAuxText[0]); /**< tens  */
		clLCD_.print(scAuxText[1]); /**< units */

		/* Update rotor speed. Convert to a char[]. We want 3 digits with no comma or sign */
		dtostrf(stAeroData_.fRotorSpeedRPM, 3, 0, scAuxText);
		clLCD_.setCursor(5, 2);
		clLCD_.print(scAuxText[0]); /**< hundreds */
		clLCD_.print(scAuxText[1]); /**< tens  */
		clLCD_.print(scAuxText[2]); /**< units */

		/* Update max rotor speed. Convert to a char[]. We want 3 digits with no comma or sign */
		dtostrf(stControlParams_.fMaxRotorSpeedRPM, 3, 0, scAuxText); 
		clLCD_.setCursor(16, 2);
		clLCD_.print(scAuxText[0]); /**< hundreds */
		clLCD_.print(scAuxText[1]); /**< tens  */
		clLCD_.print(scAuxText[2]); /**< units */

		/* Update variable step mode */
		clLCD_.setCursor(5, 3);
		if (stAeroData_.stStatus.ePitchMode == PITCHMODE_MANUAL) 
		{
			clLCD_.print("manual"); 	
		}
		else 
		{
			clLCD_.print("auto. ");
		}

		/* Update blade pitch percentage. Convert to a char[]. We want 3 digits with no comma or
		sign */
		Serial.println("lcd pitch: " + (String)stAeroData_.fBladePitchPercentage);
		dtostrf(stAeroData_.fBladePitchPercentage, 3, 0, scAuxText); 
		clLCD_.setCursor(14, 3);
		clLCD_.print(scAuxText[0]); /**< hundreds */
		clLCD_.print(scAuxText[1]); /**< tens  */
		clLCD_.print(scAuxText[2]); /**< units */
	}
}

/****************************************** FUNCTION *******************************************//**
* \brief Method that sends information to the Arduino Control trough HC12
***************************************************************************************************/
void vSendDataHC12() 
{
	Serial.println("-------");
	Serial.println("Manual break = " + (String)stControlParams_.bManualBreak);
	Serial.println("Pitch mode = " + (String)stControlParams_.ePitchMode);
	Serial.println("pitch percent = " + (String)stControlParams_.fBladePitchPercentage);
	Serial.println("max rotor speed = " + (String)stControlParams_.fMaxRotorSpeedRPM);
	Serial.println("max wind speed = " + (String)stControlParams_.fMaxWindSpeed);
	Serial.println("-------");

	/* Check if it is time to send new data */
	if (clSenderHC12Timer_.check()) 
	{
		clCommsManager_.vSendMessage(stControlParams_, MESSAGEID_CONTROLPARAMS, Serial1);
	}
}

/****************************************** FUNCTION *******************************************//**
* \brief Method that sends data to the ESP8266 wifi module
***************************************************************************************************/
void vSendDataESP8266() 
{
	/* Check if it is time to send new data */
	if (clSenderESP8266Timer.check()) 
	{
		/* Send Aero data comming from the Arduino control */
		clCommsManager_.vSendMessage(stAeroData_, MESSAGEID_AERODATA, Serial2);

		/* If the Arduino is the master, send the current control params to the Wifi module, just to
		show them as the default values for the fields of the IHM */
		if (eUserMaster_ == USERMASTER_ARDUINO)
		{
			clCommsManager_.vSendMessage(stControlParams_, MESSAGEID_CONTROLPARAMS, Serial2);
		}
	}
}

/****************************************** FUNCTION *******************************************//**
* \brief Method that manages the color of the break led
***************************************************************************************************/
void vManageBreakLed() 
{
	/* Check the current break mode */
	switch (stAeroData_.stStatus.eBreakStatus)
	{
	case BREAK_ENABLED:
		vSetColorRGBLed(255, 0, 0);
		break;

	case BREAK_DISABLED:
		vSetColorRGBLed(0, 255, 0);
		break;

	case BREAK_BREAKING:
	case BREAK_RELEASING:
		vSetColorRGBLed(0, 0, 255);
		break;
	
	default:
		break;
	}
}

/****************************************** FUNCTION *******************************************//**
* \brief Method that controls the color of the RGB led
* \param[in] ucR: Red [0-255]
* \param[in] ucG: Green [0-255]
* \param[in] ucB: Blue [0-255]
***************************************************************************************************/
void vSetColorRGBLed(unsigned char ucR, unsigned char ucG, unsigned char ucB)
{
	/* We have to set (225-value) because we use common anode leds */
	analogWrite(BREAK_LED_RED_PIN_UL,   255-ucR); /**< Red */
	analogWrite(BREAK_LED_GREEN_PIN_UL, 255-ucG); /**< Green */
	analogWrite(BREAK_LED_BLUE_PIN_UL,  255-ucB); /**< Blue */
}
