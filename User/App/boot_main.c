#include <stdio.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "gpio.h"
#include "ymodem.h"
#include "usart.h"
#include "bootloader_flag.h"

int boot_main(void) {
    ymodem_file_info_t file_info;
    ymodem_result_t result;
    uint32_t app_stack_ptr;
    
    printf("\r\n");
    printf("=================================\r\n");
    printf("  STM32F4xx YMODEM Bootloader\r\n");
    printf("=================================\r\n");
    printf("Application Address: 0x%08X\r\n", APP_ADDR);
    printf("\r\n");
    
    /* Check KEY1 button state on power-up first */
    if (HAL_GPIO_ReadPin(BOOT_GPIO_Port, BOOT_Pin) == GPIO_PIN_RESET) {
        /* KEY1 pressed during power-up - enter bootloader mode */
        printf("KEY1 detected during power-up\r\n");
        printf("Entering firmware update mode...\r\n");
        printf("Ready to receive firmware via YMODEM protocol\r\n");
        printf("Please send .bin file using YMODEM...\r\n");
        
        /* Wait for LED indication during bootloader mode */
        HAL_GPIO_WritePin(RUN_LED_GPIO_Port, RUN_LED_Pin, GPIO_PIN_RESET); /* LED ON */
        
        /* Start YMODEM reception */
        result = ymodem_receive_file(&file_info);
        
        switch (result) {
            case YMODEM_OK:
                printf("\r\nFirmware update successful!\r\n");
                printf("Rebooting to new application...\r\n");
                HAL_Delay(1000);
                jump_to_application();
                break;
                
            case YMODEM_COMPLETE:
                printf("\r\nNo file received.\r\n");
                break;
                
            case YMODEM_ERROR_TIMEOUT:
                printf("\r\nTimeout error during file transfer.\r\n");
                break;
                
            case YMODEM_ERROR_CRC:
                printf("\r\nCRC error during file transfer.\r\n");
                break;
                
            case YMODEM_ERROR_PACKET:
                printf("\r\nPacket error during file transfer.\r\n");
                break;
                
            case YMODEM_ERROR_FLASH:
                printf("\r\nFlash write error during file transfer.\r\n");
                break;
                
            case YMODEM_ERROR_CANCEL:
                printf("\r\nFile transfer cancelled.\r\n");
                break;
                
            default:
                printf("\r\nUnknown error during file transfer.\r\n");
                break;
        }
        
        /* After transfer attempt, stay in bootloader mode */
        printf("\r\nBootloader mode - Reset to try again\r\n");
        while (1) {
            HAL_GPIO_TogglePin(RUN_LED_GPIO_Port, RUN_LED_Pin);
            HAL_Delay(100);
        }
    } else {
        /* KEY1 not pressed - check bootloader upgrade flag in Flash */
        printf("KEY1 not pressed, checking bootloader upgrade flag...\r\n");
        
        if (check_bootloader_upgrade_flag()) {
            /* Bootloader upgrade flag detected - clear it immediately and enter bootloader mode */
            printf("Bootloader upgrade flag detected!\r\n");
            clear_bootloader_flag();
            printf("Bootloader flag cleared, entering firmware update mode...\r\n");
            printf("Ready to receive firmware via YMODEM protocol\r\n");
            printf("Please send .bin file using YMODEM...\r\n");
            
            /* Wait for LED indication during bootloader mode */
            HAL_GPIO_WritePin(RUN_LED_GPIO_Port, RUN_LED_Pin, GPIO_PIN_RESET); /* LED ON */
            
            /* Start YMODEM reception */
            result = ymodem_receive_file(&file_info);
            
            switch (result) {
                case YMODEM_OK:
                    printf("\r\nFirmware update completed successfully!\r\n");
                    printf("System will reset in 1 seconds...\r\n");
                    HAL_Delay(1000);
                    NVIC_SystemReset();
                    break;
                    
                case YMODEM_ERROR_TIMEOUT:
                    printf("\r\nTimeout waiting for file transfer\r\n");
                    break;
                    
                case YMODEM_ERROR_CRC:
                    printf("\r\nCRC error during file transfer\r\n");
                    break;
                    
                case YMODEM_ERROR_FLASH:
                    printf("\r\nFlash programming error\r\n");
                    break;
                    
                case YMODEM_ERROR_CANCEL:
                    printf("\r\nFile transfer cancelled\r\n");
                    break;
                    
                default:
                    printf("\r\nUnknown error during file transfer\r\n");
                    break;
            }
            
            /* Blink LED to indicate bootloader mode after transfer attempt */
            while (1) {
                HAL_GPIO_TogglePin(RUN_LED_GPIO_Port, RUN_LED_Pin);
                HAL_Delay(100);
            }
        }
        
        /* No upgrade flag - check for valid application and jump */
        printf("No bootloader upgrade flag, checking for valid application...\r\n");
        app_stack_ptr = *(volatile uint32_t*)APP_ADDR;
        uint32_t reset_vector = *(volatile uint32_t*)(APP_ADDR + 4);
        
        /* Improved validation logic with dynamic RAM size detection */
        bool stack_valid = (app_stack_ptr >= RAM_START_ADDR) && (app_stack_ptr <= RAM_END_ADDR);
        bool reset_valid = (reset_vector >= APP_ADDR) && (reset_vector < (APP_ADDR + 0x200000)) && ((reset_vector & 0x1) == 0x1);
        bool not_empty = (app_stack_ptr != 0xFFFFFFFF) && (reset_vector != 0xFFFFFFFF);
        
        printf("MCU RAM Configuration: %d KB (0x%08X - 0x%08X)\r\n", RAM_SIZE_KB, RAM_START_ADDR, RAM_END_ADDR);
        printf("Validation results:\r\n");
        printf("  Stack valid: %s (0x%08X)\r\n", stack_valid ? "YES" : "NO", (unsigned int)app_stack_ptr);
        printf("  Reset valid: %s (0x%08X)\r\n", reset_valid ? "YES" : "NO", (unsigned int)reset_vector);
        printf("  Not empty: %s\r\n", not_empty ? "YES" : "NO");
        
        if (stack_valid && reset_valid && not_empty) {
            printf("Valid application found, jumping to application...\r\n");
            HAL_Delay(100); /* Brief delay for message to transmit */
            jump_to_application();
        } else {
            printf("No valid application found!\r\n");
            printf("Flash content at 0x%08X:\r\n", APP_ADDR);
            printf("  Stack Pointer: 0x%08X\r\n", (unsigned int)app_stack_ptr);
            printf("  Reset Vector:  0x%08X\r\n", (unsigned int)(*(volatile uint32_t*)(APP_ADDR + 4)));
            printf("  Expected stack range: 0x%08X-0x%08X\r\n", RAM_START_ADDR, RAM_END_ADDR);
            printf("\r\nChecking other possible locations:\r\n");
            
            /* Check if application exists at old location (0x08004000) */
            uint32_t old_app_stack = *(volatile uint32_t*)0x08004000;
            if ((old_app_stack & 0x2FFE0000) == 0x20000000) {
                printf("  Found valid app at 0x08004000 (old location)\r\n");
                printf("  Please recompile application for 0x%08X\r\n", APP_ADDR);
            } else {
                printf("  No valid app at 0x08004000 either\r\n");
            }
            
            /* Check if Flash is completely empty */
            if (app_stack_ptr == 0xFFFFFFFF) {
                printf("  Flash appears empty (all 0xFF)\r\n");
            }
            
            printf("\r\nSolutions:\r\n");
            printf("1. Hold KEY1 and reset to enter bootloader mode\r\n");
            printf("2. Upload application via YMODEM\r\n");
            printf("3. Ensure application is compiled for address 0x%08X\r\n", APP_ADDR);
            
            while (1) {
                HAL_GPIO_TogglePin(RUN_LED_GPIO_Port, RUN_LED_Pin);
                HAL_Delay(1000);
            }
        }
    }
    
    return 0;
}