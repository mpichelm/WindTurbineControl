/******************************************** INCLUDES ********************************************/
/* System includes */
#include <string>
#include <ESP8266WiFi.h>
#include <CommonConstants.h>
#include <CommonTypes.h>
#include <CommsManager.h>
#include <Metro.h>

/* Custom includes */


/******************************************* CONSTANTS ********************************************/

/* WIFI */
const int CLIENT_TIMEOUT_MS_UL                    = 200;   /**< Milliseconds to consider a client disconnected      */
const int WIFI_RECONNECT_PERIOD_MS_UL             = 10000; /**< Milliseconds until finding Wifi networks again      */
const int SERIAL_DATA_SEND_PERIOD_MS_UL           = 500;   /**< Milliseconds to send data again through serial port */
const int SERVER_PORT_UL                          = 80;    /**< Default port                                        */
const unsigned char NUM_WIFI_NETWORKS             = 2;     /**< Number of known wifi networks                       */
const std::string WIFI_SSID_AS[NUM_WIFI_NETWORKS] =        /**< SSID of known wifi networks                         */
                {"Tenda_07B540", "MOVISTAR_2610"};             
const std::string WIFI_PASS_AS[NUM_WIFI_NETWORKS] =        /**< Password for known wifi networks                    */
                {"123456789", "Q2bYmTm7xdfPkTWJyBX5"};

/* COMMUNICATIONS CONSTANTS */
const int BAUD_RATE         = 9600; /**< Baud rate for serial communications                    */
const char MSG_DELIMITER_SC = ';';  /**< Delimiter between different values in a string message */
const char MSG_START_SC     = '[';  /**< Character to indicate the start of a http message      */
const char MSG_END_SC       = ']';  /**< Character to indicate the start of a http message      */


/******************************************** GLOBALS *********************************************/
/* Operational data */
AeroData_st      stAeroData_      = {};	/**< Current data 				 */
ControlParams_st stControlParams_ = {};	/**< Control requests by the user */

/* Communications variables */
const unsigned int MAX_MSG_SIZE_UL_ = sizeof(ControlParams_st) > sizeof(AeroData_st) ? 
									  sizeof(ControlParams_st) : sizeof(AeroData_st);
unsigned char aucReadingBuf_[MAX_MSG_SIZE_UL_];

WiFiServer clServer_(SERVER_PORT_UL); 							   /**< Instance for the wifi server                                                  */
CommsManager_cl clCommsManager_;	 							   /**< Manager to communicate with the Arduino                                       */
Metro clSenderSerialTimer_ = Metro(SERIAL_DATA_SEND_PERIOD_MS_UL); /**< Timer to send messages through serial port                                    */
bool bNewMessageWifi_ = false;									   /**< A new message has been received trough wifi                                   */
bool bFlagClientInitialData_ = false;							   /**< Boolean that indicates if the app has received inital state of control params */


/****************************************** FUNCTION *******************************************//**
* \brief Setup function for the Arduino board
***************************************************************************************************/
void setup() 
{
	/* Serial port initialization */
	Serial.begin(BAUD_RATE);
	delay(10);

	 /* Connect to WIFI network */
	 vWifiConnect();
}

/****************************************** FUNCTION *******************************************//**
* \brief Loop function for the Arduino board
***************************************************************************************************/
void loop() 
{
	/* Read messages received from Serial port */
	vReadSerialArduino();

	/* Send data serial port */
	vSendDataArduinoSerial();

	/* Read data from Android */
	vManageWifiClient();
}

/****************************************** FUNCTION *******************************************//**
* \brief Method that receives data from Android application through the WIFI module
* \return Boolean indicating if the message is valid
***************************************************************************************************/
bool bReadAndroid(std::string sMessage) 
{
	/* Check that the message contains the start and end delimiters */
	bool bMessageValid = (sMessage.find(MSG_START_SC) != std::string::npos) &&
					     (sMessage.find(MSG_END_SC)   != std::string::npos);

	/* Initialize to false the boolean that indicates the Android app has initialized its control 
	parameters. The rest of parameters are only read if this is true */
	bFlagClientInitialData_ = false;

	/* If the message is valid... */
	if (bMessageValid)
	{
		/* Remove except what is inside the brackets [] */ 
		sMessage = sMessage.substr(sMessage.find(MSG_START_SC) + 1, 
								   sMessage.find(MSG_END_SC) - sMessage.find(MSG_START_SC) - 1);

		size_t tPos = 0;
		std::string sToken;
		unsigned char ucIdx = 0; /* Use this index to know which field of the structure is being populated */
		sMessage += ";"; /* Add final ; to make the while loop work for the last element */
		while ((tPos = sMessage.find(MSG_DELIMITER_SC)) != std::string::npos) 
		{
			sToken = sMessage.substr(0, tPos);
			switch (ucIdx)
			{
			case 0 /* bFlagClientInitialData_ */:
				bFlagClientInitialData_ = static_cast<bool>(std::atoi(sToken.c_str()));
				break;

			case 1 /* fMaxRotorSpeedRPM */:
				if (bFlagClientInitialData_)
				{
					stControlParams_.fMaxRotorSpeedRPM = std::atof(sToken.c_str());
				}
				break;

			case 2 /* fMaxWindSpeed */:
				if (bFlagClientInitialData_)
				{
					stControlParams_.fMaxWindSpeed = std::atof(sToken.c_str());
				}
				break;

			case 3 /* fBladePitchPercentage */:
				if (bFlagClientInitialData_)
				{
					stControlParams_.fBladePitchPercentage = std::atof(sToken.c_str());
				}
				break;

			case 4 /* bManualBreak */:
				if (bFlagClientInitialData_)
				{
					stControlParams_.eManualBreak = static_cast<ManualBreak_e>(std::atoi(sToken.c_str()));
				}
				break;

			case 5 /* ePitchMode */:
				if (bFlagClientInitialData_)
				{
					stControlParams_.ePitchMode = static_cast<PitchMode_e>(std::atoi(sToken.c_str()));
				}
				break;
				
			default:
				break;
			}

			/* Remove the token already processed */				
			sMessage.erase(0, tPos + 1); /* +1 accounts for the size of the delimiter */

			/* Increment index */
			ucIdx++;
		}
	}

	return bMessageValid;
}

/****************************************** FUNCTION *******************************************//**
* \brief Method that receives data from the user arduino using Serial communications
***************************************************************************************************/
void vReadSerialArduino() 
{
	/* Create a variable for the message length and other one for the message ID */
	unsigned int ulMsgLength = 0;
	MessageID_e eMsgID = MESSAGEID_COUNT;

	/* Check if there is input data */
	while (clCommsManager_.bReadInputMessage(Serial, aucReadingBuf_, ulMsgLength, eMsgID))
	{
		/* Copy the buffer to the message */
		if (eMsgID == MESSAGEID_AERODATA)
		{
			memcpy(&stAeroData_, aucReadingBuf_, sizeof(stAeroData_));
		}
		else if (eMsgID == MESSAGEID_CONTROLPARAMS)
		{
			memcpy(&stControlParams_, aucReadingBuf_, sizeof(stControlParams_));
		}
	}
}

/****************************************** FUNCTION *******************************************//**
* \brief Method that manages wifi communications
***************************************************************************************************/
void vManageWifiClient() 
{
	bool bClientOk = true;

	/* Check if a client has connected */
	WiFiClient clClient = clServer_.available();
	if (clClient) 
	{
		//Serial.println("Client available");
	
		/* Define a timeout for clients. Google Chrome opens the connection in a different way and makes
		it necessary this piece of code. Otherwise, the ESP8266 stops answering after a random number of
		requests. This problem was not found when connecting from Android or Explorer. More info here:
		https://github.com/esp8266/Arduino/issues/3735*/
		unsigned long timeout = millis();
		while (clClient.available() == 0 && bClientOk) 
		{
			if (millis() - timeout > CLIENT_TIMEOUT_MS_UL) 
			{
				//Serial.println("Client stopped");
				clClient.stop();
				bClientOk = false;
			}
		}

		/* If the client is ok, create a response */
		if (bClientOk)
		{
			/* Read the request as a string */	
			bNewMessageWifi_ = bReadAndroid(clClient.readStringUntil('\n').c_str());

			if (bNewMessageWifi_)
			{
				/* Elaborate and send response */
				clClient.print(sBuildMsgToAndroid());
			}
		}
	}
	else
	{
		bClientOk = false;
	}

	/* Flush client */
	clClient.flush();
}

/****************************************** FUNCTION *******************************************//**
* \brief Method that finds the wifi network and connects to it
***************************************************************************************************/
void vWifiConnect() 
{
	Serial.println("Scan start");

	/* Try to connect until a known WIFI network is found */
	bool bWifiFound = false;
	unsigned char ucWifiIdx = 0;
	while (!bWifiFound) 
	{
		/* Get the number of available networks */
		unsigned char ucNumNetworks = WiFi.scanNetworks();
		Serial.println("Scan done");
		if (ucNumNetworks == 0) 
		{
			Serial.println("No networks found");
		}
		else 
		{
			/* Print number of networks found */
			Serial.print(ucNumNetworks);
			Serial.println(" networks found");

			/* If at least one network have been found, check if its knwon */
			for (unsigned char ucFoundWifiIdx = 0; ucFoundWifiIdx < ucNumNetworks; ++ucFoundWifiIdx) 
			{
				/* Check if the found wifi network matches any knwon one */
				for (unsigned char ucKnownWifiIdx = 0; ucKnownWifiIdx < NUM_WIFI_NETWORKS; ucKnownWifiIdx++)
				{
					/* Check if the network is known */
					if (WiFi.SSID(ucFoundWifiIdx).equals(WIFI_SSID_AS[ucKnownWifiIdx].c_str())) 
					{
						ucWifiIdx = ucKnownWifiIdx;
						bWifiFound = true;
						break;
					}
				}

				/* If the network has been found, stop searching */
				if (bWifiFound)
				{
					break;
				}
			}
	    }
		delay(WIFI_RECONNECT_PERIOD_MS_UL);
	}

	/* Connect to WiFi network */
	Serial.println();
	Serial.println();
	Serial.print("Connecting to ");
	Serial.println(WIFI_SSID_AS[ucWifiIdx].c_str());

	WiFi.mode(WIFI_STA);
	WiFi.begin(WIFI_SSID_AS[ucWifiIdx].c_str(), WIFI_PASS_AS[ucWifiIdx].c_str());

	/* Wait until it connects */
	while (WiFi.status() != WL_CONNECTED) 
	{
		delay(500);
		Serial.print(".");
	}
	Serial.println("");
	Serial.println("WiFi connected");
	
	/* Start the server */
	clServer_.begin();
	Serial.println("Server started");

	/* Print the IP address */
	Serial.println(WiFi.localIP());
}

/****************************************** FUNCTION *******************************************//**
* \brief Method that creates the message to be sent to the Android
***************************************************************************************************/
String sBuildMsgToAndroid() 
{
	/* Beginning of the message */
	String sMsg = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>\r\n";

	/* Concatenate parameter os AeroData structure */
	sMsg.concat(MSG_START_SC);
	sMsg.concat(stAeroData_.fAverageWindSpeed);
	sMsg.concat(MSG_DELIMITER_SC);
	sMsg.concat(stAeroData_.fBladePitchPercentage);
	sMsg.concat(MSG_DELIMITER_SC);
	sMsg.concat(stAeroData_.fRelHumidity);
	sMsg.concat(MSG_DELIMITER_SC);
	sMsg.concat(stAeroData_.fRotorSpeedRPM);
	sMsg.concat(MSG_DELIMITER_SC);
	sMsg.concat(stAeroData_.fTempCelsius);
	sMsg.concat(MSG_DELIMITER_SC);
	sMsg.concat(stAeroData_.fWindSpeed);
	sMsg.concat(MSG_DELIMITER_SC);
	sMsg.concat(stAeroData_.stStatus.eBreakStatus);
	sMsg.concat(MSG_DELIMITER_SC);
	sMsg.concat(stAeroData_.stStatus.ePitchMode);
	sMsg.concat(MSG_DELIMITER_SC);
	sMsg.concat(stControlParams_.fMaxRotorSpeedRPM);
	sMsg.concat(MSG_DELIMITER_SC);
	sMsg.concat(stControlParams_.fMaxWindSpeed);
	sMsg.concat(MSG_DELIMITER_SC);
	sMsg.concat(stControlParams_.fBladePitchPercentage);
	sMsg.concat(MSG_DELIMITER_SC);
	sMsg.concat(stControlParams_.eManualBreak);
	sMsg.concat(MSG_DELIMITER_SC);
	sMsg.concat(stControlParams_.ePitchMode);
	sMsg.concat(MSG_END_SC);

	/* Add message end */
	return sMsg;
}

/****************************************** FUNCTION *******************************************//**
* \brief Method that sends information to the Arduino User trough Serial port
***************************************************************************************************/
void vSendDataArduinoSerial() 
{
	/* Check if it is time to send new data */
	//if (clSenderSerialTimer_.check()) 
	if (bNewMessageWifi_)
	{
		// Serial.println("enviando...");
		// Serial.print("fMaxRotorSpeedRPM: ");
		// Serial.println(stControlParams_.fMaxRotorSpeedRPM);
		// Serial.print("fMaxWindSpeed: ");
		// Serial.println(stControlParams_.fMaxWindSpeed);
		// Serial.print("fBladePitchPercentage: ");
		// Serial.println(stControlParams_.fBladePitchPercentage);
		// Serial.print("bManualBreak: ");
		// Serial.println(stControlParams_.eManualBreak);
		// Serial.print("ePitchMode: ");
		// Serial.println(stControlParams_.ePitchMode);

		clCommsManager_.vSendMessage(stControlParams_, MESSAGEID_CONTROLPARAMS, Serial);
		bNewMessageWifi_ = false;
	}
}
