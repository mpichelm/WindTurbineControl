/******************************************** INCLUDES ********************************************/
/* System includes */
#include <Arduino.h> // TODO: only for debug

/* Custom includes */
#include "CommsManager.h"

/****************************************** FUNCTION *******************************************//**
* \brief Constructor of the communications manager class
***************************************************************************************************/
CommsManager_cl::CommsManager_cl()
{
    /* Initialize array positions */
    ulNextWritePos_ = 0;
    ulNextReadPos_ = 0;   
}

/****************************************** FUNCTION *******************************************//**
* \brief Destructor of the communications manager class
***************************************************************************************************/
CommsManager_cl::~CommsManager_cl()
{
    
}

/****************************************** FUNCTION *******************************************//**
* \brief This function tries to read a new message from the buffer
* \param[in] clSerial: Stream (serial port) to read from
* \param[out] pucMessage: Buffer where the message will be copied (body only)
* \param[out] ulMsgLength: Length of the read message
* \param[out] eMsgId: Id of the received message
***************************************************************************************************/
bool CommsManager_cl::bReadInputMessage(Stream&        clSerial,
                                        unsigned char* pucMessage, 
                                        unsigned int&  ulMsgLength,
                                        MessageID_e&   eMsgId)
{
    /* Declare output variables */
    bool bMsgFound = false;
    
    /* Initialize output variable */
    eMsgId = MESSAGEID_COUNT;

    /* Read all new received bytes */
    while (clSerial.available())
    {
        /* Read byte */
        aucInputBuffer_[ulNextWritePos_] = static_cast<unsigned char>(clSerial.read());

        /* Get the next index to write bytes */
        ulNextWritePos_ = (ulNextWritePos_ + 1) % INPUT_BUFFER_LENGTH_UL;
    }  

    //Serial.println("next read pos: " + (String)ulNextReadPos_);
    //Serial.println("next write pos: " + (String)ulNextWritePos_);
    //Serial.println("Remaining bytes: " + (String)ulGetNumRemainingBytes(ulNextReadPos_));

    /* Find the start of a message. The stop condition is that there are less bytes available 
    than the size of a header */
    while( ulGetNumRemainingBytes(ulNextReadPos_) > sizeof(MsgHeader_st) ) 
    {
        /* Create temporary next reading position. If a complete message is not read, ignore
        all read positions incremented. Next time, when more bytes are received, try to read them
        again and see if the message is now complete */
        unsigned int ulTempNextReadPos_ = ulNextReadPos_;

        /* Check if the message preamble has been found */
        if (ullReadUint32FromBuffer(ulTempNextReadPos_) == MESSAGE_PREAMBLE_ULL)
        {
            /* If the preamble was found, try to read the full header */
            MsgHeader_st stMsgHeader = {};
            unsigned char aucHeaderBuffer[sizeof(MsgHeader_st)];
            for (unsigned int ulHeaderByte = 0; ulHeaderByte < sizeof(MsgHeader_st); ulHeaderByte++)
            {
                aucHeaderBuffer[ulHeaderByte] = aucInputBuffer_[ulTempNextReadPos_];
                ulTempNextReadPos_ = (ulTempNextReadPos_ + 1) % INPUT_BUFFER_LENGTH_UL;
            }
            memcpy(&stMsgHeader, &aucHeaderBuffer[0], sizeof(MsgHeader_st));

            // Serial.println("------header----");
            // Serial.println("Id: " + (String)stMsgHeader.eId);
            // Serial.println("Length: " + (String)stMsgHeader.ulLength);
            // Serial.println("preamble: " + (String)stMsgHeader.ullPreable);
            // Serial.println("----------");

            /* If the header is valid, check if the full message can be read from the buffer */
            if (bCheckHeader(stMsgHeader))
            {
                /* Header has already been read, so do not expect these bytes */
                if (ulGetNumRemainingBytes(ulTempNextReadPos_) >= stMsgHeader.ulLength - sizeof(MsgHeader_st)) 
                {
                    /* Copy those bytes to the output buffer */
                    for (unsigned int ulMsgByte = 0; 
                         ulMsgByte < stMsgHeader.ulLength - sizeof(MsgHeader_st) - NUM_CHECKSUM_BYTES_UC; 
                         ulMsgByte++)
                    {
                        pucMessage[ulMsgByte] = aucInputBuffer_[ulTempNextReadPos_];
                        ulTempNextReadPos_ = (ulTempNextReadPos_ + 1) % INPUT_BUFFER_LENGTH_UL;
                        Serial.print(aucInputBuffer_[ulTempNextReadPos_], HEX);
                        Serial.print(" ");
                    }
                    Serial.println();
                    ulMsgLength = stMsgHeader.ulLength;
                    //pucMessage = &aucInputBuffer_[sizeof(MsgHeader_st)];

                    /* Check message validity */
                    unsigned char aucChecksumBytes[NUM_CHECKSUM_BYTES_UC];
                    for (unsigned int ulByte = 0; ulByte < NUM_CHECKSUM_BYTES_UC; ulByte++)
                    {
                        aucChecksumBytes[ulByte] = aucInputBuffer_[ulTempNextReadPos_];
                        ulTempNextReadPos_ = (ulTempNextReadPos_ + 1) % INPUT_BUFFER_LENGTH_UL;
                    }
                    int32_t ulReceivedChecksum = 0;
                    memcpy(&ulReceivedChecksum, &aucChecksumBytes, NUM_CHECKSUM_BYTES_UC);
                    bMsgFound = bCheckInputMsg(pucMessage, 
                                               ulMsgLength - sizeof(MsgHeader_st) - NUM_CHECKSUM_BYTES_UC, 
                                               ulReceivedChecksum);
                    eMsgId = stMsgHeader.eId;

                    /* If the message was read, update the real counter */
                    ulNextReadPos_ = ulTempNextReadPos_;

                    break;
                }
                /* Otherwise, do not update the reading position. Break this while and wait to 
                receive more bytes */
                else
                {
                    break;
                }
            }
            /* Header is not valid, go to next byte */
            else
            {
                /* Update the next position to be read */
                ulNextReadPos_ = (ulNextReadPos_ + 1) % INPUT_BUFFER_LENGTH_UL;
            }
        }
        /* This is not a preamble, so go to next byte */
        else
        {
            /* Update the next position to be read */
            ulNextReadPos_ = (ulNextReadPos_ + 1) % INPUT_BUFFER_LENGTH_UL;
        }
    }

    return bMsgFound;
}

/****************************************** FUNCTION *******************************************//**
* \brief This function checks if an input message has a valid CRC
* \param[in] pucBuffer: Input array of bytes that compose the message (body only)
* \param[in] ulLength: length of bytes that compose the message (body only)
* \param[in] ulReceivedChecksum: checksum received in message
* \return Boolean indicating if the header have been succesfully parsed
***************************************************************************************************/
bool CommsManager_cl::bCheckInputMsg(const unsigned char* pucBuffer, 
                                     const unsigned int   ulLength,
                                     const int32_t        ulReceivedChecksum)
{
    /* Declare a status variable */
    bool bStatus = true;

    /* Check the CRC is correct for the received data */
    if (bStatus)
    {
        /* Computed checksum for the actual received data */
        int32_t slExpectedChecksum = slCRC(pucBuffer, ulLength);

        /* Check if both checksums match */
        bStatus &= ulReceivedChecksum == slExpectedChecksum;
    }

    return bStatus;
}

/****************************************** FUNCTION *******************************************//**
* \brief This function gives the number of bytes remaining to reach the last written byte. The 
* input position is counted
* \param[in] ulPos: Starting position from where to read
* return Number of remaining bytes
***************************************************************************************************/
unsigned int CommsManager_cl::ulGetNumRemainingBytes(const unsigned int ulPos)
{
    /* Declare output variable */
    unsigned int ulNumBytes = 0;

    /* Check the number of bytes from the position to the end */ 
    if (ulPos <= ulNextWritePos_)
    {
        ulNumBytes = ulNextWritePos_ - ulPos;
    }
    else
    {
        ulNumBytes = INPUT_BUFFER_LENGTH_UL + ulNextWritePos_ - ulPos;
    }

    return ulNumBytes;
}

/****************************************** FUNCTION *******************************************//**
* \brief This function reads an unsigned int from the buffer, starting at a given position
* \param[in] ulPos: Starting position from where to read
* return Unsigned int corresponding to the 4 bytes read
***************************************************************************************************/
unsigned long CommsManager_cl::ullReadUint32FromBuffer(const unsigned int ulPos)
{
    /* Get the index of the three following bytes */
    unsigned int ulPos2 = (ulPos + 1) % INPUT_BUFFER_LENGTH_UL;
    unsigned int ulPos3 = (ulPos + 2) % INPUT_BUFFER_LENGTH_UL;
    unsigned int ulPos4 = (ulPos + 3) % INPUT_BUFFER_LENGTH_UL;

    unsigned long ullOutput = 
            aucInputBuffer_[ulPos] | (uint32_t)aucInputBuffer_[ulPos2] << 8 |
            (uint32_t)aucInputBuffer_[ulPos3] << 16 | (uint32_t)aucInputBuffer_[ulPos4] << 24;
            
    return ullOutput;
}

/****************************************** FUNCTION *******************************************//**
* \brief This function CRC checksum. This is a custom CRC of 32 bits. It performs XOR of all bytes, 
* in 4 groups. Bytes 0, 4, 8... define byte 0 of the CRC, bytes 1, 5, 9... define byte 1 of CRC, 
* and so on.
* \param[in] pucBuffer: Input array of bytes to compute CRC
* \param[in] ulLength: Lngth of the buffer
* \return CRC checksum
***************************************************************************************************/
int32_t CommsManager_cl::slCRC(const unsigned char* pucBuffer, const int ulLength)
{  
    /* Declare temporal variable to compute CRC */
    unsigned char aucCRCArray[NUM_CHECKSUM_BYTES_UC];

    /* Iterate all bytes of the CRC */
    for (unsigned char ucCRCByte = 0; ucCRCByte < NUM_CHECKSUM_BYTES_UC; ucCRCByte++)
    {
        /* Initialize this byte of the CRC */
        aucCRCArray[ucCRCByte] = 0;

        /* Iterate all bytes of the buffer, jumping in chunks of 4 bytes */
        for (unsigned short usBufferByte = ucCRCByte; 
             usBufferByte < ulLength; 
             usBufferByte += NUM_CHECKSUM_BYTES_UC)
        {
            aucCRCArray[ucCRCByte] ^= pucBuffer[usBufferByte];
        }
    }

    /* Convert from buffer to int32_t */
    int32_t slCRC = 0;  
    memcpy(&slCRC, aucCRCArray, NUM_CHECKSUM_BYTES_UC);
      
    /* Note, this number has low and high bytes swapped, so use it accordingly (or swap bytes) */
    return slCRC;  
}

/****************************************** FUNCTION *******************************************//**
* \brief This function check if a message header is valid
* \param[in] stMsgHeader: Header to be checked
* \return Validity of the message header
****************************************************************************************************/
bool CommsManager_cl::bCheckHeader(const MsgHeader_st& stMsgHeader)
{
    /* Initialize output variable */
    bool bValid = true;

    /* Check the preamble */
    bValid &= stMsgHeader.ullPreable == MESSAGE_PREAMBLE_ULL;

    /* Check if the message ID is valid */
    bValid &= stMsgHeader.eId < MESSAGEID_COUNT;

    /* Check that the message length is less than the total buffer size */
    bValid &= stMsgHeader.ulLength < INPUT_BUFFER_LENGTH_UL;

    return bValid;
}