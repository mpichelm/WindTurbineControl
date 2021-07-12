/******************************************** INCLUDES ********************************************/
/* System includes */

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
    ulNumUsedBytes_ = 0;
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
* \param[out] pucMessage: Buffer where the message will be copied
* \param[out] ulMsgLength: Length of the read message
* \param[out] eMsgId: Id of the received message
***************************************************************************************************/
bool CommsManager_cl::bReadInputMessage(Stream&        clSerial,
                                        unsigned char* pucMessage, 
                                        unsigned int   ulMsgLength,
                                        MessageID_e    eMsgId)
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

        /* Update the number of bytes of the buffer with data to be processed */
        ulNumUsedBytes_++;
        // TODO: check that ulNumUsedBytes_ < INPUT_BUFFER_LENGTH_UL to ensure buffer has not been overrun

        /* Get the next index to write bytes */
        ulNextWritePos_ = (ulNextWritePos_ + 1) % INPUT_BUFFER_LENGTH_UL;
    }

    /* Find the start of a message. The stop condition is that there are less bytes available 
    than the size of a header */
    while( ulGetNumRemainingBytes(ulNextReadPos_) > sizeof(MsgHeader_st) ) 
    {
        /* Check if the message preamble has been found */
        if (ulReadUint32FromBuffer(ulNextReadPos_) == MESSAGE_PREAMBLE_UL)
        {
            /* If the preamble was found, try to read the full header */
            MsgHeader_st stMsgHeader = {};
            memcpy(&stMsgHeader, &aucInputBuffer_[ulNextReadPos_], sizeof(MsgHeader_st));

            /* If the header is valid, check if the full message can be read from the buffer */
            if (bCheckHeader(stMsgHeader))
            {
                if (ulGetNumRemainingBytes(ulNextReadPos_) >= stMsgHeader.ulLength)
                {
                    /* Copy those bytes to the output buffer */
                    memcpy(pucMessage, &aucInputBuffer_[ulNextReadPos_], stMsgHeader.ulLength);
                    ulMsgLength = stMsgHeader.ulLength;

                    /* Update the next position to be read */
                    ulNextReadPos_ += stMsgHeader.ulLength;
                    ulNumUsedBytes_ -= stMsgHeader.ulLength;

                    /* Set to true the boolean indicating a new message was received */
                    bMsgFound = true;
                    eMsgId = stMsgHeader.eId;
                }
                /* Otherwise, do not update the reading position. Break this while and wait to 
                receive more bytes */
                break;
            }
        }

        /* Update the next position to be read */
        ulNextReadPos_++;
        ulNumUsedBytes_--;
    }

    return true;
}

/****************************************** FUNCTION *******************************************//**
* \brief This function checks if an input message has a valid CRC and reads its header
* \param[in] pucBuffer: Input array of bytes that compose the message
* \param[in] ulLength: length of bytes that compose the message
* \param[out] stMsgHeader: header of the received message
* \return Boolean indicating if the header have been succesfully parsed
***************************************************************************************************/
bool CommsManager_cl::bCheckInputMsg(const unsigned char* pucBuffer, 
                                     const unsigned int   ulLength,
                                           MsgHeader_st&  stMsgHeader)
{
    /* Declare a status variable */
    bool bStatus = true;

    /* The minimum message size is the size of the header plus the size of the checksum */
    bStatus &= ulLength > sizeof(MsgHeader_st) + NUM_CHECKSUM_BYTES_UC;

    if (bStatus)
    {
        /* Read the header */
        memcpy(&stMsgHeader, pucBuffer, sizeof(MsgHeader_st));

        /* Check the message size defined in the header is compatible with the length of the input 
        buffer */ 
        bStatus &= ulLength == stMsgHeader.ulLength;
    }

    /* Check the CRC is correct for the received data */
    if (bStatus)
    {
        /* Computed checksum for the actual received data */
        int slExpectedChecksum = slModRTU_CRC(pucBuffer + sizeof(MsgHeader_st),
                                              ulLength - sizeof(MsgHeader_st) - NUM_CHECKSUM_BYTES_UC);

        /* Checksum contained in the message */
        int slChecksum = 0;
        memcpy(&slChecksum,
               pucBuffer + (stMsgHeader.ulLength - NUM_CHECKSUM_BYTES_UC),
               NUM_CHECKSUM_BYTES_UC);

        /* Check if both checksums match */
        bStatus &= slChecksum == slExpectedChecksum;
    }

    return bStatus;
}

/****************************************** FUNCTION *******************************************//**
* \brief This function gives the number of bytes remaining reach the last written byte. The 
* unput position is counted
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
unsigned int CommsManager_cl::ulReadUint32FromBuffer(const unsigned int ulPos)
{
    /* Get the index of the three following bytes */
    unsigned int ulPos2 = (ulPos + 1) % INPUT_BUFFER_LENGTH_UL;
    unsigned int ulPos3 = (ulPos + 2) % INPUT_BUFFER_LENGTH_UL;
    unsigned int ulPos4 = (ulPos + 3) % INPUT_BUFFER_LENGTH_UL;

    unsigned int ulOutput = 
            aucInputBuffer_[ulPos] | (uint32_t)aucInputBuffer_[ulPos2] << 8 |
            (uint32_t)aucInputBuffer_[ulPos3] << 16 | (uint32_t)aucInputBuffer_[ulPos4] << 24;
            
    return ulOutput;
}

/****************************************** FUNCTION *******************************************//**
* \brief This function CRC checksum
* \param[in] pucBuffer: Input array of bytes to compute CRC
* \param[in] ulLength: Lngth of the buffer
* \return CRC checksum
* \note https://stackoverflow.com/questions/17474223/getting-the-crc-checksum-of-a-byte-array-and-adding-it-to-that-byte-array
***************************************************************************************************/
int CommsManager_cl::slModRTU_CRC(const unsigned char* pucBuffer, const int ulLength)
{  
    int slCRC = 0xFFFF;
  
    for (int slPos = 0; slPos < ulLength; slPos++) {
      slCRC ^= (int)pucBuffer[slPos] & 0xFF;      /* XOR byte into least sig. byte of crc */
  
      for (int i = 8; i != 0; i--) {    /* Loop over each bit */
        if ((slCRC & 0x0001) != 0) {    /* If the LSB is set */
          slCRC >>= 1;                  /* Shift right and XOR 0xA001 */
          slCRC ^= 0xA001;
        }
        else                            /* Else LSB is not set */
          slCRC >>= 1;                  /* Just shift right */
      }
    }
    
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
    bValid &= stMsgHeader.ulPreable == MESSAGE_PREAMBLE_UL;

    /* Check if the message ID is valid */
    bValid &= stMsgHeader.eId < MESSAGEID_COUNT;

    /* Check that the message length is less than the total buffer size */
    bValid &= stMsgHeader.ulLength < INPUT_BUFFER_LENGTH_UL;

    return bValid;
}