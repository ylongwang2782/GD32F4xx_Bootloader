#ifndef __YMODEM_H__
#define __YMODEM_H__

#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

/* YMODEM Protocol Definitions */
#define SOH                     0x01    /* Start of 128-byte data packet */
#define STX                     0x02    /* Start of 1024-byte data packet */
#define EOT                     0x04    /* End of transmission */
#define ACK                     0x06    /* Acknowledge */
#define NAK                     0x15    /* Negative acknowledge */
#define CAN                     0x18    /* Cancel */
#define CTRLZ                   0x1A    /* EOF character */

#define YMODEM_PACKET_SIZE_128  128
#define YMODEM_PACKET_SIZE_1024 1024
#define YMODEM_PACKET_HEADER    3       /* SOH/STX + packet number + ~packet number */
#define YMODEM_PACKET_TRAILER   2       /* CRC16 */

#define YMODEM_TIMEOUT_MS       3000  /* 3 second timeout for periodic 'C' transmission */
#define YMODEM_MAX_ERRORS       255            /* Large number to effectively disable error limit */

/* Application start address */
#define APP_ADDR                0x08008000

/* RAM configuration for different STM32F4 series */
#if defined(STM32F429xx) || defined(STM32F439xx)
    // #define RAM_SIZE_KB         256
    // #define RAM_END_ADDR        0x20040000  /* 256KB */
    #define RAM_SIZE_KB         768
    #define RAM_END_ADDR        0x200C0000  /* 768KB for larger variants */
#elif defined(STM32F469xx) || defined(STM32F479xx)
    #define RAM_SIZE_KB         384
    #define RAM_END_ADDR        0x20060000  /* 384KB */
#elif defined(STM32F446xx)
    #define RAM_SIZE_KB         128
    #define RAM_END_ADDR        0x20020000  /* 128KB */
#elif defined(STM32F407xx) || defined(STM32F417xx)
    #define RAM_SIZE_KB         192
    #define RAM_END_ADDR        0x20030000  /* 192KB */
#else
    /* Default configuration - can be overridden */
    #define RAM_SIZE_KB         768
    #define RAM_END_ADDR        0x200C0000  /* 768KB for larger variants */
#endif

#define RAM_START_ADDR          0x20000000

/* YMODEM Result codes */
typedef enum {
    YMODEM_OK = 0,
    YMODEM_COMPLETE = 1,
    YMODEM_ERROR_TIMEOUT = 2,
    YMODEM_ERROR_CRC = 3,
    YMODEM_ERROR_PACKET = 4,
    YMODEM_ERROR_FLASH = 5,
    YMODEM_ERROR_CANCEL = 6
} ymodem_result_t;

/* File info structure */
typedef struct {
    char filename[256];
    uint32_t filesize;
    uint32_t received_bytes;
} ymodem_file_info_t;

/* Function prototypes */
ymodem_result_t ymodem_receive_file(ymodem_file_info_t *file_info);
uint16_t crc16_calc(const uint8_t *data, uint16_t length);
void jump_to_application(void);

#endif /* __YMODEM_H__ */
