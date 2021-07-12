#ifndef COMMUNICATION_UTILS_H_
#define COMMUNICATION_UTILS_H_

/******************************************** INCLUDES ********************************************/
/* System includes */
#include <assert.h>
#include <Stream.h>

/* Custom includes */
#include "CommonConstants.h"
#include "CommonTypes.h"


/****************************************** FUNCTION *******************************************//**
* \brief This function sends a message whose body is the data contained in a structure. A 4 bytes 
* header and 4 bytes checksum is added
* \param[in] dataStruct: Structure containing the data for the message body
* \param[in] msgId: Id the of the message, necessary to fill the message header
* \param[in] serial: Handle to the serial port to be used to send data
* \tparam Type_t: Type for the structure to be sent
***************************************************************************************************/
template <typename Type_t>
void vSendMessage(const Type_t& dataStruct, MessageID_e msgId, Stream &serial)
{
    /* Initialize a buffer to store message */
    const int msgLength = sizeof(Type_t) + sizeof(MsgHeader_st) + NUM_CHECKSUM_BYTES;
    unsigned char buffer[msgLength];
    unsigned int bufferPosition = 0;

    /* Insert header */
    MsgHeader_st msgHeader = {MESSAGE_PREAMBLE, msgId, msgLength};
    memcpy(buffer, msgHeader, sizeof(MsgHeader_st));
    bufferPosition += sizeof(MsgHeader_st);

    /* Insert message body */
    memcpy(buffer + bufferPosition, &dataStruct, sizeof(Type_t));
    bufferPosition += sizeof(Type_t);

    /* Insert checksum (message body only) */
    int checksum = ModRTU_CRC(buffer + sizeof(MsgHeader_st), sizeof(Type_t));
    memcpy(buffer + bufferPosition, &checksum, sizeof(int));

    /* Send data */
    serial.write(buffer, msgLength);
}

/****************************************** FUNCTION *******************************************//**
* \brief This function converts an array of bytes into a structure
* \param[in] buffer: Bytes to be converted to the structure
* \param[in] length: Number of bytes contained in the buffer
* \tparam Type_t: Type for the structure
***************************************************************************************************/
template <typename Type_t>
Type_t vComposeStruct(const unsigned char* buffer, const unsigned int length)
{
    /* Check that the length of the buffer is compatible with the size of the structure */
    assert(length == sizeof(Type_t));

    /* Compose the structure */
    Type_t outputData = {};
    memcpy(outputData, buffer, sizeof(Type_t))

    /* Return the data */
    return outputData;
}

/****************************************** FUNCTION *******************************************//**
* \brief This function checks if an input message has a valid CRC and reads its header
* \param[in] buffer: Input array of bytes that compose the message
* \param[in] length: length of bytes that compose the message
***************************************************************************************************/
MsgHeader_st vCheckInputMsg(const unsigned char* buffer, const unsigned int length);

/****************************************** FUNCTION *******************************************//**
* \brief This function CRC checksum
* \param[in] buf: Input array of bytes to compute CRC
* \param[in] len: Lngth of the buffer
* \return CRC checksum
* \note https://stackoverflow.com/questions/17474223/getting-the-crc-checksum-of-a-byte-array-and-adding-it-to-that-byte-array
***************************************************************************************************/
int ModRTU_CRC(const unsigned char* buf, const int len);


#endif /* COMMUNICATION_UTILS_H_ */