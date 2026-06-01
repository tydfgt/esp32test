# ESP32 工作区 — AI 编程上下文文档

> **目标读者**：AI 编程助手（GitHub Copilot 等）
> **更新日期**：2026-06-01（新增 esp32_flasher 烧录工具、microros_publisher 项目）
> **机器**：Ubuntu 22.04.5 LTS (x86_64)，主机名 `cedarTX`

---

## 1. 环境速览

| 项目 | 值 |
|------|-----|
| **OS** | Ubuntu 22.04.5 LTS, Linux 6.8.0, x86_64 |
| **Arduino CLI** | `~/.local/bin/arduino-cli` v1.5.0 |
| **ESP32 Arduino Core** | `esp32:esp32` v3.3.8 (基于 ESP-IDF 5.x) |
| **ESP-IDF（备用）** | v6.1-dev, `~/esp/esp-idf/`, 激活: `source ~/esp/esp-idf/export.sh` |
| **Qt 6** | `~/Qt/6.8.3/gcc_64/` |
| **Python** | `/usr/bin/python3` (系统 3.10) — 图片转换必须用系统 Python，不要用 ESP-IDF 虚拟环境 |
| **板卡 FQBN** | `esp32:esp32:esp32` |
| **烧录端口** | `/dev/ttyUSB0` |
| **芯片** | ESP32-D0WDQ6 v1.1 (Xtensa 双核 LX6, Flash 4MB) |
| **串口波特率** | 烧录 921600 / 监视 115200 |
| **编译器** | `~/.arduino15/packages/esp32/tools/esp-x32/2601/bin/xtensa-esp32-elf-g++` |
| **已安装 Arduino 库** | `ArduinoJson 7.4.3`, `LibSSH-ESP32 5.8.0`, `waveshare-e-Paper 1.0.0` |

---

## 2. 关键路径速查

```
/home/cedar/esp32/                        # ← 工作区根目录
├── AI_CONTEXT.md                         # ← 本文件（给 AI 读的）
├── Arduino开发环境交接文档.md              # Arduino CLI 详细手册
├── ESP32开发环境部署指南.md                # 环境搭建步骤
├── README.md                             # 项目列表概览
├── setup_esp32_env.sh                    # 一键部署脚本
├── .vscode/
│   ├── settings.json                     # Arduino 扩展配置
│   └── c_cpp_properties.json             # C++ IntelliSense 配置
│
├── clockQT/                              # Qt6 桌面端 UI 原型
│   ├── CMakeLists.txt
│   ├── main.cpp / clockwidget.h / clockwidget.cpp
│   ├── generate_resources.py
│   └── 墨稿-左右组合.png                  # Logo 源文件
│
├── projects/                             # 所有 ESP32 固件项目
│   ├── epaper_clock/                     # 🔥 主力：墨水屏桌面时钟
│   ├── epaper_clock_qt/                  # Qt 风格重构版时钟
│   ├── epaper_gif/                       # GIF 动画播放
│   ├── microros_publisher/               # 🆕 micro-ROS 发布者示例
│   ├── epaper_image_test/                # 图片显示测试
│   ├── epaper_5in83_test/                # 5.83寸 V1 驱动测试
│   ├── epaper_5in83_V2_test/             # 5.83寸 V2 驱动测试
│   ├── gpio_button_test/                 # 按键 + SSH 远程上报
│   ├── ssh_log_uploader/                 # SSH 日志上传器
│   ├── arduino_blink/                    # 基础 LED 闪烁 (Arduino)
│   ├── blink_d2/                         # LED 闪烁 (ESP-IDF)
│   ├── breath_d2/                        # LED 呼吸灯 (ESP-IDF)
│   ├── hello_world/                      # Hello World (ESP-IDF)
│   └── convert_image.py                  # PNG→C数组 通用工具
│
├── esp32_flasher/                        # 🖥 Qt6 ESP32 烧录工具 (GUI)
│   ├── CMakeLists.txt
│   ├── main.cpp
│   ├── mainwindow.h / mainwindow.cpp
│   ├── run.sh                            # 一键启动脚本
│   └── build/ESP32Flasher                # 编译产物
│
├── epaper/                               # Waveshare 官方驱动板资料
│   └── E-Paper_ESP32_Driver_Board_Code/
│       ├── examples/                     # 官方示例
│       ├── project-template/             # 项目模板
│       ├── Loader_esp32bt/               # 蓝牙刷图 App 固件
│       ├── Loader_esp32wf/               # WiFi 刷图 App 固件
│       ├── ePape_Esp32_Loader_APP/       # 手机刷图 App 源码
│       ├── dev-tools/                    # 开发工具
│       └── app-release.apk               # 手机刷图 App APK
│
└── upload/                               # 文档备份
```

### Arduino 工具链路径

| 工具 | 路径 |
|------|------|
| Arduino CLI | `~/.local/bin/arduino-cli` |
| 数据目录 | `~/.arduino15/` (6.9GB) |
| ESP32 核心源码 | `~/.arduino15/packages/esp32/hardware/esp32/3.3.8/` |
| Xtensa 编译器 | `~/.arduino15/packages/esp32/tools/esp-x32/2601/bin/xtensa-esp32-elf-g++` |
| RISC-V 编译器 | `~/.arduino15/packages/esp32/tools/esp-rv32/2601/bin/riscv32-esp-elf-g++` |
| esptool | `~/.arduino15/packages/esp32/tools/esptool_py/5.2.0/esptool` |
| GDB | `~/.arduino15/packages/esp32/tools/xtensa-esp-elf-gdb/16.3_20250913/bin/` |
| 编译缓存 | `~/.cache/arduino/` (可安全删除) |

---

## 3. 硬件规格

### 3.1 墨水屏驱动板

| 项目 | 详情 |
|------|------|
| 型号 | Waveshare E-Paper ESP32 Driver Board (CH343 版) |
| WiFi | 802.11b/g/n 2.4G |
| 蓝牙 | BLE 4.2 |
| Flash | 4MB |

### 3.2 墨水屏

| 项目 | 详情 |
|------|------|
| 型号 | 5.83inch e-Paper V2 |
| 类型 | 黑白（单色） |
| 分辨率 | **648 × 480** |
| 驱动 IC | EPD_5in83_V2 |

### 3.3 引脚映射（固定，不可改）

| 功能 | GPIO | 说明 |
|------|------|------|
| SCK (SPI 时钟) | **13** | |
| MOSI/DIN (SPI 数据) | **14** | |
| CS (片选) | **15** | 低有效 |
| DC (数据/命令) | **27** | |
| RST (复位) | **26** | 低有效 |
| BUSY (忙信号) | **25** | 高=忙 |

### 3.4 硬件开关

| 开关 | 正确位置 | 说明 |
|------|----------|------|
| 1号 (Display Config) | **A** (0.47R) | 5.83/7.5寸必须 A |
| 2号 (USB to UART) | **ON** | 烧录必须 ON |

---

## 4. 常用一键命令

> ⚠️ 每次新终端必须先设置 PATH：`export PATH="$HOME/.local/bin:$PATH"`（已写入 `~/.bashrc`，重启生效）

### 编译

```bash
cd /home/cedar/esp32/projects/<项目名> && export PATH="$HOME/.local/bin:$PATH" && arduino-cli compile --fqbn esp32:esp32:esp32
```

### 编译+烧录一步到位

```bash
cd /home/cedar/esp32/projects/<项目名> && export PATH="$HOME/.local/bin:$PATH" && arduino-cli compile --fqbn esp32:esp32:esp32 -u -p /dev/ttyUSB0
```

### 串口监视

```bash
export PATH="$HOME/.local/bin:$PATH" && arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

### 清理编译缓存

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 --clean
# 或彻底清理
rm -rf ~/.cache/arduino/
```

### 列出已连接板卡

```bash
export PATH="$HOME/.local/bin:$PATH" && arduino-cli board list
```

### 列出已安装库

```bash
export PATH="$HOME/.local/bin:$PATH" && arduino-cli lib list
```

### 安装新库

```bash
export PATH="$HOME/.local/bin:$PATH" && arduino-cli lib install "库名"
```

### 编译 Qt 原型

```bash
cd /home/cedar/esp32/clockQT && mkdir -p build && cd build && cmake .. && make -j$(nproc)
./ClockQT
```

---

## 5. 各项目详解

### 5.1 epaper_clock（🔥 主力项目）

**一句话**：墨水屏桌面时钟，显示时间/日期/天气/Logo

| 项目 | 值 |
|------|-----|
| 路径 | `/home/cedar/esp32/projects/epaper_clock/` |
| 类型 | Arduino (.ino) |
| 主文件 | `epaper_clock.ino` |

**文件结构**：
```
epaper_clock/
├── epaper_clock.ino          # 主固件 (~400行)
├── Font24CN_Clock.h/.cpp     # 24×24 中文字模 (53个汉字)
├── DigitData.h/.cpp          # 大号数字位图 0-9 (84×148px)
├── Ascii32Data.h/.cpp        # 32×32 ASCII 数字/符号位图
├── ImageData.h/.cpp          # 全屏底图 C 数组（备用）
├── LogoOnly.h/.cpp           # Logo 区域位图 (~500×107)
└── README.md
```

**核心功能**：
- 大号数字 HH:MM:SS，每秒局部刷新（`EPD_5in83_V2_Display_Partial`）
- 中文日期格式「yyyy年M月d日 周X」
- 天气：uapis.cn API，显示天气描述/温度/湿度/风向风力
- 天气图标：手绘（晴/多云/阴/雨/雪/雾）
- 全屏刷新每 10 分钟清残影（`EPD_5in83_V2_Display`）
- NTP 每小时校准
- 天气 10 分钟刷新
- WiFi 自动重连

**全局架构**：
- `g_FullBuf` / `g_TimeBuf` — 全屏/局部刷新缓冲区
- `fullRefresh()` — 全屏绘制（天气栏 + 分隔线 + 大时间 + 日期 + Logo）
- `partialTime()` — 局部刷新只更新数字时间区域
- `drawBigDigit()` — 绘制 84×148 大数字
- `drawMixedText32()` — 混合中英文 32px 文本
- `drawWeatherIcon()` — 手绘天气图标
- `wxFetch()` — uapis.cn API 请求 + JSON 解析
- `wifiConnect()` / `ntpSync()` — 网络基础设施

**依赖库**：`waveshare-e-Paper`, `ArduinoJson`

**编译烧录**：
```bash
cd /home/cedar/esp32/projects/epaper_clock && export PATH="$HOME/.local/bin:$PATH" && arduino-cli compile --fqbn esp32:esp32:esp32 -u -p /dev/ttyUSB0
```

**配置项**（在 .ino 顶部修改）：
- `WIFI_SSID` / `WIFI_PASS` — WiFi 凭据
- `WX_HOST` / `WX_PATH` / `WX_KEY` — 天气 API（uapis.cn）
- `GMT_OFFSET` — 时区偏移（默认 +8）
- 刷新间隔：`WX_INTERVAL`、`FULL_INTERVAL`、`NTP_INTERVAL`

---

### 5.2 epaper_clock_qt（Qt 风格重构版）

**一句话**：epaper_clock 的代码结构重构版，更清晰的模块化

| 项目 | 值 |
|------|-----|
| 路径 | `/home/cedar/esp32/projects/epaper_clock_qt/` |
| 类型 | Arduino (.ino) |
| 主文件 | `epaper_clock_qt.ino` |

**文件结构**：
```
epaper_clock_qt/
├── epaper_clock_qt.ino       # 主固件
├── FontClockCN.h/.cpp        # 32×32 中文字模（与 epaper_clock 的 Font24CN 不同！）
├── DigitData.h/.cpp          # 大号数字位图 (48×72, 比 epaper_clock 小)
├── LogoData.h/.cpp           # Logo 位图
└── gen_assets.py             # 资源生成脚本
```

**与 epaper_clock 的主要差异**：
- 数字尺寸不同：48×72 vs 84×148
- 中文字模：32×32 vs 24×24
- 布局和间距可能不同
- 天气 API Key 配置位置相同

**依赖**：`waveshare-e-Paper`, `ArduinoJson`

**编译**：
```bash
cd /home/cedar/esp32/projects/epaper_clock_qt && export PATH="$HOME/.local/bin:$PATH" && arduino-cli compile --fqbn esp32:esp32:esp32 -u -p /dev/ttyUSB0
```

---

### 5.3 clockQT（Qt6 桌面原型）

**一句话**：在 PC 上模拟墨水屏时钟 UI，用于快速原型验证

| 项目 | 值 |
|------|-----|
| 路径 | `/home/cedar/esp32/clockQT/` |
| 类型 | Qt6 C++ (CMake) |
| 分辨率 | 648×480 (与墨水屏一致) |

**文件**：
```
clockQT/
├── CMakeLists.txt            # Qt6 CMake 配置
├── main.cpp                  # 入口
├── clockwidget.h             # 时钟控件声明
├── clockwidget.cpp           # 时钟控件实现 (绘制+天气+定时器)
├── generate_resources.py     # 资源生成
├── 墨稿-左右组合.png          # Logo 源文件
└── 墨水屏开发经验总结.md
```

**核心类**：`ClockWidget : public QWidget`
- `paintEvent()` — QPainter 绘制所有 UI
- `updateTime()` — 每秒刷新时间
- `fetchWeather()` — 高德天气 API（非 uapis.cn！）
- 天气图标：纯 QPainter 手绘（`drawSun`, `drawCloud`, `drawRain`, `drawSnow`, `drawCloudSun`, `drawFog`）
- Logo：QPixmap 加载 PNG

**编译运行**：
```bash
cd /home/cedar/esp32/clockQT/build && cmake .. && make -j$(nproc) && ./ClockQT
```

**与 ESP32 版本的关系**：clockQT 先在 PC 上验证 UI 布局和绘制逻辑，确认后再移植到 Arduino C++。

---

### 5.4 epaper_gif（GIF 动画播放）

| 项目 | 值 |
|------|-----|
| 路径 | `/home/cedar/esp32/projects/epaper_gif/` |
| 类型 | Arduino (.ino) |
| 主文件 | `epaper_gif.ino` |

**文件**：
- `epaper_gif.ino` — 固件，逐帧播放预转换的 GIF 帧
- `GifFrames.h/.cpp` — GIF 帧 C 数组
- `convert_gif_to_c.py` — GIF→C 数组转换工具

**convert_gif_to_c.py 用法**：
```bash
/usr/bin/python3 /home/cedar/esp32/projects/epaper_gif/convert_gif_to_c.py <gif路径> <输出目录>
```
- 正确处理 delta 帧合成
- 自动缩放到 648×480
- Floyd-Steinberg 抖动到 1-bit
- ⚠️ 必须用 `/usr/bin/python3`，ESP-IDF 虚拟环境会屏蔽 PIL

---

### 5.5 epaper_image_test / epaper_5in83_test / epaper_5in83_V2_test

**一句话**：墨水屏驱动和图片显示的基础测试项目

| 项目 | 说明 |
|------|------|
| `epaper_image_test` | 图片显示测试，含 `ImageData.h/.cpp` |
| `epaper_5in83_test` | 5.83寸 V1 驱动测试，含 `ImageData.c/.h` |
| `epaper_5in83_V2_test` | 5.83寸 V2 驱动测试，含 `imagedata.cpp` |

---

### 5.6 gpio_button_test（按键 + SSH 上报）

**一句话**：按键控制 LED + LibSSH 远程日志上报

| 项目 | 值 |
|------|-----|
| 路径 | `/home/cedar/esp32/projects/gpio_button_test/` |
| 类型 | Arduino (.ino) |
| 主文件 | `gpio_button_test.ino` |

**架构**：FreeRTOS 三任务
- `btnTask` (Core 0) — 按键扫描，通过 Queue 发送事件
- `ledTask` (Core 1) — 接收事件，控制 LED 状态机 (OFF/ON/BLINK)
- `sshTask` (Core 0, 24KB 栈) — 每 10s 通过 SSH exec 上报闪烁计数

**硬件**：按键 D18→GND（INPUT_PULLUP），板载 LED D2

**依赖**：`LibSSH-ESP32`

**已做优化**：去 SPIFFS、去 String 类、裁剪 LibSSH（关 WITH_SERVER + DEBUG_CALLTRACE）→ Flash 85%→81%

⚠️ **注意**：代码包含硬编码的 WiFi/SSH 凭据，分发前需清除。

---

### 5.7 ssh_log_uploader（SSH 日志上传器）

**一句话**：独立的 LibSSH-ESP32 日志上传客户端

| 项目 | 值 |
|------|-----|
| 路径 | `/home/cedar/esp32/projects/ssh_log_uploader/` |

**架构**：FreeRTOS 双任务（controlTask 32KB 栈 Core 0 + loop 空闲）

**功能**：LED 闪烁计数 → 每 10s SSH exec `echo >>` 追加到远程服务器日志

**附加文件**：`server_receiver.py`（服务器端接收脚本）、`技术交接文档.md`、`CSDN发布文章.md`

---

### 5.8 ESP-IDF 项目

| 项目 | 说明 |
|------|------|
| `blink_d2` | GPIO 2 LED 闪烁，含 `CMakeLists.txt` + `main/` + `sdkconfig` |
| `breath_d2` | GPIO 2 LED 呼吸灯 (PWM) |
| `hello_world` | 标准 Hello World，含 pytest 测试 |

**ESP-IDF 编译方式**（与 Arduino 不同！）：
```bash
source ~/esp/esp-idf/export.sh
cd /home/cedar/esp32/projects/blink_d2
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

---

### 5.9 microros_publisher（🆕 micro-ROS 发布者）

**一句话**：ESP32 通过 WiFi+UDP 连接 micro-ROS Agent，向 ROS2 话题发布计数器

| 项目 | 值 |
|------|-----|
| 路径 | `/home/cedar/esp32/projects/microros_publisher/` |
| 类型 | Arduino (.ino) |
| 主文件 | `microros_publisher.ino` |

**架构**：
```
ESP32 (micro-ROS client) --[WiFi/UDP]--> micro-ROS Agent (PC, snap) --[DDS]--> ROS2 网络
```

**核心功能**：
- WiFi 连接 + UDP 传输（`set_microros_wifi_transports`）
- 每秒发布 `std_msgs/msg/Int32` 到话题 `/esp32_counter`
- 定时器 + 执行器模式
- 板载 LED 状态指示

**依赖库**：`micro_ros_arduino@2.0.7-humble`（预编译）

**使用步骤**：
```bash
# 1. 修改 .ino 中 WiFi SSID/PASS，确认 AGENT_IP 为 PC 的 192.168.3.x 地址

# 2. 启动 Agent（PC 端，新终端）
micro-ros-agent udp4 --port 8888

# 3. 编译+烧录
cd /home/cedar/esp32/projects/microros_publisher && export PATH="$HOME/.local/bin:$PATH" && arduino-cli compile --fqbn esp32:esp32:esp32 -u -p /dev/ttyUSB0

# 4. 验证（PC 端）
source /opt/ros/humble/setup.bash
ros2 topic list
ros2 topic echo /esp32_counter
```

---

### 5.10 arduino_blink

最简 Arduino blink 示例（GPIO 2），用于验证 Arduino CLI 工具链正常。

```bash
cd /home/cedar/esp32/projects/arduino_blink && export PATH="$HOME/.local/bin:$PATH" && arduino-cli compile --fqbn esp32:esp32:esp32 -u -p /dev/ttyUSB0
```

---

### 5.11 esp32_flasher（🖥 Qt6 ESP32 烧录工具）

**一句话**：Qt6 GUI 烧录工具，支持 Arduino 项目编译+烧录和 .bin 直接烧录，内建串口监视器

| 项目 | 值 |
|------|-----|
| 路径 | `/home/cedar/esp32/esp32_flasher/` |
| 类型 | Qt6 Widgets C++ (CMake) |
| 技术栈 | Qt 6.8.3 + Widgets + SerialPort |

**文件结构**：
```
esp32_flasher/
├── CMakeLists.txt            # Qt6 CMake (Qt6::Widgets + Qt6::SerialPort)
├── main.cpp                  # 入口
├── mainwindow.h              # MainWindow 声明
├── mainwindow.cpp            # 主逻辑 (~650行)
├── run.sh                    # 一键启动
└── build/ESP32Flasher        # 编译产物
```

**核心功能**：
- 🔌 自动扫描可用串口，🔄一键刷新
- 📋 9 种 ESP32 板卡选择 (ESP32/S3/S2/C3/C6/H2/P4/NodeMCU/CAM)
- 🔀 双模式：Arduino 项目（.ino 编译+烧录）/ 直接烧录 .bin
- 🎯 bin 模式可指定 flash 地址 (bootloader=0x1000, app=0x10000)
- ⚡ 烧录波特率 921600~57600 可调
- 📜 深色终端风格烧录日志 (QProcess 异步捕获 arduino-cli/esptool 输出)
- 📡 内建串口监视器 (QSerialPort)，支持发送数据 (可选 \n)
- 📊 实时进度条

**编译 & 启动**：
```bash
cd /home/cedar/esp32/esp32_flasher/build && cmake .. && make -j$(nproc)
./ESP32Flasher
# 或一键启动
/home/cedar/esp32/esp32_flasher/run.sh
```

**实现要点**：
- `QProcess` 异步调用 `arduino-cli compile -u -p /dev/ttyXXX`
- bin 模式通过 `/usr/bin/python3` 调用 esptool 直接写 flash
- 端口名始终使用 `/dev/ttyUSB0` 完整路径（`QSerialPortInfo::portName()` 只返回 `ttyUSB0`，需手动补前缀）
- 串口监视独立波特率，与烧录互不干扰
- 自动注入 `PATH` 环境变量确保找到 `arduino-cli`

---

## 6. 图片转换工具

### 6.1 convert_image.py（通用 PNG→C 数组）

**路径**：`/home/cedar/esp32/projects/convert_image.py`

**用法**：
```bash
/usr/bin/python3 /home/cedar/esp32/projects/convert_image.py
```

**功能**：将 PNG 转为 5.83寸 V2 墨水屏可用的 1-bit C 数组
- 输入：修改脚本内 `INPUT` 变量
- 输出：`ImageData.h` + `ImageData.cpp` + 预览 PNG
- 自动缩放适配 648×480，居中，Floyd-Steinberg 抖动
- 1=白 0=黑（Waveshare 约定）

### 6.2 epaper_clock_qt/gen_assets.py

**路径**：`/home/cedar/esp32/projects/epaper_clock_qt/gen_assets.py`

**用法**：
```bash
/usr/bin/python3 /home/cedar/esp32/projects/epaper_clock_qt/gen_assets.py
```

**功能**：为 epaper_clock_qt 项目生成所有资源
- Logo：560px 宽等比缩放，Floyd-Steinberg 抖动 → `LogoData.h/.cpp`
- 数字：48×72 位图 0-9 → `DigitData.h/.cpp`
- 中文：32×32 点阵字模 → `FontClockCN.h/.cpp`

### 6.3 clockQT/generate_resources.py

**路径**：`/home/cedar/esp32/clockQT/generate_resources.py`

Qt 原型的资源生成脚本。

### ⚠️ Python 环境注意事项

所有图片转换脚本必须使用系统 Python：
```bash
/usr/bin/python3 <脚本>
```
**不能**使用 ESP-IDF 虚拟环境中的 python（会找不到 PIL 库）。

---

## 7. 设计模式与约定

### 7.1 Arduino 项目结构规范

每个 Arduino 项目必须包含：
```
项目名/
├── 项目名.ino                  # 主固件（必须与目录同名）
├── .vscode/
│   └── arduino.json           # VS Code Arduino 配置
├── *.h / *.cpp                 # 支持模块（数据、字模等）
└── README.md                   # 项目说明（可选）
```

`.vscode/arduino.json` 模板：
```json
{
    "board": "esp32:esp32:esp32",
    "port": "/dev/ttyUSB0",
    "output": "./build",
    "sketch": "项目名.ino",
    "configuration": "Default"
}
```

### 7.2 墨水屏开发约定

- **刷新策略**：全刷（`EPD_5in83_V2_Display`）用于初始化和定期清残影；局刷（`EPD_5in83_V2_Display_Partial`）用于高频更新
- **全刷频率**：建议 ≥ 10 分钟一次，频繁全刷会损坏屏幕
- **位图格式**：1-bit 单色，1=白 0=黑，MSB 先，行优先
- **缓冲区**：全屏缓冲区大小 = `(648+7)/8 × 480 = 81 × 480 = 38880 bytes`
- **字模**：中文用点阵字模（PROGMEM 存储），英文用位图
- **绘制流程**：`Init → Paint_NewImage → Paint_SelectImage → Paint_Clear → 绘制 → Display → Sleep`

### 7.3 编码风格

- Arduino `.ino` 文件本质是 C++（预处理后自动加 `#include <Arduino.h>` + 函数声明）
- 全局变量用 `g_` 前缀
- 中断/任务间通信用 Queue + volatile
- WiFi 凭据和 API Key 写在代码顶部（不敏感信息）
- 字符串优先用 `char[]` 而非 `String` 类（省 Flash）

### 7.4 ESP32 特定约定

- `setup()` / `loop()` — Arduino 标准入口
- FreeRTOS 任务：`xTaskCreatePinnedToCore()` 可指定核心
- 栈大小建议：简单任务 2048，SSH 任务 ≥ 24576
- `Serial.begin(115200)` — 标准串口速率
- `millis()` 用于非阻塞定时，`delay()` 仅用于初始化

---

## 8. 库与依赖管理

### 已安装 Arduino 库（用户级）

| 库名 | 版本 | 用途 |
|------|------|------|
| `ArduinoJson` | 7.4.3 | JSON 解析（天气 API） |
| `LibSSH-ESP32` | 5.8.0 | SSH 客户端（远程日志上报） |
| `waveshare-e-Paper` | 1.0.0 | 墨水屏驱动（EPD_5in83_V2） |
| `micro_ros_arduino` | 2.0.7-humble | micro-ROS 客户端（ROS2 话题发布/订阅） |

### 安装新库

```bash
export PATH="$HOME/.local/bin:$PATH"
arduino-cli lib install "库名"
arduino-cli lib list    # 验证
```

### ESP32 Arduino Core 自带库（无需安装）

WiFi, HTTPClient, WiFiClient, time, freertos/FreeRTOS, freertos/task, freertos/queue 等。

---

## 9. 故障排查

| 问题 | 解决方案 |
|------|----------|
| `arduino-cli: command not found` | `export PATH="$HOME/.local/bin:$PATH"` |
| 找不到 `/dev/ttyUSB0` | `ls /dev/ttyUSB*` 检查；重新拔插；`sudo dmesg \| tail` 查看内核日志 |
| 烧录失败 "Connecting..." | 按住 BOOT → 按 EN → 松开 BOOT → 立即烧录；或降低波特率 `--upload-field baud=115200` |
| 编译找不到库 | `arduino-cli lib list` 检查是否已安装 |
| ESP-IDF 找不到 PIL | 图片转换用 `/usr/bin/python3`，不要用 ESP-IDF 虚拟环境 |
| 墨水屏不显示 | 检查 1号开关是否在 A；检查 2号开关是否 ON |
| Flash 空间不足 | 去 SPIFFS、用 `const char*` 替代 `String`、用 PROGMEM 存大数据 |

---

## 10. AI 编程速查卡

```
【机器环境】
OS: Ubuntu 22.04.5 LTS x86_64
Arduino CLI: ~/.local/bin/arduino-cli v1.5.0
ESP32 Core: esp32:esp32@3.3.8
Board FQBN: esp32:esp32:esp32
Port: /dev/ttyUSB0
Chip: ESP32-D0WDQ6 v1.1

【工作区】
Root: /home/cedar/esp32/
Projects: /home/cedar/esp32/projects/
Libs: ~/.arduino15/

【墨水屏】
型号: 5.83inch e-Paper V2, 648×480 黑白
SPI: SCK=13 MOSI=14 CS=15 DC=27 RST=26 BUSY=25
驱动库: waveshare-e-Paper

【一键命令】
编译+烧录: cd /home/cedar/esp32/projects/<项目> && export PATH="$HOME/.local/bin:$PATH" && arduino-cli compile --fqbn esp32:esp32:esp32 -u -p /dev/ttyUSB0
监视: export PATH="$HOME/.local/bin:$PATH" && arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
图片转换: /usr/bin/python3 /home/cedar/esp32/projects/convert_image.py
烧录工具(GUI): /home/cedar/esp32/esp32_flasher/run.sh
micro-ROS Agent: micro-ros-agent udp4 --port 8888
ROS2 验证: source /opt/ros/humble/setup.bash && ros2 topic echo /esp32_counter

【注意事项】
- 图片转换必须用 /usr/bin/python3，不能用 ESP-IDF 虚拟环境
- 全屏刷新间隔 ≥ 10 分钟，保护墨水屏
- .ino 文件本质是 C++，支持所有 C++ 特性
- 大数据用 PROGMEM 存 Flash，避免 RAM 溢出
```

---

> 📝 本文档是工作区的「AI 入口」。新增项目或变更环境后，请同步更新本文件。
