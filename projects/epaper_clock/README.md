# ESP32 墨水屏桌面时钟

基于 ESP32 + 5.83 寸电子墨水屏的桌面时钟，显示实时时间、日期和天气信息。

## 效果预览

![preview](preview_small.png)

## 功能

- **实时时钟** — 大号数字 HH:MM:SS，每秒局部刷新
- **日期显示** — 中文格式「yyyy年M月d日 周X」
- **天气信息** — 自动获取天气描述、温度、湿度、风向风力
- **天气图标** — 根据天气状况绘制对应图标（晴/多云/阴/雨/雪/雾）
- **定期更新** — 天气 10 分钟刷新，NTP 每小时校准，全屏 10 分钟清残影
- **Logo 展示** — 底部居中显示自定义 Logo 位图

## 硬件

| 组件 | 型号 |
|------|------|
| 主控 | ESP32 Dev Module (CH343 USB) |
| 屏幕 | Waveshare 5.83inch e-Paper V2 (648×480, 黑白) |

**SPI 引脚**

| 信号 | GPIO |
|------|------|
| SCK  | 13   |
| MOSI | 14   |
| CS   | 15   |
| DC   | 27   |
| RST  | 26   |
| BUSY | 25   |

## 文件结构

```
epaper_clock/
├── epaper_clock.ino          # 主固件
├── Font24CN_Clock.h/.cpp     # 32×32 中文字模 (53 字)
├── DigitData.h/.cpp          # 大号数字位图 0-9 (84×148)
├── Ascii32Data.h/.cpp        # 32×32 ASCII 数字符号位图
├── ImageData.h/.cpp          # 全屏底图 C 数组（备用）
├── LogoOnly.h/.cpp           # Logo 区域位图 (500×107)
└── preview_small.png         # 效果预览图
```

## 依赖

| 库 | 说明 |
|----|------|
| waveshare-e-Paper | 墨水屏驱动 |
| ArduinoJson | JSON 解析（天气 API） |
| ESP32 Core | Arduino ESP32 核心 |

## 使用

1. 修改 `epaper_clock.ino` 顶部的 WiFi 配置和天气 API Key：

```cpp
const char *WIFI_SSID = "你的WiFi名";
const char *WIFI_PASS = "你的WiFi密码";
const char *WX_KEY    = "你的uapis.cn Key";
```

2. 编译烧录：

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 -u -p /dev/ttyACM0
```

3. 串口监视：

```bash
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

## 天气 API

使用 [uapis.cn](https://uapis.cn) 天气接口，默认城市为增城（广州），可在代码中修改：

```cpp
const char *WX_PATH = "/api/v1/misc/weather?city=你的城市";
```

## 自定义

| 需求 | 方法 |
|------|------|
| 修改城市 | 改 `WX_PATH` 中的城市参数 |
| 修改刷新间隔 | 改 `WX_INTERVAL` / `FULL_INTERVAL` / `NTP_INTERVAL` |
| 修改数字大小 | 需通过资源生成脚本重新生成位图 |
| 添加中文字符 | 需在字模生成脚本中添加并重新编译 |
| 替换 Logo | 替换位图数据文件 |

## 注意事项

- 本项目使用 **5.83inch e-Paper V2**（648×480），不兼容老款 600×448 版本
- 全屏刷新和局部刷新的时间坐标必须一致，否则会出现重影
- 中文显示依赖预生成的字模文件，缺失的汉字会显示为空白
- 固件占用约 1.12MB / 1.31MB（86% Flash）

## License

MIT
