/**
 ******************************************************************************
 * @file    IAP/IAP_Main/Src/ymodem.c
 * @author  MCD Application Team
 * @brief   This file provides all the software functions related to the ymodem
 *          protocol.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2017 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

/** @addtogroup STM32F7xx_IAP
 * @{
 */

/* Includes ------------------------------------------------------------------*/
#include "ymodem.h"
#include "common.h"
#include "flash_if.h"
#include "menu.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define CRC16_F /* activate the CRC16 integrity */
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
__IO uint32_t flashDestination;
/* @note ATTENTION - please keep this variable 32bit aligned */
uint8_t aPacketData[PACKET_1K_SIZE + PACKET_DATA_INDEX + PACKET_TRAILER_SIZE];

/* Private function prototypes -----------------------------------------------*/
static void PrepareIntialPacket(uint8_t *data, const uint8_t *fileName, uint32_t length);
static void PreparePacket(uint8_t *source, uint8_t *packet, uint8_t pktNr, uint32_t sizeBlk);
static HAL_StatusTypeDef ReceivePacket(uint8_t *data, uint32_t *length, uint32_t timeout);
uint16_t UpdateCRC16(uint16_t crcIn, uint8_t byte);
uint16_t CalCrC16(const uint8_t *data, uint32_t size);
uint8_t CalcChecksum(const uint8_t *data, uint32_t size);

/* Private functions ---------------------------------------------------------*/

/**
 * @brief  Receive a packet from sender
 * @param length
 * @param  data
 * @param  length
 *     0: end of transmission
 *     2: abort by sender
 *    >0: packet length
 * @param  timeout
 * @retval HAL_OK: normally return
 *         HAL_BUSY: abort by user
 */
static HAL_StatusTypeDef ReceivePacket(uint8_t *data, uint32_t *length, uint32_t timeout)
{
    uint32_t crc;
    uint32_t packet_size = 0;
    HAL_StatusTypeDef status;
    uint8_t char1;

    *length = 0;
    status = HAL_UART_Receive(&DEBUG_UART, &char1, 1, timeout);

    if (status == HAL_OK)
    {
        switch (char1)
        {
        case SOH:
            packet_size = PACKET_SIZE;
            break;
        case STX:
            packet_size = PACKET_1K_SIZE;
            break;
        case EOT:
            break;
        case CA:
            if ((HAL_UART_Receive(&DEBUG_UART, &char1, 1, timeout) == HAL_OK) && (char1 == CA))
            {
                packet_size = 2;
            }
            else
            {
                status = HAL_ERROR;
            }
            break;
        case ABORT1:
        case ABORT2:
            status = HAL_BUSY;
            break;
        default:
            status = HAL_ERROR;
            break;
        }
        *data = char1;

        if (packet_size >= PACKET_SIZE)
        {
            status =
                HAL_UART_Receive(&DEBUG_UART, &data[PACKET_NUMBER_INDEX], packet_size + PACKET_OVERHEAD_SIZE, timeout);

            /* Simple packet sanity check */
            if (status == HAL_OK)
            {
                if (data[PACKET_NUMBER_INDEX] != ((data[PACKET_CNUMBER_INDEX]) ^ NEGATIVE_BYTE))
                {
                    packet_size = 0;
                    status = HAL_ERROR;
                }
                else
                {
                    /* Check packet CRC */
                    crc = data[packet_size + PACKET_DATA_INDEX] << 8;
                    crc += data[packet_size + PACKET_DATA_INDEX + 1];
                    if (CalCrC16(&data[PACKET_DATA_INDEX], packet_size) != crc)
                    {
                        packet_size = 0;
                        status = HAL_ERROR;
                    }
                }
            }
            else
            {
                packet_size = 0;
            }
        }
    }
    *length = packet_size;
    return status;
}

/**
 * @brief  Prepare the first block
 * @param  data:  output buffer
 * @param  fileName: name of the file to be sent
 * @param  length: length of the file to be sent in bytes
 * @retval None
 */
static void PrepareIntialPacket(uint8_t *data, const uint8_t *fileName, uint32_t length)
{
    uint32_t i, j = 0;
    uint8_t astring[10];

    /* first 3 bytes are constant */
    data[PACKET_START_INDEX] = SOH;
    data[PACKET_NUMBER_INDEX] = 0x00;
    data[PACKET_CNUMBER_INDEX] = 0xff;

    /* Filename written */
    for (i = 0; (fileName[i] != '\0') && (i < FILE_NAME_LENGTH); i++)
    {
        data[i + PACKET_DATA_INDEX] = fileName[i];
    }

    data[i + PACKET_DATA_INDEX] = 0x00;

    /* file size written */
    Int2Str(astring, length);
    i = i + PACKET_DATA_INDEX + 1;
    while (astring[j] != '\0')
    {
        data[i++] = astring[j++];
    }

    /* padding with zeros */
    for (j = i; j < PACKET_SIZE + PACKET_DATA_INDEX; j++)
    {
        data[j] = 0;
    }
}

/**
 * @brief  Prepare the data packet
 * @param  source: pointer to the data to be sent
 * @param  packet: pointer to the output buffer
 * @param  pktNr: number of the packet
 * @param  sizeBlk: length of the block to be sent in bytes
 * @retval None
 */
static void PreparePacket(uint8_t *source, uint8_t *packet, uint8_t pktNr, uint32_t sizeBlk)
{
    uint8_t *record;
    uint32_t i, size, packetSize;

    /* Make first three packet */
    packetSize = sizeBlk >= PACKET_1K_SIZE ? PACKET_1K_SIZE : PACKET_SIZE;
    size = sizeBlk < packetSize ? sizeBlk : packetSize;
    if (packetSize == PACKET_1K_SIZE)
    {
        packet[PACKET_START_INDEX] = STX;
    }
    else
    {
        packet[PACKET_START_INDEX] = SOH;
    }
    packet[PACKET_NUMBER_INDEX] = pktNr;
    packet[PACKET_CNUMBER_INDEX] = (~pktNr);
    record = source;

    /* Filename packet has valid data */
    for (i = PACKET_DATA_INDEX; i < size + PACKET_DATA_INDEX; i++)
    {
        packet[i] = *record++;
    }
    if (size <= packetSize)
    {
        for (i = size + PACKET_DATA_INDEX; i < packetSize + PACKET_DATA_INDEX; i++)
        {
            packet[i] = 0x1A; /* EOF (0x1A) or 0x00 */
        }
    }
}

/**
 * @brief  Update CRC16 for input byte
 * @param  crcIn input value
 * @param  input byte
 * @retval None
 */
uint16_t UpdateCRC16(uint16_t crcIn, uint8_t byte)
{
    uint32_t crc = crcIn;
    uint32_t in = byte | 0x100;

    do
    {
        crc <<= 1;
        in <<= 1;
        if (in & 0x100)
            ++crc;
        if (crc & 0x10000)
            crc ^= 0x1021;
    }

    while (!(in & 0x10000));

    return crc & 0xffffu;
}

/**
 * @brief  Cal CRC16 for YModem Packet
 * @param  data
 * @param  length
 * @retval None
 */
uint16_t CalCrC16(const uint8_t *data, uint32_t size)
{
    uint32_t crc = 0;
    const uint8_t *dataEnd = data + size;

    while (data < dataEnd)
        crc = UpdateCRC16(crc, *data++);

    crc = UpdateCRC16(crc, 0);
    crc = UpdateCRC16(crc, 0);

    return crc & 0xffffu;
}

/**
 * @brief  Calculate Check sum for YModem Packet
 * @param  data Pointer to input data
 * @param  size length of input data
 * @retval uint8_t checksum value
 */
uint8_t CalcChecksum(const uint8_t *data, uint32_t size)
{
    uint32_t sum = 0;
    const uint8_t *p_data_end = data + size;

    while (data < p_data_end)
    {
        sum += *data++;
    }

    return (sum & 0xffu);
}

/* Public functions ---------------------------------------------------------*/
/**
 * @brief  Receive a file using the ymodem protocol with CRC16.
 * @param  size The size of the file.
 * @retval COM_StatusTypeDef result of reception/programming
 */
COM_StatusTypeDef Ymodem_Receive(uint32_t *size)
{
    uint32_t i, packetLength, sessionDone = 0, fileDone, errors = 0, sessionBegin = 0;
    // uint32_t flashdestination;
    uint32_t ramsource, filesize;
    uint8_t *filePtr;
    uint8_t file_size[FILE_SIZE_LENGTH], tmp, packetsReceived;
    COM_StatusTypeDef result = COM_OK;

    /* Initialize flashdestination variable */
    flashDestination = APPLICATION_ADDRESS;

    while ((sessionDone == 0) && (result == COM_OK))
    {
        packetsReceived = 0;
        fileDone = 0;
        while ((fileDone == 0) && (result == COM_OK))
        {
            switch (ReceivePacket(aPacketData, &packetLength, DOWNLOAD_TIMEOUT))
            {
            case HAL_OK:
                errors = 0;
                switch (packetLength)
                {
                case 2:
                    /* Abort by sender */
                    SerialPutByte(ACK);
                    result = COM_ABORT;
                    break;
                case 0:
                    /* End of transmission */
                    SerialPutByte(ACK);
                    fileDone = 1;
                    break;
                default:
                    /* Normal packet */
                    if (aPacketData[PACKET_NUMBER_INDEX] != packetsReceived)
                    {
                        SerialPutByte(NAK);
                    }
                    else
                    {
                        if (packetsReceived == 0)
                        {
                            /* File name packet */
                            if (aPacketData[PACKET_DATA_INDEX] != 0)
                            {
                                /* File name extraction */
                                i = 0;
                                filePtr = aPacketData + PACKET_DATA_INDEX;
                                while ((*filePtr != 0) && (i < FILE_NAME_LENGTH))
                                {
                                    aFileName[i++] = *filePtr++;
                                }

                                /* File size extraction */
                                aFileName[i++] = '\0';
                                i = 0;
                                filePtr++;
                                while ((*filePtr != ' ') && (i < FILE_SIZE_LENGTH))
                                {
                                    file_size[i++] = *filePtr++;
                                }
                                file_size[i++] = '\0';
                                Str2Int(file_size, &filesize);

                                /* Test the size of the image to be sent */
                                /* Image size is greater than Flash size */
                                if (*size > (USER_FLASH_SIZE + 1))
                                {
                                    /* End session */
                                    tmp = CA;
                                    RS485_TX_EN();
                                    HAL_UART_Transmit(&DEBUG_UART, &tmp, 1, NAK_TIMEOUT);
                                    HAL_UART_Transmit(&DEBUG_UART, &tmp, 1, NAK_TIMEOUT);
                                    RS485_RX_EN();
                                    result = COM_LIMIT;
                                }
                                /* erase user application area */
                                FlashIfErase(APPLICATION_ADDRESS);
                                *size = filesize;

                                SerialPutByte(ACK);
                                SerialPutByte(CRC16);
                            }
                            /* File header packet is empty, end session */
                            else
                            {
                                SerialPutByte(ACK);
                                fileDone = 1;
                                sessionDone = 1;
                                break;
                            }
                        }
                        else /* Data packet */
                        {
                            ramsource = (uint32_t)&aPacketData[PACKET_DATA_INDEX];
                            /* Write received data in Flash */
                            if (FlashIfWrite(flashDestination, (uint32_t *)ramsource, packetLength / 4) == FLASHIF_OK)
                            {
                                flashDestination += packetLength;
                                SerialPutByte(ACK);
                            }
                            else /* An error occurred while writing to Flash memory */
                            {
                                /* End session */
                                SerialPutByte(CA);
                                SerialPutByte(CA);
                                result = COM_DATA;
                            }
                        }
                        packetsReceived++;
                        sessionBegin = 1;
                    }
                    break;
                }
                break;
            case HAL_BUSY: /* Abort actually */
                SerialPutByte(CA);
                SerialPutByte(CA);
                result = COM_ABORT;
                break;
            default:
                if (sessionBegin > 0)
                {
                    errors++;
                }
                if (errors > MAX_ERRORS)
                {
                    /* Abort communication */
                    SerialPutByte(CA);
                    SerialPutByte(CA);
                }
                else
                {
                    SerialPutByte(CRC16); /* Ask for a packet */
                }
                break;
            }
        }
    }
    return result;
}

/**
 * @brief  Transmit a file using the ymodem protocol
 * @param  buf: Address of the first byte
 * @param  fileName: Name of the file sent
 * @param  fileSize: Size of the transmission
 * @retval COM_StatusTypeDef result of the communication
 */
COM_StatusTypeDef Ymodem_Transmit(uint8_t *buf, const uint8_t *fileName, uint32_t fileSize)
{
    uint32_t errors = 0, ackRecpt = 0, size = 0, pkt_size;
    uint8_t *bufInt;
    COM_StatusTypeDef result = COM_OK;
    uint32_t blkNumber = 1;
    uint8_t aRxCtrl[2];
    uint8_t i;
#ifdef CRC16_F
    uint32_t temp_crc;
#else  /* CRC16_F */
    uint8_t temp_chksum;
#endif /* CRC16_F */

    /* Prepare first block - header */
    PrepareIntialPacket(aPacketData, fileName, fileSize);

    while ((!ackRecpt) && (result == COM_OK))
    {
        /* Send Packet */
        RS485_TX_EN();
        HAL_UART_Transmit(&DEBUG_UART, &aPacketData[PACKET_START_INDEX], PACKET_SIZE + PACKET_HEADER_SIZE, NAK_TIMEOUT);
        RS485_RX_EN();

        /* Send CRC or Check Sum based on CRC16_F */
#ifdef CRC16_F
        temp_crc = CalCrC16(&aPacketData[PACKET_DATA_INDEX], PACKET_SIZE);
        SerialPutByte(temp_crc >> 8);
        SerialPutByte(temp_crc & 0xFF);
#else  /* CRC16_F */
        temp_chksum = CalcChecksum(&aPacketData[PACKET_DATA_INDEX], PACKET_SIZE);
        Serial_PutByte(temp_chksum);
#endif /* CRC16_F */

        /* Wait for Ack and 'C' */
        if (HAL_UART_Receive(&DEBUG_UART, &aRxCtrl[0], 1, NAK_TIMEOUT) == HAL_OK)
        {
            if (aRxCtrl[0] == ACK)
            {
                ackRecpt = 1;
            }
            else if (aRxCtrl[0] == CA)
            {
                if ((HAL_UART_Receive(&DEBUG_UART, &aRxCtrl[0], 1, NAK_TIMEOUT) == HAL_OK) && (aRxCtrl[0] == CA))
                {
                    HAL_Delay(2);
                    __HAL_UART_FLUSH_DRREGISTER(&DEBUG_UART);
                    result = COM_ABORT;
                }
            }
        }
        else
        {
            errors++;
        }
        if (errors >= MAX_ERRORS)
        {
            result = COM_ERROR;
        }
    }

    bufInt = buf;
    size = fileSize;

    /* Here 1024 bytes length is used to send the packets */
    while ((size) && (result == COM_OK))
    {
        /* Prepare next packet */
        PreparePacket(bufInt, aPacketData, blkNumber, size);
        ackRecpt = 0;
        aRxCtrl[0] = 0;
        errors = 0;

        /* Resend packet if NAK for few times else end of communication */
        while ((!ackRecpt) && (result == COM_OK))
        {
            /* Send next packet */
            if (size >= PACKET_1K_SIZE)
            {
                pkt_size = PACKET_1K_SIZE;
            }
            else
            {
                pkt_size = PACKET_SIZE;
            }

            RS485_TX_EN();
            HAL_UART_Transmit(&DEBUG_UART, &aPacketData[PACKET_START_INDEX], pkt_size + PACKET_HEADER_SIZE,
                              NAK_TIMEOUT);
            RS485_RX_EN();

            /* Send CRC or Check Sum based on CRC16_F */
#ifdef CRC16_F
            temp_crc = CalCrC16(&aPacketData[PACKET_DATA_INDEX], pkt_size);
            SerialPutByte(temp_crc >> 8);
            SerialPutByte(temp_crc & 0xFF);
#else  /* CRC16_F */
            temp_chksum = CalcChecksum(&aPacketData[PACKET_DATA_INDEX], pkt_size);
            Serial_PutByte(temp_chksum);
#endif /* CRC16_F */

            /* Wait for Ack */
            if ((HAL_UART_Receive(&DEBUG_UART, &aRxCtrl[0], 1, NAK_TIMEOUT) == HAL_OK) && (aRxCtrl[0] == ACK))
            {
                ackRecpt = 1;
                if (size > pkt_size)
                {
                    bufInt += pkt_size;
                    size -= pkt_size;
                    if (blkNumber == (USER_FLASH_SIZE / PACKET_1K_SIZE))
                    {
                        result = COM_LIMIT; /* boundary error */
                    }
                    else
                    {
                        blkNumber++;
                    }
                }
                else
                {
                    bufInt += pkt_size;
                    size = 0;
                }
            }
            else
            {
                errors++;
            }

            /* Resend packet if NAK  for a count of 10 else end of communication */
            if (errors >= MAX_ERRORS)
            {
                result = COM_ERROR;
            }
        }
    }

    /* Sending End Of Transmission char */
    ackRecpt = 0;
    aRxCtrl[0] = 0x00;
    errors = 0;
    while ((!ackRecpt) && (result == COM_OK))
    {
        SerialPutByte(EOT);

        /* Wait for Ack */
        if (HAL_UART_Receive(&DEBUG_UART, &aRxCtrl[0], 1, NAK_TIMEOUT) == HAL_OK)
        {
            if (aRxCtrl[0] == ACK)
            {
                ackRecpt = 1;
            }
            else if (aRxCtrl[0] == CA)
            {
                if ((HAL_UART_Receive(&DEBUG_UART, &aRxCtrl[0], 1, NAK_TIMEOUT) == HAL_OK) && (aRxCtrl[0] == CA))
                {
                    HAL_Delay(2);
                    __HAL_UART_FLUSH_DRREGISTER(&DEBUG_UART);
                    result = COM_ABORT;
                }
            }
        }
        else
        {
            errors++;
        }

        if (errors >= MAX_ERRORS)
        {
            result = COM_ERROR;
        }
    }

    /* Empty packet sent - some terminal emulators need this to close session */
    if (result == COM_OK)
    {
        /* Preparing an empty packet */
        aPacketData[PACKET_START_INDEX] = SOH;
        aPacketData[PACKET_NUMBER_INDEX] = 0;
        aPacketData[PACKET_CNUMBER_INDEX] = 0xFF;
        for (i = PACKET_DATA_INDEX; i < (PACKET_SIZE + PACKET_DATA_INDEX); i++)
        {
            aPacketData[i] = 0x00;
        }

        /* Send Packet */
        RS485_TX_EN();
        HAL_UART_Transmit(&DEBUG_UART, &aPacketData[PACKET_START_INDEX], PACKET_SIZE + PACKET_HEADER_SIZE, NAK_TIMEOUT);
        RS485_RX_EN();

        /* Send CRC or Check Sum based on CRC16_F */
#ifdef CRC16_F
        temp_crc = CalCrC16(&aPacketData[PACKET_DATA_INDEX], PACKET_SIZE);
        SerialPutByte(temp_crc >> 8);
        SerialPutByte(temp_crc & 0xFF);
#else  /* CRC16_F */
        temp_chksum = CalcChecksum(&aPacketData[PACKET_DATA_INDEX], PACKET_SIZE);
        Serial_PutByte(temp_chksum);
#endif /* CRC16_F */

        /* Wait for Ack and 'C' */
        if (HAL_UART_Receive(&DEBUG_UART, &aRxCtrl[0], 1, NAK_TIMEOUT) == HAL_OK)
        {
            if (aRxCtrl[0] == CA)
            {
                HAL_Delay(2);
                __HAL_UART_FLUSH_DRREGISTER(&DEBUG_UART);
                result = COM_ABORT;
            }
        }
    }

    return result; /* file transmitted successfully */
}

/**
 * @}
 */
