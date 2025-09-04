/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : bootloader_flag.c
 * @brief          : Bootloader flag management implementation
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "bootloader_flag.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>

/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
static HAL_StatusTypeDef FlashEraseFlagSector(void);
static HAL_StatusTypeDef FlashWriteFlagData(const BootloaderFlag *flagData);
static const BootloaderFlag *GetBootloaderFlagPtr(void);

/* Private functions ---------------------------------------------------------*/

/**
 * @brief  Get pointer to bootloader flag structure in Flash
 * @retval Pointer to bootloader flag structure in Flash
 */
static const BootloaderFlag *GetBootloaderFlagPtr(void)
{
    return (const BootloaderFlag *)BOOTLOADER_FLAG_ADDRESS;
}

/**
 * @brief  Erase the Flash sector containing bootloader flag
 * @retval HAL_OK if successful, HAL_ERROR otherwise
 */
static HAL_StatusTypeDef FlashEraseFlagSector(void)
{
    FLASH_EraseInitTypeDef eraseInitStruct;
    uint32_t sectorError;

    // 解锁Flash
    HAL_StatusTypeDef status = HAL_FLASH_Unlock();
    if (status != HAL_OK)
    {
        printf("Flash unlock failed: %d\r\n", status);
        return status;
    }

    // 配置擦除参数
    eraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    eraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3; // 2.7V-3.6V
    eraseInitStruct.Sector = BOOTLOADER_FLAG_SECTOR;
    eraseInitStruct.NbSectors = 1;

    // 执行扇区擦除
    status = HAL_FLASHEx_Erase(&eraseInitStruct, &sectorError);
    if (status != HAL_OK)
    {
        printf("Flash erase failed: %d, sector error: %u\r\n", status, (unsigned int)sectorError);
        HAL_FLASH_Lock();
        return status;
    }

    // 锁定Flash
    HAL_FLASH_Lock();

    printf("Flash sector %lu erased successfully\r\n", (unsigned long)BOOTLOADER_FLAG_SECTOR);
    return HAL_OK;
}

/**
 * @brief  Write bootloader flag data to Flash
 * @param  flagData: Pointer to flag data structure
 * @retval HAL_OK if successful, HAL_ERROR otherwise
 */
static HAL_StatusTypeDef FlashWriteFlagData(const BootloaderFlag *flagData)
{
    uint32_t address = BOOTLOADER_FLAG_ADDRESS;

    // 解锁Flash
    HAL_StatusTypeDef status = HAL_FLASH_Unlock();
    if (status != HAL_OK)
    {
        printf("Flash unlock failed: %d\r\n", status);
        return status;
    }

    // 写入magic字段 (32位)
    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, flagData->MagicValue);
    if (status != HAL_OK)
    {
        printf("Flash write magic failed: %d\r\n", status);
        HAL_FLASH_Lock();
        return status;
    }

    // 写入flag字段 (32位)
    address += 4;
    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, flagData->BootFlag);
    if (status != HAL_OK)
    {
        printf("Flash write flag failed: %d\r\n", status);
        HAL_FLASH_Lock();
        return status;
    }

    // 锁定Flash
    HAL_FLASH_Lock();

    printf("Bootloader flag written to Flash at 0x%08lX\r\n", (unsigned long)BOOTLOADER_FLAG_ADDRESS);
    return HAL_OK;
}

/* Public functions ----------------------------------------------------------*/

/**
 * @brief  Set bootloader upgrade flag in Flash
 * @retval None
 */
void SetBootloaderUpgradeFlag(void)
{
    BootloaderFlag flagData;

    // 准备标志位数据
    flagData.MagicValue = BOOTLOADER_FLAG_MAGIC;
    flagData.BootFlag = BOOTLOADER_FLAG_UPGRADE;

    printf("Setting bootloader upgrade flag...\r\n");

    // 先擦除Flash扇区
    HAL_StatusTypeDef status = FlashEraseFlagSector();
    if (status != HAL_OK)
    {
        printf("Failed to erase Flash sector for bootloader flag\r\n");
        return;
    }

    // 写入标志位数据
    status = FlashWriteFlagData(&flagData);
    if (status != HAL_OK)
    {
        printf("Failed to write bootloader flag to Flash\r\n");
        return;
    }

    printf("Bootloader upgrade flag set successfully at address 0x%08lX\r\n", (unsigned long)BOOTLOADER_FLAG_ADDRESS);
}

/**
 * @brief  Clear bootloader flag in Flash
 * @retval None
 */
void ClearBootloaderFlag(void)
{

    printf("Clearing bootloader flag...\r\n");

    // 擦除整个扇区即可清除标志位
    HAL_StatusTypeDef status = FlashEraseFlagSector();
    if (status != HAL_OK)
    {
        printf("Failed to clear bootloader flag\r\n");
        return;
    }

    printf("Bootloader flag cleared successfully\r\n");
}

/**
 * @brief  Check if bootloader upgrade flag is set
 * @retval 1 if upgrade flag is set, 0 otherwise
 */
uint8_t CheckBootloaderUpgradeFlag(void)
{
    const BootloaderFlag *flagPtr = GetBootloaderFlagPtr();

    // 检查Flash中的标志位
    if (flagPtr->MagicValue == BOOTLOADER_FLAG_MAGIC && flagPtr->BootFlag == BOOTLOADER_FLAG_UPGRADE)
    {
        printf("Bootloader upgrade flag detected in Flash\r\n");
        return 1;
    }

    return 0;
}

/**
 * @brief  Trigger system reset to bootloader
 * @note   This function sets the bootloader flag and performs system reset
 * @retval None (function does not return)
 */
void TriggerSystemResetToBootloader(void)
{
    printf("Triggering system reset to bootloader...\r\n");

    // 设置bootloader升级标志位到Flash
    SetBootloaderUpgradeFlag();

    printf("System will reset now...\r\n");

    // 执行系统软复位
    NVIC_SystemReset();
}