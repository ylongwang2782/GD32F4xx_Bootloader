#include <stdbool.h>
#include <stdio.h>

#include "common.h"
#include "main.h"
#include "menu.h"

extern pFunction JumpToApplication;
extern uint32_t JumpAddress;

int boot_main(void) {
    /* Test if Key push-button on STM327x6G-EVAL RevB Board is pressed */
    if (HAL_GPIO_ReadPin(BOOT_GPIO_Port, BOOT_Pin) == GPIO_PIN_RESET) {
        /* Display main menu */
        Main_Menu();
    }
    /* Keep the user application running */
    else {
        /* Test if user code is programmed starting from address
         * "APPLICATION_ADDRESS" */
        uint32_t sp = *(__IO uint32_t *)APPLICATION_ADDRESS;
        if ((sp >= 0x20000000 && sp <= 0x2001FFFF) ||    // SRAM1
            (sp >= 0x20020000 && sp <= 0x2002FFFF) ||    // SRAM2
            (sp >= 0x200B0000 && sp <= 0x200BFFFF))      // CCM

        {
            /* Disable all interrupts */
            __disable_irq();

            /* Disable SysTick */
            SysTick->CTRL = 0;
            SysTick->LOAD = 0;
            SysTick->VAL = 0;

            /* Clear pending interrupts */
            for (int i = 0; i < 8; i++) {
                NVIC->ICER[i] = 0xFFFFFFFF;
                NVIC->ICPR[i] = 0xFFFFFFFF;
            }

            /* Jump to user application */
            JumpAddress = *(__IO uint32_t *)(APPLICATION_ADDRESS + 4);
            JumpToApplication = (pFunction)JumpAddress;
            /* Initialize user application's Stack Pointer */
            __set_MSP(*(__IO uint32_t *)APPLICATION_ADDRESS);
            JumpToApplication();
        }
    }

    while (1) {
    }
}