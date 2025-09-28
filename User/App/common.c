/**
 ******************************************************************************
 * @file    IAP/IAP_Main/Src/common.c
 * @author  MCD Application Team
 * @brief   This file provides all the common functions.
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

/** @addtogroup STM32F7xx_IAP_Main
 * @{
 */

/* Includes ------------------------------------------------------------------*/
#include "common.h"
#include "main.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
 * @brief  Convert an Integer to a string
 * @param pStr
 * @param  p_str: The string output pointer
 * @param  intnum: The integer to be converted
 * @retval None
 */
void Int2Str(uint8_t *pStr, uint32_t intnum)
{
    uint32_t divider = 1000000000;
    uint32_t pos = 0;
    uint32_t status = 0;

    for (uint32_t i = 0; i < 10; i++)
    {
        pStr[pos++] = (intnum / divider) + 48;

        intnum = intnum % divider;
        divider /= 10;
        if ((pStr[pos - 1] == '0') & (status == 0))
        {
            pos = 0;
        }
        else
        {
            status++;
        }
    }
}

/**
 * @brief  Convert a string to an integer
 * @param  p_inputstr: The string to be converted
 * @param  pIntnum: The integer value
 * @retval 1: Correct
 *         0: Error
 */
uint32_t Str2Int(const uint8_t *pInputstr, uint32_t *pIntnum)
{
    uint32_t i = 0;
    uint32_t res = 0;
    uint32_t val = 0;

    if ((pInputstr[0] == '0') && ((pInputstr[1] == 'x') || (pInputstr[1] == 'X')))
    {
        i = 2;
        while ((i < 11) && (pInputstr[i] != '\0'))
        {
            if (ISVALIDHEX(pInputstr[i]))
            {
                val = (val << 4) + CONVERTHEX(pInputstr[i]);
            }
            else
            {
                /* Return 0, Invalid input */
                res = 0;
                break;
            }
            i++;
        }

        /* valid result */
        if (pInputstr[i] == '\0')
        {
            *pIntnum = val;
            res = 1;
        }
    }
    else /* max 10-digit decimal input */
    {
        while ((i < 11) && (res != 1))
        {
            if (pInputstr[i] == '\0')
            {
                *pIntnum = val;
                /* return 1 */
                res = 1;
            }
            else if (((pInputstr[i] == 'k') || (pInputstr[i] == 'K')) && (i > 0))
            {
                val = val << 10;
                *pIntnum = val;
                res = 1;
            }
            else if (((pInputstr[i] == 'm') || (pInputstr[i] == 'M')) && (i > 0))
            {
                val = val << 20;
                *pIntnum = val;
                res = 1;
            }
            else if (ISVALIDDEC(pInputstr[i]))
            {
                val = val * 10 + CONVERTDEC(pInputstr[i]);
            }
            else
            {
                /* return 0, Invalid input */
                res = 0;
                break;
            }
            i++;
        }
    }

    return res;
}

/**
 * @brief  Print a string on the HyperTerminal
 * @param  p_string: The string to be printed
 * @retval None
 */
void SerialPutString(const uint8_t *pString)
{
    uint16_t length = 0;

    while (pString[length] != '\0')
    {
        length++;
    }
    RS485_TX_EN();
    HAL_UART_Transmit(&DEBUG_UART, pString, length, TX_TIMEOUT);
    RS485_RX_EN();
}

/**
 * @brief  Transmit a byte to the HyperTerminal
 * @param  param The byte to be sent
 * @retval HAL_StatusTypeDef HAL_OK if OK
 */
HAL_StatusTypeDef SerialPutByte(uint8_t param)
{
    /* May be timeouted... */
    if (DEBUG_UART.gState == HAL_UART_STATE_TIMEOUT)
    {
        DEBUG_UART.gState = HAL_UART_STATE_READY;
    }
    RS485_TX_EN();
    HAL_StatusTypeDef status = HAL_UART_Transmit(&DEBUG_UART, &param, 1, TX_TIMEOUT);
    RS485_RX_EN();
    return status;
}
/**
 * @}
 */
