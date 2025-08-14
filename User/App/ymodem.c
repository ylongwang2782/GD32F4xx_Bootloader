#include "ymodem.h"
#include "usart.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

/* External UART handle */
extern UART_HandleTypeDef huart4;

/* Static functions */
static HAL_StatusTypeDef uart_receive_byte(uint8_t *data, uint32_t timeout);
static HAL_StatusTypeDef uart_transmit_byte(uint8_t data);
static ymodem_result_t receive_packet(uint8_t *packet, uint16_t *packet_size);
static ymodem_result_t parse_header_packet(uint8_t *packet, ymodem_file_info_t *file_info);
static HAL_StatusTypeDef flash_erase_app_area(void);
static HAL_StatusTypeDef flash_write_data(uint32_t address, uint8_t *data, uint32_t size);

/**
 * @brief Calculate CRC16 for YMODEM protocol
 * @param data: pointer to data buffer
 * @param length: data length
 * @retval CRC16 value
 */
uint16_t crc16_calc(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0x0000;
    uint16_t polynomial = 0x1021; /* CRC-CCITT polynomial */
    
    for (uint16_t i = 0; i < length; i++) {
        crc ^= (uint16_t)(data[i] << 8);
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ polynomial;
            } else {
                crc <<= 1;
            }
        }
    }
    
    return crc;
}

/**
 * @brief Receive a single byte from UART with timeout
 * @param data: pointer to store received byte
 * @param timeout: timeout in milliseconds
 * @retval HAL status
 */
static HAL_StatusTypeDef uart_receive_byte(uint8_t *data, uint32_t timeout)
{
    return HAL_UART_Receive(&huart4, data, 1, timeout);
}

/**
 * @brief Transmit a single byte via UART
 * @param data: byte to transmit
 * @retval HAL status
 */
static HAL_StatusTypeDef uart_transmit_byte(uint8_t data)
{
    return HAL_UART_Transmit(&huart4, &data, 1, 100);
}

/**
 * @brief Receive YMODEM packet
 * @param packet: buffer to store packet data
 * @param packet_size: pointer to store packet size
 * @retval YMODEM result
 */
static ymodem_result_t receive_packet(uint8_t *packet, uint16_t *packet_size)
{
    uint8_t header[3];
    uint8_t crc_bytes[2];
    uint16_t calculated_crc, received_crc;
    uint32_t data_size;
    
    /* Receive packet header (SOH/STX + packet number + ~packet number) */
    for (int i = 0; i < 3; i++) {
        if (uart_receive_byte(&header[i], YMODEM_TIMEOUT_MS) != HAL_OK) {
            return YMODEM_ERROR_TIMEOUT;
        }
    }
    
    /* Check start byte and determine packet size */
    if (header[0] == SOH) {
        data_size = YMODEM_PACKET_SIZE_128;
    } else if (header[0] == STX) {
        data_size = YMODEM_PACKET_SIZE_1024;
    } else if (header[0] == EOT) {
        *packet_size = 0;
        return YMODEM_OK; /* End of transmission */
    } else {
        return YMODEM_ERROR_PACKET;
    }
    
    /* Verify packet number */
    if (header[1] != (uint8_t)(~header[2])) {
        return YMODEM_ERROR_PACKET;
    }
    
    /* Store header in packet buffer */
    memcpy(packet, header, 3);
    
    /* Receive data */
    for (uint32_t i = 0; i < data_size; i++) {
        if (uart_receive_byte(&packet[3 + i], YMODEM_TIMEOUT_MS) != HAL_OK) {
            return YMODEM_ERROR_TIMEOUT;
        }
    }
    
    /* Receive CRC */
    for (int i = 0; i < 2; i++) {
        if (uart_receive_byte(&crc_bytes[i], YMODEM_TIMEOUT_MS) != HAL_OK) {
            return YMODEM_ERROR_TIMEOUT;
        }
    }
    
    /* Calculate and verify CRC */
    calculated_crc = crc16_calc(&packet[3], data_size);
    received_crc = (crc_bytes[0] << 8) | crc_bytes[1];
    
    if (calculated_crc != received_crc) {
        return YMODEM_ERROR_CRC;
    }
    
    *packet_size = 3 + data_size + 2;
    return YMODEM_OK;
}

/**
 * @brief Parse YMODEM header packet to extract file information
 * @param packet: header packet data
 * @param file_info: pointer to file info structure
 * @retval YMODEM result
 */
static ymodem_result_t parse_header_packet(uint8_t *packet, ymodem_file_info_t *file_info)
{
    char *ptr;
    uint8_t *data = &packet[3]; /* Skip header bytes */
    
    /* Extract filename */
    strncpy(file_info->filename, (char*)data, sizeof(file_info->filename) - 1);
    file_info->filename[sizeof(file_info->filename) - 1] = '\0';
    
    /* Check if it's an empty filename (end of batch) */
    if (strlen(file_info->filename) == 0) {
        return YMODEM_COMPLETE;
    }
    
    /* Find file size */
    ptr = (char*)data + strlen(file_info->filename) + 1;
    file_info->filesize = atol(ptr);
    file_info->received_bytes = 0;
    
    return YMODEM_OK;
}

/**
 * @brief Erase application area in flash
 * @retval HAL status
 */
static HAL_StatusTypeDef flash_erase_app_area(void)
{
    HAL_StatusTypeDef status;
    FLASH_EraseInitTypeDef erase_init;
    uint32_t sector_error;
    
    /* Unlock flash */
    status = HAL_FLASH_Unlock();
    if (status != HAL_OK) {
        return status;
    }
    
    /* Erase sectors from sector 2 onwards (sectors 0-1 contain bootloader) */
    erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3; /* 2.7V to 3.6V */
    erase_init.Sector = FLASH_SECTOR_2; /* Start from sector 2 (0x08008000) */
    erase_init.NbSectors = 10; /* Erase sectors 2-11 (leaving sectors 0-1 for bootloader) */
    
    status = HAL_FLASHEx_Erase(&erase_init, &sector_error);
    
    /* Lock flash */
    HAL_FLASH_Lock();
    
    return status;
}

/**
 * @brief Write data to flash
 * @param address: flash address to write
 * @param data: pointer to data buffer
 * @param size: data size in bytes
 * @retval HAL status
 */
static HAL_StatusTypeDef flash_write_data(uint32_t address, uint8_t *data, uint32_t size)
{
    HAL_StatusTypeDef status = HAL_OK;
    uint32_t write_address = address;
    uint32_t remaining = size;
    
    /* Unlock flash */
    status = HAL_FLASH_Unlock();
    if (status != HAL_OK) {
        return status;
    }
    
    /* Write data word by word (32-bit) */
    while (remaining >= 4 && status == HAL_OK) {
        uint32_t word = *(uint32_t*)data;
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, write_address, word);
        
        write_address += 4;
        data += 4;
        remaining -= 4;
    }
    
    /* Handle remaining bytes */
    if (remaining > 0 && status == HAL_OK) {
        uint32_t word = 0xFFFFFFFF; /* Default value for unwritten flash */
        memcpy(&word, data, remaining);
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, write_address, word);
    }
    
    /* Lock flash */
    HAL_FLASH_Lock();
    
    return status;
}

/**
 * @brief Main YMODEM file reception function
 * @param file_info: pointer to file info structure
 * @retval YMODEM result
 */
ymodem_result_t ymodem_receive_file(ymodem_file_info_t *file_info)
{
    uint8_t packet[1024 + 5]; /* Max packet size + header + CRC */
    uint16_t packet_size;
    ymodem_result_t result;
    uint8_t packet_number = 0;
    uint32_t write_address = APP_ADDR;
    uint8_t error_count = 0;
    bool first_packet = true;
    bool file_started __attribute__((unused)) = false;
    
    printf("\r\nBootloader ready. Send file using YMODEM...\r\n");
    
    /* Send initial CRC request and continue sending periodically */
    uart_transmit_byte('C');
    
    while (1) {  /* Wait indefinitely for YMODEM transmission */
        result = receive_packet(packet, &packet_size);
        
        if (result == YMODEM_OK) {
            if (packet_size == 0) {
                /* EOT received - end of file transmission */
                uart_transmit_byte(ACK);
                printf("\r\nReceived EOT, requesting next file...\r\n");
                
                /* Send 'C' to request the null file name packet */
                uart_transmit_byte('C');
                
                /* Receive end-of-batch packet (null filename) */
                result = receive_packet(packet, &packet_size);
                if (result == YMODEM_OK && packet_size > 3) {
                    /* Check if it's an empty filename (end of batch) */
                    if (packet[3] == 0x00) {
                        /* Empty filename indicates end of batch */
                        uart_transmit_byte(ACK);
                        printf("\r\nFile transfer completed successfully!\r\n");
                        printf("Filename: %s\r\n", file_info->filename);
                        printf("Size: %u bytes\r\n", (unsigned int)file_info->filesize);
                        return YMODEM_OK;
                    } else {
                        /* Another file is being sent - not supported in this implementation */
                        uart_transmit_byte(ACK);
                        printf("\r\nMultiple files not supported.\r\n");
                        return YMODEM_COMPLETE;
                    }
                } else {
                    /* Error receiving end packet or timeout */
                    printf("\r\nError receiving end-of-batch packet, but file transfer appears complete\r\n");
                    uart_transmit_byte(ACK);
                    return YMODEM_OK;
                }
            }
            
            if (first_packet) {
                /* Parse header packet */
                result = parse_header_packet(packet, file_info);
                if (result == YMODEM_COMPLETE) {
                    uart_transmit_byte(ACK);
                    return YMODEM_COMPLETE;
                } else if (result == YMODEM_OK) {
                    printf("\r\nReceiving file: %s (%u bytes)\r\n", 
                           file_info->filename, (unsigned int)file_info->filesize);
                    
                    /* Erase application area */
                    printf("Erasing flash...\r\n");
                    if (flash_erase_app_area() != HAL_OK) {
                        uart_transmit_byte(CAN);
                        return YMODEM_ERROR_FLASH;
                    }
                    
                    uart_transmit_byte(ACK);
                    uart_transmit_byte('C'); /* Request first data packet */
                    first_packet = false;
                    file_started = true;
                    packet_number = 1;
                }
            } else {
                /* Data packet */
                uint8_t expected_packet_num = packet_number;
                uint8_t received_packet_num = packet[1];
                
                if (received_packet_num == expected_packet_num) {
                    /* Correct packet number */
                    uint32_t data_size = (packet[0] == SOH) ? 128 : 1024;
                    uint32_t bytes_to_write = data_size;
                    
                    /* Calculate actual bytes to write for last packet */
                    if (file_info->received_bytes + data_size > file_info->filesize) {
                        bytes_to_write = file_info->filesize - file_info->received_bytes;
                    }
                    
                    /* Write to flash */
                    if (bytes_to_write > 0) {
                        if (flash_write_data(write_address, &packet[3], bytes_to_write) != HAL_OK) {
                            uart_transmit_byte(CAN);
                            return YMODEM_ERROR_FLASH;
                        }
                        
                        write_address += bytes_to_write;
                        file_info->received_bytes += bytes_to_write;
                        
                        /* Print progress */
                        if (file_info->received_bytes % 8192 == 0 || 
                            file_info->received_bytes >= file_info->filesize) {
                            printf("Progress: %u/%u bytes (%.1f%%)\r\n", 
                                   (unsigned int)file_info->received_bytes, (unsigned int)file_info->filesize,
                                   (float)file_info->received_bytes * 100.0f / file_info->filesize);
                        }
                    }
                    
                    uart_transmit_byte(ACK);
                    packet_number++;
                    error_count = 0;
                } else if (received_packet_num == (uint8_t)(expected_packet_num - 1)) {
                    /* Duplicate packet, acknowledge and continue */
                    uart_transmit_byte(ACK);
                } else {
                    /* Wrong packet number */
                    uart_transmit_byte(NAK);
                    error_count++;
                }
            }
        } else {
            /* Error occurred - send NAK and continue waiting */
            if (result == YMODEM_ERROR_TIMEOUT) {
                /* On timeout, send 'C' periodically to request transmission start */
                if (first_packet) {
                    uart_transmit_byte('C');
                    printf(".");  /* Show we're waiting */
                } else {
                    /* During file transfer, send NAK on timeout */
                    uart_transmit_byte(NAK);
                }
            } else {
                /* On other errors, send NAK */
                uart_transmit_byte(NAK);
                error_count++;
                
                /* Only exit on excessive consecutive errors (not timeout) */
                if (error_count >= 50) {  /* Allow more errors before giving up */
                    uart_transmit_byte(CAN);
                    return result;
                }
            }
        }
    }
}

/**
 * @brief Jump to application
 */
void jump_to_application(void)
{
    typedef void (*app_func_t)(void);
    
    uint32_t app_stack_ptr = *(volatile uint32_t*)APP_ADDR;
    uint32_t app_entry_point = *(volatile uint32_t*)(APP_ADDR + 4);
    
    /* Improved application validation with dynamic RAM size */
    bool stack_valid = (app_stack_ptr >= RAM_START_ADDR) && (app_stack_ptr <= RAM_END_ADDR);
    bool reset_valid = (app_entry_point >= APP_ADDR) && (app_entry_point < (APP_ADDR + 0x200000)) && ((app_entry_point & 0x1) == 0x1);
    bool not_empty = (app_stack_ptr != 0xFFFFFFFF) && (app_entry_point != 0xFFFFFFFF);
    
    if (stack_valid && reset_valid && not_empty) {
        printf("\r\nJumping to application at 0x%08X...\r\n", (unsigned int)APP_ADDR);
        printf("Stack Pointer: 0x%08X\r\n", (unsigned int)app_stack_ptr);
        printf("Entry Point: 0x%08X\r\n", (unsigned int)app_entry_point);
        
        /* Validate entry point is in application range */
        if (app_entry_point < APP_ADDR || app_entry_point > (APP_ADDR + 0x1F0000)) {
            printf("WARNING: Entry point outside application range!\r\n");
            printf("Expected range: 0x%08X - 0x%08X\r\n", APP_ADDR, APP_ADDR + 0x1F0000);
        }
        
        /* Give some time for UART transmission to complete */
        HAL_Delay(100);
        
        /* Disable all interrupts */
        __disable_irq();
        
        /* Deinitialize peripherals */
        HAL_UART_DeInit(&huart4);
        
        /* Reset all peripherals and clocks to default state */
        HAL_RCC_DeInit();
        
        /* Disable SysTick */
        SysTick->CTRL = 0;
        SysTick->LOAD = 0;
        SysTick->VAL = 0;
        
        /* Clear any pending interrupts */
        for (int i = 0; i < 8; i++) {
            NVIC->ICER[i] = 0xFFFFFFFF;
            NVIC->ICPR[i] = 0xFFFFFFFF;
        }
        
        /* Set vector table location */
        SCB->VTOR = APP_ADDR;
        
        /* Set stack pointer */
        __set_MSP(app_stack_ptr);
        
        /* Jump to application */
        app_func_t app_func = (app_func_t)app_entry_point;
        app_func();
    } else {
        printf("No valid application found at 0x%08X\r\n", (unsigned int)APP_ADDR);
        printf("Stack Pointer value: 0x%08X (expected range: 0x%08X-0x%08X)\r\n", 
               (unsigned int)app_stack_ptr, RAM_START_ADDR, RAM_END_ADDR);
    }
}
