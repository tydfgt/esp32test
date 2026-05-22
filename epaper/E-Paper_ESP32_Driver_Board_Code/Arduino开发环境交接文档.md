# ESP32 Arduino 开发环境 — 项目交接文档

> **机器**：Ubuntu 22.04.5 LTS (x86_64) — `cedarTX`  
> **创建日期**：2026-05-21  
> **用途**：本机 VS Code + Arduino CLI 开发 ESP32 的完整参考手册  
> **适用对象**：开发者本人 & AI 编程助手

---

## 目录

1. [环境速览](#1-环境速览)
2. [关键路径一览](#2-关键路径一览)
3. [Arduino CLI 常用命令](#3-arduino-cli-常用命令)
4. [VS Code 配置说明](#4-vs-code-配置说明)
5. [项目模板与工作流](#5-项目模板与工作流)
6. [常用 ESP32 板卡 FQBN](#6-常用-esp32-板卡-fqbn)
7. [烧录与串口监视](#7-烧录与串口监视)
8. [已安装的库与依赖](#8-已安装的库与依赖)
9. [故障排查](#9-故障排查)
10. [AI 编程助手速查卡](#10-ai-编程助手速查卡)

---

## 1. 环境速览

| 组件 | 详情 |
|------|------|
| **操作系统** | Ubuntu 22.04.5 LTS, Linux 6.8.0-117-generic |
| **架构** | x86_64 |
| **Arduino CLI** | v1.5.0 (dd407d42d, 2026-05-19) |
| **ESP32 核心** | esp32:esp32 v3.3.8（Espressif 官方，基于 ESP-IDF 5.x） |
| **编译器** | xtensa-esp32-elf-g++ (esp-x32/2601) |
| **Python** | 系统自带 Python 3.10 |
| **VS Code 扩展** | Arduino Community Edition (`vscode-arduino.vscode-arduino-community`) |
| **烧录工具** | esptool v5.2.0 |
| **支持芯片** | ESP32, ESP32-S2, ESP32-S3, ESP32-C2, ESP32-C3, ESP32-C5, ESP32-C6, ESP32-C61, ESP32-H2, ESP32-P4 |
| **支持板卡数** | 380+ 种 ESP32 变体板卡 |

### 已连接开发板

| 端口 | 芯片 | 版本 | MAC |
|------|------|------|-----|
| `/dev/ttyUSB0` | ESP32-D0WDQ6 | v1.1 | `84:1f:e8:8f:52:74` |

---

## 2. 关键路径一览

### 2.1 Arduino 相关

| 路径 | 说明 |
|------|------|
| `~/.local/bin/arduino-cli` | Arduino CLI 可执行文件 (36MB) |
| `~/.arduino15/` | Arduino 数据目录（6.9GB，含工具链+核心+库） |
| `~/.arduino15/arduino-cli.yaml` | Arduino CLI 主配置文件 |
| `~/.arduino15/packages/esp32/` | ESP32 所有硬件包 |
| `~/.arduino15/packages/esp32/hardware/esp32/3.3.8/` | ESP32 Arduino 核心源码 |
| `~/.arduino15/packages/esp32/tools/` | ESP32 工具链目录 |
| `~/.cache/arduino/` | 编译缓存（30MB，可定期清理） |

### 2.2 工具链详细路径

| 工具 | 路径 |
|------|------|
| **编译器 (Xtensa)** | `~/.arduino15/packages/esp32/tools/esp-x32/2601/bin/xtensa-esp32-elf-g++` |
| **编译器 (RISC-V)** | `~/.arduino15/packages/esp32/tools/esp-rv32/2601/bin/riscv32-esp-elf-g++` |
| **GDB 调试器** | `~/.arduino15/packages/esp32/tools/xtensa-esp-elf-gdb/16.3_20250913/bin/` |
| **esptool 烧录** | `~/.arduino15/packages/esp32/tools/esptool_py/5.2.0/esptool` |
| **OpenOCD** | `~/.arduino15/packages/esp32/tools/openocd-esp32/v0.12.0-esp32-20251215/` |

### 2.3 项目路径

| 路径 | 说明 |
|------|------|
| `/home/cedar/esp32/` | 工作区根目录 |
| `/home/cedar/esp32/projects/arduino_blink/` | Arduino blink 示例项目 |
| `/home/cedar/esp32/projects/blink_d2/` | ESP-IDF blink 项目 |
| `/home/cedar/esp32/projects/breath_d2/` | ESP-IDF 呼吸灯项目 |
| `/home/cedar/esp32/projects/hello_world/` | ESP-IDF hello_world 项目 |
| `/home/cedar/esp32/.vscode/` | 工作区 VS Code 配置 |

---

## 3. Arduino CLI 常用命令

> **重要**：每次新开终端，先确保 PATH 正确：
> ```bash
> export PATH="$HOME/.local/bin:$PATH"
> ```
> （已写入 `~/.bashrc`，重启终端自动生效）

### 3.1 板卡管理

```bash
# 更新板卡索引
arduino-cli core update-index

# 列出已安装的核心
arduino-cli core list

# 搜索可用 ESP32 核心
arduino-cli core search esp32

# 安装 ESP32 核心（已装，无需再执行）
arduino-cli core install esp32:esp32@3.3.8

# 列出所有支持的 ESP32 板卡
arduino-cli board listall | grep esp32:esp32

# 列出当前连接的板卡
arduino-cli board list

# 查看某板卡详细信息
arduino-cli board details -b esp32:esp32:esp32
```

### 3.2 库管理

```bash
# 搜索库
arduino-cli lib search 库名

# 安装库
arduino-cli lib install 库名

# 列出已安装的库
arduino-cli lib list

# 卸载库
arduino-cli lib uninstall 库名
```

### 3.3 编译

```bash
# 标准编译（在 .ino 文件所在目录执行）
arduino-cli compile --fqbn esp32:esp32:esp32

# 详细输出编译
arduino-cli compile --fqbn esp32:esp32:esp32 -v

# 指定输出目录
arduino-cli compile --fqbn esp32:esp32:esp32 --output-dir ./build

# 清理后重新编译
arduino-cli compile --fqbn esp32:esp32:esp32 --clean
```

### 3.4 烧录（Upload）

```bash
# 烧录到 /dev/ttyUSB0
arduino-cli upload --fqbn esp32:esp32:esp32 -p /dev/ttyUSB0

# 详细输出烧录过程
arduino-cli upload --fqbn esp32:esp32:esp32 -p /dev/ttyUSB0 -v

# 编译+烧录一步到位
arduino-cli compile --fqbn esp32:esp32:esp32 -u -p /dev/ttyUSB0
```

### 3.5 串口监视

```bash
# 打开串口监视器 (115200 baud)
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200

# 指定其他配置
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=9600
```

### 3.6 配置管理

```bash
# 查看当前配置
arduino-cli config dump

# 查看配置文件位置
arduino-cli config init --show

# 添加额外板卡 URL
arduino-cli config set board_manager.additional_urls "URL"
```

---

## 4. VS Code 配置说明

### 4.1 工作区配置文件

#### `/home/cedar/esp32/.vscode/settings.json`
```json
{
    "arduino.path": "/home/cedar/.local/bin",
    "arduino.commandPath": "/home/cedar/.local/bin/arduino-cli",
    "arduino.useArduinoCli": true,
    "arduino.logLevel": "info",
    "arduino.allowPDEFiletype": false,
    "[arduino]": {
        "editor.defaultFormatter": "vscode.arduino-community"
    },
    "C_Cpp.intelliSenseEngine": "default",
    "files.associations": {
        "*.ino": "cpp",
        "*.pde": "cpp"
    }
}
```

#### `/home/cedar/esp32/.vscode/c_cpp_properties.json`
- IntelliSense 配置，includePath 指向 ESP32 Arduino 核心源码
- 编译器路径：`~/.arduino15/packages/esp32/tools/esp-x32/2601/bin/xtensa-esp32-elf-g++`
- 预定义宏：`ARDUINO=10800`, `ESP32`, `F_CPU=240000000L`

### 4.2 项目级配置模板

每个 Arduino 项目下创建 `.vscode/arduino.json`：

```json
{
    "board": "esp32:esp32:esp32",
    "port": "/dev/ttyUSB0",
    "output": "./build",
    "sketch": "项目名.ino",
    "configuration": "Default"
}
```

### 4.3 VS Code 快捷键操作

| 操作 | 方式 |
|------|------|
| **编译** | `F1` → `Arduino: Verify` 或 `Ctrl+Alt+R` |
| **烧录** | `F1` → `Arduino: Upload` 或 `Ctrl+Alt+U` |
| **串口监视** | `F1` → `Arduino: Open Serial Monitor` |
| **选择板卡** | `F1` → `Arduino: Board Config` |
| **选择端口** | `F1` → `Arduino: Select Serial Port` |

---

## 5. 项目模板与工作流

### 5.1 创建新 Arduino 项目

```bash
# 1. 创建项目目录
mkdir -p ~/esp32/projects/新项目名/.vscode

# 2. 创建 .ino 文件
cat > ~/esp32/projects/新项目名/新项目名.ino << 'EOF'
#define LED_BUILTIN 2

void setup() {
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);
    digitalWrite(LED_BUILTIN, LOW);
    delay(1000);
}
EOF

# 3. 创建 arduino.json 配置
cat > ~/esp32/projects/新项目名/.vscode/arduino.json << 'EOF'
{
    "board": "esp32:esp32:esp32",
    "port": "/dev/ttyUSB0",
    "output": "./build",
    "sketch": "新项目名.ino",
    "configuration": "Default"
}
EOF

# 4. 编译测试
cd ~/esp32/projects/新项目名
arduino-cli compile --fqbn esp32:esp32:esp32

# 5. 烧录
arduino-cli upload --fqbn esp32:esp32:esp32 -p /dev/ttyUSB0
```

### 5.2 推荐开发工作流

```
编写代码 → 编译验证 → 连接板卡 → 烧录 → 串口监视
   ↓          ↓           ↓          ↓         ↓
 .ino文件  Ctrl+Alt+R  /dev/ttyUSB0  Ctrl+Alt+U  F1→Monitor
```

---

## 6. 常用 ESP32 板卡 FQBN

| 板卡名称 | FQBN | 说明 |
|----------|------|------|
| **ESP32 Dev Module** | `esp32:esp32:esp32` | 通用 ESP32-WROOM-32 开发板 ✅当前使用 |
| ESP32-S3 Dev Module | `esp32:esp32:esp32s3` | ESP32-S3 通用开发板 |
| ESP32-S2 Dev Module | `esp32:esp32:esp32s2` | ESP32-S2 通用开发板 |
| ESP32-C3 Dev Module | `esp32:esp32:esp32c3` | ESP32-C3 RISC-V 开发板 |
| ESP32-C6 Dev Module | `esp32:esp32:esp32c6` | ESP32-C6 RISC-V 开发板 |
| NodeMCU-32S | `esp32:esp32:nodemcu-32s` | NodeMCU ESP32 |
| ESP32-CAM | `esp32:esp32:esp32cam` | AI Thinker 摄像头板 |
| Adafruit Feather ESP32 | `esp32:esp32:featheresp32` | Adafruit Feather |
| WEMOS LOLIN32 | `esp32:esp32:lolin32` | WEMOS ESP32 板 |

> 查看完整 380+ 板卡列表：`arduino-cli board listall | grep esp32:esp32`

---

## 7. 烧录与串口监视

### 7.1 USB 串口权限

```bash
# 查看当前用户组（应包含 dialout）
groups $USER

# 如果没有 dialout，手动添加（需注销重新登录）
sudo usermod -a -G dialout $USER
```

✅ 本机 `cedar` 用户已在 `dialout` 组。

### 7.2 烧录参数说明

| 参数 | 默认值 | 说明 |
|------|--------|------|
| 端口 | `/dev/ttyUSB0` | USB 串口设备 |
| 波特率 | `921600` | 烧录通信速率 |
| Flash 模式 | `dio` | 双 I/O 模式 |
| Flash 频率 | `80m` | 80MHz |
| Flash 大小 | `4MB` | 4MB Flash |

### 7.3 串口常用波特率

| 波特率 | 用途 |
|--------|------|
| `115200` | ESP32 默认串口速率（推荐） |
| `921600` | 烧录专用高速率 |
| `9600` | 某些传感器的低速通信 |

---

## 8. 已安装的库与依赖

### 8.1 Arduino 已安装核心

```
esp32:esp32    3.3.8    esp32
```

### 8.2 已安装工具包

| 工具 | 版本 |
|------|------|
| xtensa-esp-elf-gcc (esp-x32) | 2601 |
| riscv32-esp-elf-gcc (esp-rv32) | 2601 |
| xtensa-esp-elf-gdb | 16.3_20250913 |
| riscv32-esp-elf-gdb | 16.3_20250913 |
| esptool_py | 5.2.0 |
| openocd-esp32 | v0.12.0-esp32-20251215 |
| mkspiffs | 0.2.3 |
| mklittlefs | 4.0.2-db0513a |

### 8.3 系统依赖

```bash
# 已安装的关键系统包
python3-pip python3-venv
libusb-1.0-0 libusb-1.0-0-dev
```

---

## 9. 故障排查

### 9.1 找不到 arduino-cli 命令

```bash
# 检查 PATH
echo $PATH | grep ".local/bin"
# 手动添加
export PATH="$HOME/.local/bin:$PATH"
```

### 9.2 无法连接 /dev/ttyUSB0

```bash
# 检查设备是否存在
ls -la /dev/ttyUSB*

# 检查权限
ls -la /dev/ttyUSB0

# 检查是否被占用
sudo lsof /dev/ttyUSB0

# 重新拔插 ESP32 后重试
```

### 9.3 烧录失败

```bash
# 1. 按住 BOOT 按钮不放
# 2. 按一下 EN 按钮（或重新上电）
# 3. 松开 BOOT 按钮
# 4. 立即执行烧录命令

# 或降低烧录波特率
arduino-cli upload --fqbn esp32:esp32:esp32 -p /dev/ttyUSB0 --upload-field baud=115200
```

### 9.4 编译错误清理

```bash
# 清理编译缓存
arduino-cli compile --fqbn esp32:esp32:esp32 --clean

# 手动清理缓存
rm -rf ~/.cache/arduino/
```

### 9.5 重置 Arduino 配置

```bash
# 备份后重新初始化
mv ~/.arduino15 ~/.arduino15.bak
arduino-cli config init --overwrite
arduino-cli config set board_manager.additional_urls "https://espressif.github.io/arduino-esp32/package_esp32_index.json"
arduino-cli core update-index
arduino-cli core install esp32:esp32@3.3.8
```

---

## 10. AI 编程助手速查卡

> 以下是提供给 AI 编程助手（如 GitHub Copilot）的上下文摘要。

### 10.1 本机环境标识

```
OS: Ubuntu 22.04.5 LTS x86_64
Arduino CLI: ~/.local/bin/arduino-cli (v1.5.0)
ESP32 Core: esp32:esp32@3.3.8
Board FQBN: esp32:esp32:esp32
Port: /dev/ttyUSB0
Chip: ESP32-D0WDQ6 v1.1
Workspace: /home/cedar/esp32
Projects: /home/cedar/esp32/projects/
```

### 10.2 给 AI 的提示词模板

```
我正在本机 Ubuntu 22.04 x86_64 上使用 Arduino 框架开发 ESP32。
Arduino CLI 路径：~/.local/bin/arduino-cli
板卡 FQBN：esp32:esp32:esp32
烧录端口：/dev/ttyUSB0
项目放在 /home/cedar/esp32/projects/ 下。
请帮我 [你的需求]。
```

### 10.3 创建新项目的 AI 指令示例

```
在 /home/cedar/esp32/projects/ 下创建一个名为 xxx 的 Arduino 项目，
使用 esp32:esp32:esp32 板卡，端口 /dev/ttyUSB0，
包含 .ino 文件和 .vscode/arduino.json 配置。
[描述项目功能]
```

### 10.4 常用一键命令（给 AI 用）

```bash
# 编译
cd /home/cedar/esp32/projects/<项目名> && export PATH="$HOME/.local/bin:$PATH" && arduino-cli compile --fqbn esp32:esp32:esp32

# 烧录
cd /home/cedar/esp32/projects/<项目名> && export PATH="$HOME/.local/bin:$PATH" && arduino-cli upload --fqbn esp32:esp32:esp32 -p /dev/ttyUSB0

# 串口监视
export PATH="$HOME/.local/bin:$PATH" && arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

---

> 📝 **文档维护**：环境变更后请更新本文档。本文件位于 `/home/cedar/esp32/Arduino开发环境交接文档.md`。
