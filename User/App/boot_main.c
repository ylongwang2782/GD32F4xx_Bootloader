#include <stdbool.h>

#include "bootloader_flag.h"
#include "main.h"
#include "menu.h"

/* Private variables */
typedef void (*pFunction)(void);
pFunction jumpToApplication;
uint32_t jumpAddress;

int boot_main(void)
{
    /* First, check for software upgrade flag */
    if (CheckBootloaderUpgradeFlag())
    {
        /* Clear the upgrade flag */
        ClearBootloaderFlag();
        /* Enter bootloader menu */
        Main_Menu();
    }
    /* Then check if DIP1 and DIP2 are both pressed */
    else if (HAL_GPIO_ReadPin(DIP2_GPIO_Port, DIP2_Pin) == GPIO_PIN_RESET &&
             HAL_GPIO_ReadPin(DIP1_GPIO_Port, DIP1_Pin) == GPIO_PIN_RESET)
    {
        /* Display main menu */
        Main_Menu();
    }
    /* Keep the user application running */
    else
    {
        /* Test if user code is programmed starting from address
         * "APPLICATION_ADDRESS" */
        uint32_t sp = *(__IO uint32_t *)APPLICATION_ADDRESS;
        if ((sp >= 0x20000000 && sp <= 0x2001FFFF) || // SRAM1
            (sp >= 0x20020000 && sp <= 0x2002FFFF) || // SRAM2
            (sp >= 0x200B0000 && sp <= 0x200BFFFF))   // CCM

        {
            /* Disable all interrupts */
            __disable_irq();

            /* Disable SysTick */
            SysTick->CTRL = 0;
            SysTick->LOAD = 0;
            SysTick->VAL = 0;

            /* Clear pending interrupts */
            for (int i = 0; i < 8; i++)
            {
                NVIC->ICER[i] = 0xFFFFFFFF;
                NVIC->ICPR[i] = 0xFFFFFFFF;
            }

            /* Jump to user application */
            jumpAddress = *(__IO uint32_t *)(APPLICATION_ADDRESS + 4);
            jumpToApplication = (pFunction)jumpAddress;
            /* Initialize user application's Stack Pointer */
            __set_MSP(*(__IO uint32_t *)APPLICATION_ADDRESS);
            jumpToApplication();
        }
    }

    while (1)
    {
    }
}