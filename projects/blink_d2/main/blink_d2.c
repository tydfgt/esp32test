#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define BLINK_GPIO  GPIO_NUM_2   // D2 引脚 (GPIO2)

void app_main(void)
{
    // 配置 GPIO2 为输出
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    printf("Hello ESP32-WROOM-32!\n");
    printf("GPIO%d (D2) LED blinking...\n", BLINK_GPIO);

    while (1) {
        gpio_set_level(BLINK_GPIO, 1);   // 亮
        vTaskDelay(pdMS_TO_TICKS(500));   // 延时 500ms
        gpio_set_level(BLINK_GPIO, 0);   // 灭
        vTaskDelay(pdMS_TO_TICKS(500));   // 延时 500ms
    }
}
