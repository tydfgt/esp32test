#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/gpio_types.h"
#include "driver/ledc.h"

#define LED_GPIO        GPIO_NUM_2    // D2
#define LEDC_TIMER      LEDC_TIMER_0
#define LEDC_MODE       LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL    LEDC_CHANNEL_0
#define LEDC_RESOLUTION LEDC_TIMER_13_BIT  // 13位分辨率 = 0~8191
#define LEDC_FREQ       5000               // 5kHz PWM, 不闪屏

void app_main(void)
{
    printf("ESP32-WROOM-32 GPIO2 (D2) 慢速呼吸灯\n");

    // 配置 LEDC 定时器
    ledc_timer_config_t timer_cfg = {
        .speed_mode      = LEDC_MODE,
        .duty_resolution = LEDC_RESOLUTION,
        .timer_num       = LEDC_TIMER,
        .freq_hz         = LEDC_FREQ,
        .clk_cfg         = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer_cfg);

    // 配置 LEDC 通道
    ledc_channel_config_t ch_cfg = {
        .gpio_num   = LED_GPIO,
        .speed_mode = LEDC_MODE,
        .channel    = LEDC_CHANNEL,
        .timer_sel  = LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0,
        .intr_type  = LEDC_INTR_DISABLE
    };
    ledc_channel_config(&ch_cfg);

    int max_duty = (1 << LEDC_RESOLUTION) - 1;  // 8191
    float step = 0.008f;   // 每步 0.008 弧度，约 196 步走完 π/2
    int step_ms = 25;       // 每步 25ms → 半周期 ≈ 4.9s，一呼一吸 ≈ 10s

    while (1) {
        // 渐亮 (暗→亮)
        for (float a = 0.0f; a <= M_PI_2; a += step) {
            float b = sinf(a);
            ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, (uint32_t)(b * max_duty));
            ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
            vTaskDelay(pdMS_TO_TICKS(step_ms));
        }
        vTaskDelay(pdMS_TO_TICKS(400));  // 峰值停顿 0.4s

        // 渐灭 (亮→暗)
        for (float a = M_PI_2; a >= 0.0f; a -= step) {
            float b = sinf(a);
            ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, (uint32_t)(b * max_duty));
            ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
            vTaskDelay(pdMS_TO_TICKS(step_ms));
        }
        vTaskDelay(pdMS_TO_TICKS(400));  // 谷底停顿 0.4s
    }
}
