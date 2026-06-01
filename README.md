# ESP32 开发项目集合

ESP32 相关开发项目，包含基础实验、墨水屏应用、micro-ROS 集成和烧录工具。

> **仓库镜像**：[Gitee](https://gitee.com/tydfgt/esp32) | [GitHub](https://github.com/tydfgt/esp32test)  
> **最新更新**：2026-06-01

## 项目列表

### 主力项目

| 项目 | 说明 |
| --- | --- |
| [epaper_clock](projects/epaper_clock/) | 🔥 墨水屏桌面时钟（生产级），5.83寸屏显示时间/日期/天气/Logo，每秒局部刷新，10分钟全屏清残影 |
| [epaper_clock_qt](projects/epaper_clock_qt/) | 墨水屏时钟 Qt 风格重构版，32×32字模 + 48×72 数字 |

### 网络与通信

| 项目 | 说明 |
| --- | --- |
| [microros_publisher](projects/microros_publisher/) | 🆕 **micro-ROS 发布者**，ESP32 WiFi+UDP 连接 ROS2 网络，发布计数器话题 `/esp32_counter`，订阅遥控话题 `/esp32_led` |
| [gpio_button_test](projects/gpio_button_test/) | 🔘 按键控制 LED + SSH 远程上报（FreeRTOS 三任务）|
| [ssh_log_uploader](projects/ssh_log_uploader/) | SSH 远程日志上传器（LibSSH-ESP32） |

### 墨水屏测试

| 项目 | 说明 |
| --- | --- |
| [epaper_gif](projects/epaper_gif/) | 墨水屏 GIF 动画逐帧播放（自动合成 delta 帧，Floyd-Steinberg 抖动） |
| [epaper_image_test](projects/epaper_image_test/) | 墨水屏图片显示测试 |
| [epaper_5in83_test](projects/epaper_5in83_test/) | 5.83寸 V1 驱动测试 |
| [epaper_5in83_V2_test](projects/epaper_5in83_V2_test/) | 5.83寸 V2 驱动测试 |

### ESP-IDF 项目

| 项目 | 说明 |
| --- | --- |
| [blink_d2](projects/blink_d2/) | LED 闪烁（ESP-IDF） |
| [breath_d2](projects/breath_d2/) | LED 呼吸灯 PWM（ESP-IDF） |
| [hello_world](projects/hello_world/) | Hello World（ESP-IDF，含 pytest） |

### 基础示例

| 项目 | 说明 |
| --- | --- |
| [arduino_blink](projects/arduino_blink/) | LED 闪烁（Arduino 最简示例） |

### microros_publisher 简介

ESP32 通过 **WiFi + UDP** 连接 micro-ROS Agent，在 ROS2 网络中发布计数器话题。

- 🔗 **架构**：ESP32 (micro-ROS client) → WiFi/UDP → Agent (PC) → DDS → ROS2
- 📊 **功能**：每秒发布 `std_msgs/msg/Int32` 到话题 `/esp32_counter`
- 🎮 **反向通信**：订阅 `/esp32_led` 话题，遥控板载 LED 开关
- 🎯 **QoS**：best_effort（适合 UDP 不可靠链路）
- 🧵 **执行器模式**：定时器 + 订阅者，2 handle 配置

**使用步骤**（详见 [CSDN_ESP32_micro-ROS_实战.md](projects/microros_publisher/CSDN_ESP32_micro-ROS_实战.md)）：
```bash
# 1. PC 启动 Agent（源码编译版，snap 版不可用）
micro-ros-agent udp4 --port 8888

# 2. ESP32 烧录（修改 WiFi SSID/PASS 和 AGENT_IP）
cd projects/microros_publisher
arduino-cli compile --fqbn esp32:esp32:esp32 -u -p /dev/ttyUSB0

# 3. ROS2 验证
source /opt/ros/humble/setup.bash
ros2 topic echo /esp32_counter
ros2 topic pub -1 /esp32_led std_msgs/msg/Int32 "data: 1"  # LED 开
```

**依赖库**：`micro_ros_arduino@2.0.7-humble`（预编译二进制）

---

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
| [clockQT/](clockQT/) | 🖥 Qt6 桌面端时钟原型（648×480，与墨水屏 1:1），用于快速 UI 验证 |
| [esp32_flasher/](esp32_flasher/) | 🖥 **Qt6 ESP32 烧录工具 GUI**，支持 Arduino 编译+烧录 & .bin 直接烧录，内建串口监视器，见 [run.sh](esp32_flasher/run.sh) |
| [epaper/](epaper/) | Waveshare 官方驱动板资料（驱动库、示例、手机刷图 App） |
| [upload/](upload/) | 📚 文档备份（Arduino 环境交接、ESP32 部署指南） |

## 硬件环境

| 项目 | 规格 |
|------|------|
| **MCU** | ESP32-D0WDQ6 v1.1（Xtensa 双核 LX6，Flash 4MB） |
| **开发板** | Waveshare E-Paper ESP32 Driver Board（CH343 版） |
| **连接器** | `/dev/ttyUSB0`（USB 转 UART） |
| **GPIO** | 板载 LED = GPIO 2（高电平亮） |
| **墨水屏** | Waveshare 5.83inch e-Paper **V2**（648×480，单色黑白） |
| **屏幕驱动 IC** | EPD_5in83_V2 |
| **SPI 引脚** | SCK=13, MOSI=14, CS=15, DC=27, RST=26, BUSY=25（固定，不可改） |
| **硬件开关** | 1 号 → A（屏幕 0.47R），2 号 → ON（USB 烧录） |

## 开发环境

| 工具 | 版本 / 路径 |
|------|-----------|
| **操作系统** | Ubuntu 22.04.5 LTS (x86_64) |
| **Arduino CLI** | v1.5.0 (`~/.local/bin/arduino-cli`) |
| **ESP32 Arduino Core** | 3.3.8（基于 ESP-IDF 5.x） |
| **ESP-IDF**（备用） | v6.1-dev (`~/esp/esp-idf/`) |
| **Qt6**（桌面原型） | 6.8.3 (`~/Qt/6.8.3/gcc_64/`) |
| **Python** | 3.10 系统版（*必须用 `/usr/bin/python3`，图片转换不兼容 ESP-IDF 虚拟环境*） |
| **Xtensa 编译器** | `~/.arduino15/packages/esp32/tools/esp-x32/2601/bin/xtensa-esp32-elf-g++` |
| **esptool** | `~/.arduino15/packages/esp32/tools/esptool_py/5.2.0/esptool.py` |

**Arduino CLI 配置**（已自动配置）：
```bash
export PATH="$HOME/.local/bin:$PATH"
# ~/.bashrc 已添加，重启生效
```

**快速验证环境**：
```bash
arduino-cli version                    # → 1.5.0
arduino-cli board list                 # 看到 /dev/ttyUSB0
/usr/bin/python3 --version             # → 3.10.x
```

## 一键编译 & 烧录

### Arduino 项目编译 + 烧录

```bash
export PATH="$HOME/.local/bin:$PATH"

# 以 epaper_clock 为例
cd /home/cedar/esp32/projects/epaper_clock

# 编译 + 烧录一步到位
arduino-cli compile --fqbn esp32:esp32:esp32 -u -p /dev/ttyUSB0

# 或仅编译（output 在 build/ 目录）
arduino-cli compile --fqbn esp32:esp32:esp32
```

### 串口监视（115200 波特率）

```bash
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

### 使用 ESP32Flasher GUI 工具（推荐）

```bash
/home/cedar/esp32/esp32_flasher/run.sh
```

功能：
- 自动扫描 9 种 ESP32 板卡类型
- **Arduino 项目模式**：选择 .ino 目录，一键编译+烧录
- **.bin 模式**：选择二进制文件，直接烧录到指定地址（bootloader@0x1000, app@0x10000 等）
- **内建串口监视器**（15200~921600 波特率可调）
- 实时进度条和深色终端日志

### ESP-IDF 项目编译 & 烧录

```bash
source ~/esp/esp-idf/export.sh
cd /home/cedar/esp32/projects/blink_d2

idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## 依赖库

| 库 | 版本 | 用途 | 项目 |
| --- | --- | --- | --- |
| **micro_ros_arduino** | 2.0.7-humble | micro-ROS 客户端（ROS2 话题收发） | microros_publisher |
| **LibSSH-ESP32** | 5.8.0 | SSH/SCP 客户端 | gpio_button_test, ssh_log_uploader |
| **waveshare-e-Paper** | 1.0.0 | 墨水屏驱动 | epaper_* |
| **ArduinoJson** | 7.4.3 | 天气 API JSON 解析 | epaper_clock |
| ESP32 Core | 3.3.8 | Arduino ESP32 核心库 | 全部 Arduino 项目 |

### LibSSH-ESP32 Flash 优化

为节省 Flash，对 `LibSSH-ESP32` 库配置做了以下裁剪（修改文件：`~/Arduino/libraries/LibSSH-ESP32/src/libssh_esp32_config.h`）：

| 选项 | 原始 | 修改后 | 效果 |
|------|------|--------|------|
| `WITH_SERVER` | `#define ... 1` | `/* #undef */` | 去掉 SSH 服务端代码 |
| `DEBUG_CALLTRACE` | `#define ... 1` | `/* #undef */` | 去掉调试追踪 |

> ⚠️ 升级 LibSSH-ESP32 库后需重新应用以上修改。

## License

各项目分别适用其声明的许可证。`ssh_log_uploader` 使用 GNU LGPL v2.1（与 LibSSH-ESP32 一致）。
