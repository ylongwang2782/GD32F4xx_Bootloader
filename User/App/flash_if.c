/**
 ******************************************************************************
 * @file    IAP/IAP_Main/Src/flash_if.c
 * @author  MCD Application Team
 * @brief   This file provides all the memory related operation functions.
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
#include "flash_if.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
static uint32_t GetSector(uint32_t address);

/* Private functions ---------------------------------------------------------*/

/**
 * @brief  Unlocks Flash for write access
 * @param  None
 * @retval None
 */
void FlashIfInit(void)
{
    HAL_FLASH_Unlock();

    /* Clear pending flags (if any) */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR |
                           FLASH_FLAG_PGPERR);
}

/**
 * @brief  This function does an erase of all user flash area
 * @param  StartSector: start of user flash area
 * @retval 0: user flash area successfully erased
 *         1: error occurred
 */
uint32_t FlashIfErase(uint32_t startSector)
{
    uint32_t sectorError;
    FLASH_EraseInitTypeDef pEraseInit;

    /* Unlock the Flash to enable the flash control register access *************/
    FlashIfInit();

    /* Get the sector where start the user flash area */
    const uint32_t userStartSector = GetSector(APPLICATION_ADDRESS);

    pEraseInit.TypeErase = TYPEERASE_SECTORS;
    pEraseInit.Sector = userStartSector;
    pEraseInit.NbSectors = 24 - userStartSector; /* Erase from start sector to the end */
    pEraseInit.VoltageRange = VOLTAGE_RANGE_3;

    if (HAL_FLASHEx_Erase(&pEraseInit, &sectorError) != HAL_OK)
    {
        /* Error occurred while page erase */
        return (1);
    }

    return (0);
}

/**
 * @brief  This function writes a data buffer in flash (data are 32-bit aligned).
 * @note   After writing data buffer, the flash content is checked.
 * @param  FlashAddress: start address for writing data buffer
 * @param  Data: pointer on data buffer
 * @param  DataLength: length of data buffer (unit is 32-bit word)
 * @retval 0: Data successfully written to Flash memory
 *         1: Error occurred while writing data in Flash memory
 *         2: Written Data in flash memory is different from expected one
 */
uint32_t FlashIfWrite(uint32_t flashAddress, uint32_t *data, uint32_t dataLength)
{
    uint32_t i = 0;

    for (i = 0; (i < dataLength) && (flashAddress <= (USER_FLASH_END_ADDRESS - 4)); i++)
    {
        /* Device voltage range supposed to be [2.7V to 3.6V], the operation will
           be done by word */
        if (HAL_FLASH_Program(TYPEPROGRAM_WORD, flashAddress, *(uint32_t *)(data + i)) == HAL_OK)
        {
            /* Check the written value */
            if (*(uint32_t *)flashAddress != *(uint32_t *)(data + i))
            {
                /* Flash content doesn't match SRAM content */
                return (FLASHIF_WRITINGCTRL_ERROR);
            }
            /* Increment FLASH destination address */
            flashAddress += 4;
        }
        else
        {
            /* Error occurred while writing data in Flash memory */
            return (FLASHIF_WRITING_ERROR);
        }
    }

    return (FLASHIF_OK);
}

/**
 * @brief  Returns the write protection status of user flash area.
 * @param  None
 * @retval 0: No write protected sectors inside the user flash area
 *         1: Some sectors inside the user flash area are write protected
 */
uint16_t FlashIfGetWriteProtectionStatus(void)
{
    uint32_t protectedSector = 0xFFF;
    FLASH_OBProgramInitTypeDef optionsBytesStruct;

    /* Unlock the Flash to enable the flash control register access *************/
    HAL_FLASH_Unlock();

    /* Check if there are write protected sectors inside the user flash area ****/
    HAL_FLASHEx_OBGetConfig(&optionsBytesStruct);

    /* Lock the Flash to disable the flash control register access (recommended
       to protect the FLASH memory against possible unwanted operation) *********/
    HAL_FLASH_Lock();

    /* Get pages already write protected ****************************************/
    protectedSector = ~(optionsBytesStruct.WRPSector) & FLASH_SECTOR_TO_BE_PROTECTED;

    /* Check if desired pages are already write protected ***********************/
    if (protectedSector != 0)
    {
        /* Some sectors inside the user flash area are write protected */
        return FLASHIF_PROTECTION_WRPENABLED;
    }
    else
    {
        /* No write protected sectors inside the user flash area */
        return FLASHIF_PROTECTION_NONE;
    }
}

/**
 * @brief  Gets the sector of a given address for GD32F4xx (2MB Flash)
 * @param  address: Flash address
 * @retval The sector of a given address
 */
static uint32_t GetSector(uint32_t address)
{
    uint32_t sector = 0;

    if ((address < ADDR_FLASH_SECTOR_1) && (address >= ADDR_FLASH_SECTOR_0))
    {
        sector = FLASH_SECTOR_0;
    }
    else if ((address < ADDR_FLASH_SECTOR_2) && (address >= ADDR_FLASH_SECTOR_1))
    {
        sector = FLASH_SECTOR_1;
    }
    else if ((address < ADDR_FLASH_SECTOR_3) && (address >= ADDR_FLASH_SECTOR_2))
    {
        sector = FLASH_SECTOR_2;
    }
    else if ((address < ADDR_FLASH_SECTOR_4) && (address >= ADDR_FLASH_SECTOR_3))
    {
        sector = FLASH_SECTOR_3;
    }
    else if ((address < ADDR_FLASH_SECTOR_5) && (address >= ADDR_FLASH_SECTOR_4))
    {
        sector = FLASH_SECTOR_4;
    }
    else if ((address < ADDR_FLASH_SECTOR_6) && (address >= ADDR_FLASH_SECTOR_5))
    {
        sector = FLASH_SECTOR_5;
    }
    else if ((address < ADDR_FLASH_SECTOR_7) && (address >= ADDR_FLASH_SECTOR_6))
    {
        sector = FLASH_SECTOR_6;
    }
    else if ((address < ADDR_FLASH_SECTOR_8) && (address >= ADDR_FLASH_SECTOR_7))
    {
        sector = FLASH_SECTOR_7;
    }
    else if ((address < ADDR_FLASH_SECTOR_9) && (address >= ADDR_FLASH_SECTOR_8))
    {
        sector = FLASH_SECTOR_8;
    }
    else if ((address < ADDR_FLASH_SECTOR_10) && (address >= ADDR_FLASH_SECTOR_9))
    {
        sector = FLASH_SECTOR_9;
    }
    else if ((address < ADDR_FLASH_SECTOR_11) && (address >= ADDR_FLASH_SECTOR_10))
    {
        sector = FLASH_SECTOR_10;
    }
    else if ((address < ADDR_FLASH_SECTOR_12) && (address >= ADDR_FLASH_SECTOR_11))
    {
        sector = FLASH_SECTOR_11;
    }
    else if ((address < ADDR_FLASH_SECTOR_13) && (address >= ADDR_FLASH_SECTOR_12))
    {
        sector = FLASH_SECTOR_12;
    }
    else if ((address < ADDR_FLASH_SECTOR_14) && (address >= ADDR_FLASH_SECTOR_13))
    {
        sector = FLASH_SECTOR_13;
    }
    else if ((address < ADDR_FLASH_SECTOR_15) && (address >= ADDR_FLASH_SECTOR_14))
    {
        sector = FLASH_SECTOR_14;
    }
    else if ((address < ADDR_FLASH_SECTOR_16) && (address >= ADDR_FLASH_SECTOR_15))
    {
        sector = FLASH_SECTOR_15;
    }
    else if ((address < ADDR_FLASH_SECTOR_17) && (address >= ADDR_FLASH_SECTOR_16))
    {
        sector = FLASH_SECTOR_16;
    }
    else if ((address < ADDR_FLASH_SECTOR_18) && (address >= ADDR_FLASH_SECTOR_17))
    {
        sector = FLASH_SECTOR_17;
    }
    else if ((address < ADDR_FLASH_SECTOR_19) && (address >= ADDR_FLASH_SECTOR_18))
    {
        sector = FLASH_SECTOR_18;
    }
    else if ((address < ADDR_FLASH_SECTOR_20) && (address >= ADDR_FLASH_SECTOR_19))
    {
        sector = FLASH_SECTOR_19;
    }
    else if ((address < ADDR_FLASH_SECTOR_21) && (address >= ADDR_FLASH_SECTOR_20))
    {
        sector = FLASH_SECTOR_20;
    }
    else if ((address < ADDR_FLASH_SECTOR_22) && (address >= ADDR_FLASH_SECTOR_21))
    {
        sector = FLASH_SECTOR_21;
    }
    else if ((address < ADDR_FLASH_SECTOR_23) && (address >= ADDR_FLASH_SECTOR_22))
    {
        sector = FLASH_SECTOR_22;
    }
    else /* Address >= ADDR_FLASH_SECTOR_23 */
    {
        sector = FLASH_SECTOR_23;
    }
    return sector;
}

/**
 * @brief  Configure the write protection status of user flash area.
 * @param  modifier DISABLE or ENABLE the protection
 * @retval HAL_StatusTypeDef HAL_OK if change is applied.
 */
HAL_StatusTypeDef FlashIfWriteProtectionConfig(uint32_t modifier)
{
    uint32_t protectedSector = 0xFFF;
    FLASH_OBProgramInitTypeDef configNew;
    FLASH_OBProgramInitTypeDef configOld;
    HAL_StatusTypeDef result = HAL_OK;

    /* Get pages write protection status ****************************************/
    HAL_FLASHEx_OBGetConfig(&configOld);

    /* The parameter says whether we turn the protection on or off */
    configNew.WRPState = modifier;

    /* We want to modify only the Write protection */
    configNew.OptionType = OPTIONBYTE_WRP;

    /* No read protection, keep BOR and reset settings */
    configNew.RDPLevel = OB_RDP_LEVEL_0;
    configNew.USERConfig = configOld.USERConfig;
    /* Get pages already write protected ****************************************/
    protectedSector = configOld.WRPSector | FLASH_SECTOR_TO_BE_PROTECTED;

    /* Unlock the Flash to enable the flash control register access *************/
    HAL_FLASH_Unlock();

    /* Unlock the Options Bytes *************************************************/
    HAL_FLASH_OB_Unlock();

    configNew.WRPSector = protectedSector;
    result = HAL_FLASHEx_OBProgram(&configNew);

    return result;
}

/**
 * @}
 */
