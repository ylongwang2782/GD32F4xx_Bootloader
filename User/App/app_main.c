#include <stdio.h>
#include "stm32f4xx_hal.h"
#include "gpio.h"

int app_main(void) {
    printf("app_main\r\n");
    while(1) {
        printf("app_main\r\n");
        HAL_GPIO_TogglePin(RUN_LED_GPIO_Port, RUN_LED_Pin);
        HAL_Delay(500);
    }
    return 0;
}