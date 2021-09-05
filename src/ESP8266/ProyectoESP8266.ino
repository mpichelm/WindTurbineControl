/******************************************** INCLUDES ********************************************/
/* System includes */
#include <string>
#include <ESP8266WiFi.h>
#include <CommonConstants.h>
#include <CommonTypes.h>
#include <CommsManager.h>

/* Custom includes */


/******************************************* CONSTANTS ********************************************/

/* WIFI */
const int CLIENT_TIMEOUT_MS_UL                    = 200;   /**< Milliseconds to consider a client disconnected */
const int WIFI_RECONNECT_PERIOD_MS_UL             = 10000; /**< Milliseconds until finding Wifi networks again */
const int SERVER_PORT_UL                          = 80;    /**< Default port                                   */
const unsigned char NUM_WIFI_NETWORKS             = 2;     /**< Number of known wifi networks                  */
const std::string WIFI_SSID_AS[NUM_WIFI_NETWORKS] =        /**< SSID of known wifi networks                    */
                {"MOVISTAR_A90E", "FTE-A458"};        
const std::string WIFI_PASS_AS[NUM_WIFI_NETWORKS] =        /**< Password for known wifi networks               */
                {"e7dp6UHXQTuPR3Eye3Q3", "XXsrd7mc"};

/* COMMUNICATIONS CONSTANTS */
const int BAUD_RATE = 9600;        /**< Baud rate for serial communications */
const char MSG_DELIMITER_SC = ';'; /**< Delimiter between different values in a string message */


/******************************************** GLOBALS *********************************************/
/* Operational data */
AeroData_st      stAeroData_      = {};	/**< Current data 				 */
ControlParams_st stControlParams_ = {};	/**< Control requests by the user */

/* Communications variables */
const unsigned int MAX_MSG_SIZE_UL_ = sizeof(ControlParams_st) > sizeof(AeroData_st) ? 
									 sizeof(ControlParams_st) : sizeof(AeroData_st);
unsigned char aucReadingBuf_[MAX_MSG_SIZE_UL_];

WiFiServer clServer(SERVER_PORT_UL); /**< Instance for the wifi server                    */
WiFiClient clClient; 				 /**< Class to manage clients connected to the server */ 
CommsManager_cl clCommsManager_;	 /**< Manager to communicate with the Arduino         */


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

	/* Read data from Android */
	if (bCheckWifiClient())
	{
		void vReadAndroid();
	} 

	/* Send data to the Android app */
	clClient.print(sBuildMsgToAndroid());
}

/****************************************** FUNCTION *******************************************//**
* \brief Method that receives data from Android application through the WIFI module
***************************************************************************************************/
void vReadAndroid() 
{
	/* Read the request as a string */
	std::string sAndoidMsg = clClient.readStringUntil('\r').c_str();
	clClient.flush();

	/* Parse the string to the structure. Use try-catch to protect from imposible parsing exceptions */
	//try 
	//{
		size_t tPos = 0;
		std::string sToken;
		unsigned char ucIdx = 0; /* Use this index to know which field of the structure is being populated */
		while ((tPos = sAndoidMsg.find(MSG_DELIMITER_SC)) != std::string::npos) 
		{
			sToken = sAndoidMsg.substr(0, tPos);
			switch (ucIdx)
			{
			case 0 /* fMaxRotorSpeedRPM */:
				stControlParams_.fMaxRotorSpeedRPM = std::atof(sToken.c_str());
				break;

			case 1 /* fMaxWindSpeed */:
				stControlParams_.fMaxWindSpeed = std::atof(sToken.c_str());
				break;

			case 2 /* fBladePitchPercentage */:
				stControlParams_.fBladePitchPercentage = std::atof(sToken.c_str());
				break;

			case 3 /* bManualBreak */:
				stControlParams_.bManualBreak = static_cast<bool>(std::atoi(sToken.c_str()));
				break;

			case 4 /* ePitchMode */:
				stControlParams_.ePitchMode = static_cast<PitchMode_e>(std::atoi(sToken.c_str()));
				break;
			
			default:
				break;
			}

			/* Remove the token already processed */				
			sAndoidMsg.erase(0, tPos + 1); /* +1 accounts for the size of the delimiter */
		}
	//}
	//catch (...) 
	//{
	//	/* Catch all exceptions, but no action is necessary */
	//}

	clClient.flush();
	clClient.stop();
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
		if ((eMsgID == MESSAGEID_AERODATA) && (ulMsgLength == sizeof(AeroData_st)))
		{
			memcpy(&stAeroData_, aucReadingBuf_, ulMsgLength);
		}
	}
}

/****************************************** FUNCTION *******************************************//**
* \brief Method that checks if the client is ok
* \return Boolean indicating if the client is ok
***************************************************************************************************/
bool bCheckWifiClient() 
{
	bool bClientOk = true;

	/* Check if a client has connected */
	clClient = clServer.available();
	if (clClient) 
	{
		/* Define a timeout for clients. Google Chrome opens the connection in a different way and makes
		it necessary this piece of code. Otherwise, the ESP8266 stops answering after a random number of
		requests. This problem was not found when connecting from Android or Explorer. More info here:
		https://github.com/esp8266/Arduino/issues/3735*/
		unsigned long timeout = millis();
		while (clClient.available() == 0) 
		{
			if (millis() - timeout > CLIENT_TIMEOUT_MS_UL) 
			{
				clClient.stop();
				bClientOk = false;
			}
		}
	}
	else
	{
		bClientOk = false;
	}

	return bClientOk;
}

/****************************************** FUNCTION *******************************************//**
* \brief Method that finds the wifi network and connects to it
***************************************************************************************************/
void vWifiConnect() 
{
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
	clServer.begin();
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

	/* Add message end */
	sMsg += "</html>\n";
	return sMsg;
}