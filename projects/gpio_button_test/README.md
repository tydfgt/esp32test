# ESP32 GPIO 按键检测示例

基础 GPIO 输入示例，演示 D18 按键检测（按键接 GND）。

## 硬件连接

```
ESP32 D18 ────[按键]──── GND
ESP32 D2  ──── 板载 LED
```

| 引脚 | 功能 |
|------|------|
| GPIO 18 | 按键输入（INPUT_PULLUP） |
| GPIO 2  | 板载 LED 输出 |

## 原理

- D18 配置为 `INPUT_PULLUP`，内部上拉电阻使引脚默认保持 HIGH
- 按键按下时 D18 与 GND 导通，引脚电平变为 LOW
- 软件防抖（50ms）+ 状态机检测按下/释放/长按

## 功能

- ✅ 按键按下/释放检测
- ✅ 软件防抖
- ✅ LED 翻转（每次按下切换）
- ✅ 按下计数
- ✅ 长按检测（>1秒）
- ✅ 串口实时输出

## 编译烧录

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 -u -p /dev/ttyUSB0 gpio_button_test.ino
```

## 串口监视

```bash
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

## 输出示例

```
===== ESP32 GPIO 按键检测示例 =====
按键: D18 → GND (按下=导通)
LED:  D2  (板载, 按下翻转)
====================================

[按下 #1] D18=LOW (导通到GND)  LED=ON
[释放 #1] D18=HIGH (上拉恢复)
[按下 #2] D18=LOW (导通到GND)  LED=OFF
[释放 #2] D18=HIGH (上拉恢复)
  ↳ 长按检测（>1秒）
```
