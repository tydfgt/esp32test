# ESP32 开发项目集合

ESP32 相关开发项目，包含基础实验和墨水屏应用。

## 项目列表

| 项目 | 说明 |
| --- | --- |
| [epaper_clock](projects/epaper_clock/) | 墨水屏桌面时钟（主力项目），5.83寸屏显示时间/日期/天气 |
| [epaper_clock_qt](projects/epaper_clock_qt/) | 墨水屏时钟 Qt 原型版 |
| [epaper_gif](projects/epaper_gif/) | 墨水屏 GIF 动画测试 |
| [epaper_image_test](projects/epaper_image_test/) | 墨水屏图片显示测试 |
| [epaper_5in83_test](projects/epaper_5in83_test/) | 5.83寸墨水屏驱动测试 |
| [epaper_5in83_V2_test](projects/epaper_5in83_V2_test/) | 5.83寸墨水屏 V2 驱动测试 |
| [blink_d2](projects/blink_d2/) | LED 闪烁（ESP-IDF） |
| [breath_d2](projects/breath_d2/) | LED 呼吸灯（ESP-IDF） |
| [hello_world](projects/hello_world/) | Hello World（ESP-IDF） |
| [arduino_blink](projects/arduino_blink/) | LED 闪烁（Arduino） |
| [gpio_button_test](projects/gpio_button_test/) | 🔘 按键控制 LED + SSH 远程上报（FreeRTOS 三任务） |
| [ssh_log_uploader](projects/ssh_log_uploader/) | SSH 远程日志上传器（LibSSH-ESP32） |

### gpio_button_test 简介

按键控制 LED + SSH 远程上报，基于 FreeRTOS 三任务架构。

- 🔘 **短按 D18**：D2 LED 亮/灭切换
- 🕯️ **长按 D18 (>1s)**：D2 持续闪烁（200ms 间隔），闪烁中短按停止
- 📡 **每 10s SSH 上报**：闪烁计数 + 时间戳 → `echo >>` 远程服务器日志
- 🧵 **三任务架构**：btnTask (Core 0) → Queue → ledTask (Core 1)，sshTask (Core 0, 24KB)
- 🔐 LibSSH-ESP32 纯密码认证，WiFi 自动重连 + NTP 时间同步
- 📉 **Flash 优化**：去 SPIFFS、去 String 类、裁剪 LibSSH（关 WITH_SERVER + DEBUG_CALLTRACE），从 85% → 81%

```bash
cd projects/gpio_button_test
arduino-cli compile --fqbn esp32:esp32:esp32 -u -p /dev/ttyUSB0
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

### ssh_log_uploader 简介

基于 **LibSSH-ESP32** 的物联网 SSH 客户端，通过 SSH exec 远程命令向 Linux 服务器上传日志。

- ⚙️ FreeRTOS 双任务架构（32KB 栈）避免栈溢出
- 🔐 SSH 加密传输 + known_hosts 主机密钥验证
- 🌐 WiFi 自动重连 + NTP 时间同步
- 💡 板载 LED 闪烁计数
- 📡 串口 115200 实时状态报告

```bash
cd projects/ssh_log_uploader
arduino-cli compile --fqbn esp32:esp32:esp32 -u -p /dev/ttyUSB0
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

## 其他目录

| 目录 | 说明 |
| --- | --- |
| [clockQT/](clockQT/) | Qt 打样原型 + 资源生成脚本（`generate_resources.py`） |
| [epaper/](epaper/) | Waveshare 墨水屏驱动库 |

## 硬件环境

- **ESP32 Dev Module** (CH343 USB 芯片, `/dev/ttyUSB0`)
- 板载 LED: GPIO 2
- Waveshare 5.83inch e-Paper V2 (648×480, 黑白)
- SPI 引脚: SCK=13, MOSI=14, CS=15, DC=27, RST=26, BUSY=25

## 开发环境

- Arduino CLI + ESP32 Core 3.3.8
- ESP-IDF（部分项目）
- Python 3（资源生成脚本）

### 编译烧录（Arduino 项目）

```bash
export PATH="$HOME/.local/bin:$PATH"
cd projects/epaper_clock
arduino-cli compile --fqbn esp32:esp32:esp32 -u -p /dev/ttyACM0
```

### 串口监视

```bash
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

## 依赖库

| 库 | 用途 | 项目 |
| --- | --- | --- |
| **LibSSH-ESP32** | SSH/SCP 客户端 | gpio_button_test, ssh_log_uploader |
| waveshare-e-Paper | 墨水屏驱动 | epaper_* |
| ArduinoJson 7.x | 天气 API JSON 解析 | epaper_clock |
| ESP32 Core 3.3.8 | Arduino ESP32 核心 | 全部 Arduino 项目 |

### LibSSH-ESP32 Flash 优化

为节省 Flash，对 `LibSSH-ESP32` 库配置做了以下裁剪（修改文件：`~/Arduino/libraries/LibSSH-ESP32/src/libssh_esp32_config.h`）：

| 选项 | 原始 | 修改后 | 效果 |
|------|------|--------|------|
| `WITH_SERVER` | `#define ... 1` | `/* #undef */` | 去掉 SSH 服务端代码 |
| `DEBUG_CALLTRACE` | `#define ... 1` | `/* #undef */` | 去掉调试追踪 |

> ⚠️ 升级 LibSSH-ESP32 库后需重新应用以上修改。

## License

各项目分别适用其声明的许可证。`ssh_log_uploader` 使用 GNU LGPL v2.1（与 LibSSH-ESP32 一致）。
