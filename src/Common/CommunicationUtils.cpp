#ifndef COMMUNICATION_UTILS_CPP_
#define COMMUNICATION_UTILS_CPP_

/******************************************** INCLUDES ********************************************/
/* System includes */

/* Custom includes */
#include "CommunicationsUtils.h"


/****************************************** FUNCTION *******************************************//**
* \brief This function checks if an input message has a valid CRC and reads its header
* \param[in] buffer: Input array of bytes that compose the message
* \param[in] length: Number of received bytes
* \param[out] msgHeader: Deserialized message header
* \return Boolean indicating if the received message meets the criteria to be a valid message
***************************************************************************************************/
bool bCheckInputMsg(const unsigned char* buffer, const unsigned int length, MsgHeader_st &msgHeader)
{
    /* Declare a status variable */
    bool bStatus = true;

    /* The minimum message size is the size of the header plus the size of the checksum */
    bStatus &= length > sizeof(MsgHeader_st) + NUM_CHECKSUM_BYTES;

    if (bStatus)
    {
        /* Read the header */
        memcpy(&msgHeader, buffer, sizeof(MsgHeader_st));

        /* Check the message size defined in the header is compatible with the length of the input 
        buffer */ 
        bStatus &= length == msgHeader.length;
    }

    /* Check the CRC is correct for the received data */
    if (bStatus)
    {
        /* Computed checksum for the actual received data */
        int checksum = ModRTU_CRC(buffer + sizeof(MsgHeader_st),
                                  length - sizeof(MsgHeader_st) - NUM_CHECKSUM_BYTES);

        /* Checksum contained in the message */
        int expectedChecksum = 0;
        memcpy(&expectedChecksum, buffer + (msgHeader.length - NUM_CHECKSUM_BYTES), NUM_CHECKSUM_BYTES);

        /* Check if both checksums match */
        bStatus &= checksum == expectedChecksum;
    }

    return bStatus;
}

/****************************************** FUNCTION *******************************************//**
* \brief This function CRC checksum
* \param[in] buf: Input array of bytes to compute CRC
* \param[in] len: Lngth of the buffer
* \return CRC checksum
* \note https://stackoverflow.com/questions/17474223/getting-the-crc-checksum-of-a-byte-array-and-adding-it-to-that-byte-array
***************************************************************************************************/
int ModRTU_CRC(const unsigned char* buf, const int len)
{
  int crc = 0xFFFF;

  for (int pos = 0; pos < len; pos++) {
    crc ^= (int)buf[pos] & 0xFF;      /* XOR byte into least sig. byte of crc */

    for (int i = 8; i != 0; i--) {    /* Loop over each bit */
      if ((crc & 0x0001) != 0) {      /* If the LSB is set */
        crc >>= 1;                    /* Shift right and XOR 0xA001 */
        crc ^= 0xA001;
      }
      else                            /* Else LSB is not set */
        crc >>= 1;                    /* Just shift right */
    }
  }
/* Note, this number has low and high bytes swapped, so use it accordingly (or swap bytes) */
return crc;  
}


#endif /* COMMUNICATION_UTILS_CPP_ */