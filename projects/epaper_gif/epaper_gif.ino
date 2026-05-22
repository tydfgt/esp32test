/**
 * GIF 动图播放器 — 5.83inch e-Paper V2
 *
 * 在墨水屏上循环播放预转换的 GIF 帧序列。
 * 优化：只 Init_Part 一次，帧间不 Sleep，大幅减少每帧耗时。
 * 每 N 轮全刷一次清残影。
 */

#include "DEV_Config.h"
#include "EPD.h"
#include "GUI_Paint.h"
#include "GifFrames.h"
#include "soc/gpio_reg.h"
#include <stdlib.h>

// 残影清除：每播放多少轮完整动画后全刷清屏
#define CYCLES_BEFORE_CLEAR 5

// GPIO 位掩码
static const uint32_t CS_MASK   = 1UL << EPD_CS_PIN;
static const uint32_t SCK_MASK  = 1UL << EPD_SCK_PIN;
static const uint32_t MOSI_MASK = 1UL << EPD_MOSI_PIN;
static const uint32_t DC_MASK   = 1UL << EPD_DC_PIN;

static int g_frameIdx = 0;
static int g_cycleCount = 0;

// 快速 SPI 发送命令（DC=LOW）
static inline void sendCmd(uint8_t cmd) {
    WRITE_PERI_REG(GPIO_OUT_W1TC_REG, DC_MASK);   // DC LOW = command
    WRITE_PERI_REG(GPIO_OUT_W1TC_REG, CS_MASK);   // CS LOW
    for (int b = 0; b < 8; b++) {
        if (cmd & 0x80) WRITE_PERI_REG(GPIO_OUT_W1TS_REG, MOSI_MASK);
        else            WRITE_PERI_REG(GPIO_OUT_W1TC_REG, MOSI_MASK);
        cmd <<= 1;
        WRITE_PERI_REG(GPIO_OUT_W1TS_REG, SCK_MASK);
        WRITE_PERI_REG(GPIO_OUT_W1TC_REG, SCK_MASK);
    }
    WRITE_PERI_REG(GPIO_OUT_W1TS_REG, CS_MASK);   // CS HIGH
}

// 快速 SPI 发送单个数据（DC=HIGH）
static inline void sendData(uint8_t val) {
    WRITE_PERI_REG(GPIO_OUT_W1TS_REG, DC_MASK);   // DC HIGH = data
    WRITE_PERI_REG(GPIO_OUT_W1TC_REG, CS_MASK);
    for (int b = 0; b < 8; b++) {
        if (val & 0x80) WRITE_PERI_REG(GPIO_OUT_W1TS_REG, MOSI_MASK);
        else            WRITE_PERI_REG(GPIO_OUT_W1TC_REG, MOSI_MASK);
        val <<= 1;
        WRITE_PERI_REG(GPIO_OUT_W1TS_REG, SCK_MASK);
        WRITE_PERI_REG(GPIO_OUT_W1TC_REG, SCK_MASK);
    }
    WRITE_PERI_REG(GPIO_OUT_W1TS_REG, CS_MASK);
}

// 快速 SPI 批量发送数据（DC=HIGH，只切换一次 CS）
static void sendDataBurst(const uint8_t *data, uint32_t len) {
    WRITE_PERI_REG(GPIO_OUT_W1TS_REG, DC_MASK);   // DC HIGH = data
    WRITE_PERI_REG(GPIO_OUT_W1TC_REG, CS_MASK);   // CS LOW — 整块只切换一次
    for (uint32_t i = 0; i < len; i++) {
        uint8_t d = data[i];
        for (int b = 0; b < 8; b++) {
            if (d & 0x80) WRITE_PERI_REG(GPIO_OUT_W1TS_REG, MOSI_MASK);
            else          WRITE_PERI_REG(GPIO_OUT_W1TC_REG, MOSI_MASK);
            d <<= 1;
            WRITE_PERI_REG(GPIO_OUT_W1TS_REG, SCK_MASK);
            WRITE_PERI_REG(GPIO_OUT_W1TC_REG, SCK_MASK);
        }
    }
    WRITE_PERI_REG(GPIO_OUT_W1TS_REG, CS_MASK);   // CS HIGH
}

// 紧凑忙等待
static void waitBusy() {
    while (!digitalRead(EPD_BUSY_PIN)) { /* tight loop */ }
}

// 优化的局部刷新：绕过驱动的逐字节 SendData，直接用批量传输
static void fastPartial(const uint8_t *img) {
    // 坐标计算（与原 Display_Partial 一致）
    uint16_t Xs = GIF_X_OFFSET / 8;
    uint16_t Xe = (GIF_X_OFFSET + GIF_FRAME_W) / 8;
    uint16_t Ys = GIF_Y_OFFSET;
    uint16_t Ye = GIF_Y_OFFSET + GIF_FRAME_H;
    uint16_t Width = Xe - Xs;
    uint32_t imgCount = Width * (Ye - Ys);

    // VCOM 设置
    sendCmd(0x50);
    sendData(0xA9);
    sendData(0x07);

    // 部分刷新窗口设置
    sendCmd(0x91);
    sendCmd(0x90);
    sendData(((Xs * 8) >> 8) & 0xFF);
    sendData((Xs * 8) & 0xFF);
    sendData((((Xe - 1) * 8) >> 8) & 0xFF);
    sendData(((Xe - 1) * 8) & 0xFF);
    sendData((Ys >> 8) & 0xFF);
    sendData(Ys & 0xFF);
    sendData(((Ye - 1) >> 8) & 0xFF);
    sendData((Ye - 1) & 0xFF);
    sendData(0x01);

    // 批量写入图像数据
    sendCmd(0x13);
    sendDataBurst(img, imgCount);

    // 刷新显示
    sendCmd(0x12);
    delayMicroseconds(200);
    waitBusy();

    sendCmd(0x92);
}

void showFrame(int idx) {
    unsigned long t0 = millis();
    fastPartial(gGifFrames[idx]);
    Serial.printf("帧 %2d  %lu ms\n", idx, millis() - t0);
}

void fullClear() {
    Serial.println("=== 全刷清残影 ===");
    EPD_5in83_V2_Sleep();
    EPD_5in83_V2_Init();
    EPD_5in83_V2_Clear();
    EPD_5in83_V2_Init_Part();
}

void setup() {
    Serial.begin(115200);
    DEV_Module_Init();

    EPD_5in83_V2_Init();
    EPD_5in83_V2_Clear();
    EPD_5in83_V2_Init_Part();

    showFrame(0);
}

void loop() {
    g_frameIdx++;

    if (g_frameIdx >= GIF_FRAME_COUNT) {
        g_frameIdx = 0;
        g_cycleCount++;

        if (g_cycleCount >= CYCLES_BEFORE_CLEAR) {
            g_cycleCount = 0;
            fullClear();
        }
    }

    showFrame(g_frameIdx);
}
