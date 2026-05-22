/*
 * Arduino ESP32 Blink 示例
 * 使用 Arduino 框架在 ESP32 上闪烁板载 LED (GPIO 2)
 */

#define LED_BUILTIN 2  // ESP32-WROOM-32 板载 LED 通常接在 GPIO2

void setup() {
    // 初始化串口
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== ESP32 Arduino Blink Demo ===");
    Serial.println("板载 LED (GPIO2) 开始闪烁...");

    // 设置 LED 引脚为输出
    pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
    // 点亮 LED
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println("LED ON");
    delay(1000);  // 等待 1 秒

    // 熄灭 LED
    digitalWrite(LED_BUILTIN, LOW);
    Serial.println("LED OFF");
    delay(1000);  // 等待 1 秒
}
