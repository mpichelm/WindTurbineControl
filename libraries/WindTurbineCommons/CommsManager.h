#ifndef COMMS_MANAGER_H_
#define COMMS_MANAGER_H_

/******************************************** INCLUDES ********************************************/
/* System includes */
#include <assert.h>
#include <Stream.h>

/* Custom includes */
#include "CommonConstants.h"
#include "CommonTypes.h"


/********************************************* CLASS **********************************************/
class CommsManager_cl
{
public:
    /****************************************** FUNCTION ***************************************//**
    * \brief Constructor of the communications manager class
    ***********************************************************************************************/
    CommsManager_cl();

    /****************************************** FUNCTION ***************************************//**
    * \brief Destructor of the communications manager class
    ***********************************************************************************************/
    ~CommsManager_cl();

    /****************************************** FUNCTION ***************************************//**
    * \brief This function sends a message whose body is the data contained in a structure. A 4 bytes 
    * header and 4 bytes checksum is added
    * \param[in] tDataStruct: Structure containing the data for the message body
    * \param[in] eMsgId: Id the of the message, necessary to fill the message header
    * \param[in] clSerial: Handle to the serial port to be used to send data
    * \tparam Type_t: Type for the structure to be sent
    ***********************************************************************************************/
    template <typename Type_t>
    void vSendMessage(const Type_t& tDataStruct, MessageID_e eMsgId, Stream& clSerial)
    {
        /* Initialize a buffer to store message */
        const unsigned int ulMsgLength = sizeof(Type_t) + sizeof(MsgHeader_st) + NUM_CHECKSUM_BYTES_UC;
        unsigned char aucBuffer[ulMsgLength];
        unsigned int ulBufferPosition = 0;

        /* Insert header */
        MsgHeader_st stMsgHeader = {MESSAGE_PREAMBLE_UL, eMsgId, ulMsgLength};
        memcpy(aucBuffer, &stMsgHeader, sizeof(MsgHeader_st));
        ulBufferPosition += sizeof(MsgHeader_st);

        /* Insert message body */
        memcpy(aucBuffer + ulBufferPosition, &tDataStruct, sizeof(Type_t));
        ulBufferPosition += sizeof(Type_t);

        /* Insert checksum (message body only) */
        int slChecksum = slModRTU_CRC(aucBuffer + sizeof(MsgHeader_st), sizeof(Type_t));
        memcpy(aucBuffer + ulBufferPosition, &slChecksum, sizeof(int));

        /* Send data */
        clSerial.write(aucBuffer, ulMsgLength);
    }

    /****************************************** FUNCTION ***************************************//**
    * \brief This function tries to read a new message from the buffer
    * \param[in] clSerial: Stream (serial port) to read from
    * \param[out] pucMessage: Buffer where the message will be copied
    * \param[out] ulMsgLength: Length of the read message
    * \param[out] eMsgId: Id of the received message
    ***********************************************************************************************/
    bool bReadInputMessage(Stream&        clSerial,
                           unsigned char* pucMessage, 
                           unsigned int   ulMsgLength,
                           MessageID_e    eMsgId);

private:
    /****************************************** FUNCTION ***************************************//**
    * \brief This function checks if an input message has a valid CRC and reads its header
    * \param[in] pucBuffer: Input array of bytes that compose the message
    * \param[in] ulLength: length of bytes that compose the message
    * \param[out] stMsgHeader: header of the received message
    * \return Boolean indicating if the header have been succesfully parsed
    ***********************************************************************************************/
    bool bCheckInputMsg(const unsigned char* pucBuffer, 
                        const unsigned int   ulLength,
                              MsgHeader_st&  stMsgHeader);

    /****************************************** FUNCTION ***************************************//**
    * \brief This function converts an array of bytes into a structure
    * \param[in] pucBuffer: Bytes to be converted to the structure
    * \param[in] ulLength: Number of bytes contained in the buffer
    * \param[out] tOutputData: Structure generated from the input bytes
    * return Boolean indicating if the structure was successfully generated from the input bytes
    * \tparam Type_t: Type for the structure
    ***********************************************************************************************/
    template <typename Type_t>
    bool bComposeStruct(const unsigned char* pucBuffer, 
                        const unsigned int   ulLength, 
                              Type_t&        tOutputData)
    {

        /* Initialize output data */
        tOutputData = {};

        /* Check that the length of the buffer is compatible with the size of the structure */
        bool bStatus = ulLength == sizeof(Type_t);

        if (bStatus)
        {
            /* Compose the structure */
            Type_t tOutputData = {};
            memcpy(&tOutputData, pucBuffer, sizeof(Type_t));
        }

        /* Return the data */
        return tOutputData;
    }

    /****************************************** FUNCTION ***************************************//**
    * \brief This function gives the number of bytes remaining reach the last written byte. The 
    * unput position is counted
    * \param[in] ulPos: Starting position from where to read
    * return Number of remaining bytes
    ***********************************************************************************************/
    unsigned int ulGetNumRemainingBytes(const unsigned int ulPos);

    /****************************************** FUNCTION ***************************************//**
    * \brief This function reads an unsigned int from the buffer, starting at a given position
    * \param[in] ulPos: Starting position from where to read
    * return Unsigned int corresponding to the 4 bytes read
    ***********************************************************************************************/
    unsigned int ulReadUint32FromBuffer(const unsigned int ulPos);

    /****************************************** FUNCTION ***************************************//**
    * \brief This function CRC checksum
    * \param[in] pucBuffer: Input array of bytes to compute CRC
    * \param[in] ulLength: Lngth of the buffer
    * \return CRC checksum
    * \note https://stackoverflow.com/questions/17474223/getting-the-crc-checksum-of-a-byte-array-and-adding-it-to-that-byte-array
    ************************************************************************************************/
    int slModRTU_CRC(const unsigned char* pucBuffer, const int ulLength);

    /****************************************** FUNCTION ***************************************//**
    * \brief This function check if a message header is valid
    * \param[in] stMsgHeader: Header to be checked
    * \return Validity of the message header
    ************************************************************************************************/
    bool bCheckHeader(const MsgHeader_st& stMsgHeader);

    /***************************************** ATTRIBUTES *****************************************/
    unsigned char aucInputBuffer_[INPUT_BUFFER_LENGTH_UL]; /**< Buffer to store the received data                              */
    unsigned int  ulNextWritePos_;                         /**< Next position of the buffer to be written                      */
    unsigned int  ulNextReadPos_;                          /**< Next position of the buffer to be read                         */
    unsigned int  ulNumUsedBytes_;                         /**< Number of bytes in the buffer that have not been processed yet */
};


#endif /* COMMS_MANAGER_H_ */