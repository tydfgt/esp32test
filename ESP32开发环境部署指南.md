# ESP32 VS Code 开发环境部署指南（Ubuntu 22.04 x86_64）

> 适用系统：Ubuntu 22.04 LTS（x86_64）  
> 目标：在 VS Code 中搭建完整的 ESP32 开发、编译、烧录、调试环境

---

## 目录

1. [环境概览](#1-环境概览)
2. [配置代理（可选）](#2-配置代理可选国内用户强烈推荐)
3. [系统依赖安装](#3-系统依赖安装)
4. [安装 ESP-IDF 框架](#4-安装-esp-idf-框架)
5. [安装 VS Code 及 ESP-IDF 扩展](#5-安装-vs-code-及-esp-idf-扩展)
6. [配置 ESP-IDF 扩展](#6-配置-esp-idf-扩展)
7. [USB 串口权限配置](#7-usb-串口权限配置)
8. [创建并编译第一个项目](#8-创建并编译第一个项目)
9. [烧录与串口监视](#9-烧录与串口监视)
10. [常见问题排查](#10-常见问题排查)
11. [附录：快捷脚本](#11-附录快捷脚本)

---

## 1. 环境概览

| 组件 | 说明 |
|------|------|
| **芯片型号** | ESP32-WROOM-32（Xtensa 双核 LX6） |
| **操作系统** | Ubuntu 22.04 LTS (x86_64) |
| **IDE** | VS Code + ESP-IDF 官方扩展 |
| **SDK** | ESP-IDF（推荐 v5.4 或 v5.3 LTS） |
| **工具链** | xtensa-esp-elf（ESP32 系列） |
| **Python** | Python 3.10+（系统自带） |
| **调试器** | 内置 OpenOCD + 串口监视器 |

---

## 2. 配置代理（可选，国内用户强烈推荐）

由于 ESP-IDF 及工具链需从 GitHub 下载，国内网络可能很慢或失败。如果你已开启 **flclash / Clash** 等代理工具，请先配置终端代理。

### 2.1 设置终端代理环境变量

```bash
# 根据你的 Clash 端口修改（通常 HTTP=7890, SOCKS5=7891）
export HTTP_PROXY="http://127.0.0.1:7890"
export HTTPS_PROXY="http://127.0.0.1:7890"
export http_proxy="http://127.0.0.1:7890"
export https_proxy="http://127.0.0.1:7890"
```

### 2.2 配置 Git 代理

```bash
git config --global http.proxy  "http://127.0.0.1:7890"
git config --global https.proxy "http://127.0.0.1:7890"
```

### 2.3 配置 pip 代理

```bash
mkdir -p ~/.config/pip
cat > ~/.config/pip/pip.conf << 'EOF'
[global]
proxy = http://127.0.0.1:7890
EOF
```

> **注意**：部署完成后如需取消代理：
> ```bash
> git config --global --unset http.proxy
> git config --global --unset https.proxy
> unset HTTP_PROXY HTTPS_PROXY http_proxy https_proxy
> ```

---

## 3. 系统依赖安装

首先更新系统并安装 ESP-IDF 所需的基础依赖包：

```bash
sudo apt update && sudo apt upgrade -y

sudo apt install -y \
    git wget flex bison gperf \
    python3 python3-pip python3-venv \
    cmake ninja-build ccache \
    libffi-dev libssl-dev \
    dfu-util libusb-1.0-0 \
    curl unzip xz-utils
```

> **说明**：  
> - `dfu-util` / `libusb-1.0-0`：USB 烧录工具  
> - `cmake` / `ninja-build`：构建系统  
> - `ccache`：加速重复编译  
> - `python3-venv`：ESP-IDF 使用 Python 虚拟环境

---

## 4. 安装 ESP-IDF 框架

### 3.1 克隆 ESP-IDF 仓库

> 推荐安装路径：`~/esp/esp-idf`

```bash
mkdir -p ~/esp
cd ~/esp

# 克隆 ESP-IDF（以 v5.4 为例，也可选 v5.3 LTS）
git clone --recursive https://github.com/espressif/esp-idf.git -b v5.4
```

> 如果克隆速度慢，可使用国内镜像：
> ```bash
> git clone --recursive https://gitee.com/EspressifSystems/esp-idf.git -b v5.4
> ```

### 3.2 运行安装脚本

```bash
cd ~/esp/esp-idf

# 安装工具链（下载到 ~/.espressif）
./install.sh esp32,esp32s3,esp32c3
```

> 参数说明：  
> - `esp32`：经典 ESP32（Xtensa 双核）  
> - `esp32s3`：ESP32-S3（带 AI 加速）  
> - `esp32c3`：ESP32-C3（RISC-V 单核）  
> - 用 `all` 安装全部芯片支持（约 3-5 GB）

安装完成后，终端会提示类似：
```
All done! You can now run:
  . ~/esp/esp-idf/export.sh
```

### 3.3 设置环境变量（永久生效）

将以下内容追加到 `~/.bashrc`，每次打开终端自动加载 ESP-IDF 环境：

```bash
cat >> ~/.bashrc << 'EOF'

# === ESP-IDF 环境 ===
alias get_idf='. ~/esp/esp-idf/export.sh'
EOF
```

重新加载配置：

```bash
source ~/.bashrc
get_idf
```

验证安装成功：

```bash
idf.py --version
# 应输出类似: ESP-IDF v5.4
```

---

## 5. 安装 VS Code 及 ESP-IDF 扩展

### 4.1 安装 VS Code

如果尚未安装 VS Code：

```bash
# 方法一：通过 Snap（推荐）
sudo snap install --classic code

# 方法二：通过 apt（官方源）
wget -qO- https://packages.microsoft.com/keys/microsoft.asc | gpg --dearmor > packages.microsoft.gpg
sudo install -o root -g root -m 644 packages.microsoft.gpg /etc/apt/trusted.gpg.d/
sudo sh -c 'echo "deb [arch=amd64] https://packages.microsoft.com/repos/code stable main" > /etc/apt/sources.list.d/vscode.list'
rm -f packages.microsoft.gpg
sudo apt update
sudo apt install -y code
```

### 4.2 安装 ESP-IDF 扩展

1. 打开 VS Code
2. 进入扩展市场 (`Ctrl+Shift+X`)
3. 搜索 **"ESP-IDF"**（作者：Espressif Systems）
4. 点击 **安装**

> 或者命令行安装：
> ```bash
> code --install-extension espressif.esp-idf-extension
> ```

安装完成后重启 VS Code。

---

## 6. 配置 ESP-IDF 扩展

首次使用 ESP-IDF 扩展时需要配置各项路径。

### 5.1 通过命令面板配置

在 VS Code 中按 `Ctrl+Shift+P`，搜索并执行：

```
ESP-IDF: Configure ESP-IDF Extension
```

### 5.2 配置向导步骤

| 步骤 | 设置项 | 推荐值 |
|------|--------|--------|
| **Setup Mode** | 配置模式 | `Use Existing Setup` |
| **ESP-IDF Path** | IDF 路径 | `~/esp/esp-idf` |
| **ESP-IDF Tools Path** | 工具链路径 | `~/.espressif` |
| **Python Path** | Python 路径 | 系统自动检测（`/usr/bin/python3`） |
| **CMake Path** | CMake 路径 | 系统自动检测 |

> 如果选择 `Express Install`，扩展会自动下载安装一切（约需 30-60 分钟，取决于网络）。  
> 已经手动安装过 IDF 则选择 `Use Existing Setup`。

### 5.3 验证配置

按 `Ctrl+Shift+P` → 执行：

```
ESP-IDF: Doctor Command
```

应看到所有检查项均为 ✅ 绿色通过。

---

## 7. USB 串口权限配置

ESP32 开发板通过 USB 转串口芯片（CP2102/CH340/FT232 等）连接到电脑。

### 6.1 添加 udev 规则

```bash
# 将当前用户加入 dialout 组
sudo usermod -a -G dialout $USER

# 添加 udev 规则以自动赋予权限
cat << 'EOF' | sudo tee /etc/udev/rules.d/99-esp32.rules
# CP210x
SUBSYSTEM=="usb", ATTR{idVendor}=="10c4", ATTR{idProduct}=="ea60", MODE="0666"
# CH340
SUBSYSTEM=="usb", ATTR{idVendor}=="1a86", ATTR{idProduct}=="7523", MODE="0666"
# FTDI
SUBSYSTEM=="usb", ATTR{idVendor}=="0403", ATTR{idProduct}=="6001", MODE="0666"
# Espressif USB JTAG (ESP32-C3/S3 内置)
SUBSYSTEM=="usb", ATTR{idVendor}=="303a", ATTR{idProduct}=="1001", MODE="0666"
EOF

# 重新加载 udev 规则
sudo udevadm control --reload-rules
sudo udevadm trigger
```

### 6.3 验证串口识别

插入 ESP32 开发板 USB 线后：

```bash
# 查看串口设备
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
# 通常显示: /dev/ttyUSB0 或 /dev/ttyACM0

# 查看设备详细信息
dmesg | grep -i "usb\|tty" | tail -10
```

---

## 8. 创建并编译第一个项目

### 7.1 通过 VS Code 创建项目

1. 按 `Ctrl+Shift+P` → 执行：

   ```
   ESP-IDF: Show Examples Projects
   ```
   或

   ```
   ESP-IDF: New Project
   ```

2. 选择一个示例（如 `blink` 或 `hello_world`）
3. 选择目标文件夹（如 `~/esp32/projects`）
4. 点击 `Create`

### 7.2 设置目标芯片

在 VS Code 底部状态栏点击芯片图标（Target），选择你的芯片型号（如 `esp32`）。

或命令行方式：

```bash
cd ~/esp32/projects/hello_world
idf.py set-target esp32
```

### 7.3 编译项目

**方法一（VS Code 图形界面）**：  
点击底部状态栏的 🔨 **Build** 按钮。

**方法二（命令行）**：

```bash
cd ~/esp32/projects/hello_world
get_idf               # 加载 IDF 环境
idf.py build          # 编译
```

首次编译会较慢（约 1-3 分钟），后续增量编译仅需数秒。

编译成功后输出类似：

```
Project build complete.
```

---

## 9. 烧录与串口监视

### 8.1 烧录固件

**方法一（VS Code）**：  
底部状态栏点击 🔥 **Flash** 按钮，选择串口后烧录。

**方法二（命令行）**：

```bash
idf.py -p /dev/ttyUSB0 flash
```

### 8.2 串口监视器

**方法一（VS Code）**：  
底部状态栏点击 📺 **Monitor** 按钮。

**方法二（命令行）**：

```bash
idf.py -p /dev/ttyUSB0 monitor
```

> 退出监视器：`Ctrl + ]`

### 8.3 一键编译+烧录+监视

```bash
idf.py -p /dev/ttyUSB0 build flash monitor
```

---

## 10. 常见问题排查

### 9.1 找不到 idf.py 命令

```bash
# 手动加载环境
. ~/esp/esp-idf/export.sh

# 或使用别名
get_idf
```

### 9.2 串口权限拒绝 (`Permission denied`)

```bash
# 确认用户已加入 dialout 组
groups $USER | grep dialout

# 如果没有，执行：
sudo usermod -a -G dialout $USER

# ⚠️ 重要：修改用户组后必须注销重新登录才生效！
```

### 9.3 烧录失败：设备未连接

1. 确认 USB 线缆支持数据传输（部分充电线不支持）
2. 某些开发板需要按住 BOOT 按钮再上电进入下载模式
3. 检查 `dmesg` 输出确认系统正确识别设备

### 9.4 Python 虚拟环境问题

```bash
# 确保安装了 venv
sudo apt install -y python3-venv

# 手动创建虚拟环境
python3 -m venv ~/esp/esp-idf/tools/esp_python_env
```

### 9.5 VS Code 扩展检测不到 ESP-IDF

```bash
# 重置扩展配置
rm -rf ~/.vscode/extensions/espressif.esp-idf-extension-*/esp_idf_vsc_ext.json
```

然后重新在 VS Code 中执行 `ESP-IDF: Configure ESP-IDF Extension`。

### 9.6 编译时 CMake 错误

```bash
# 清理构建并重试
idf.py fullclean
idf.py build
```

---

## 11. 附录：快捷脚本

将以下保存为 `~/esp32/setup_esp32_env.sh`，在新机器上一键配置环境：

```bash
#!/bin/bash
set -e

echo "=== ESP32 开发环境一键部署脚本 ==="

# 1. 系统依赖
sudo apt update
sudo apt install -y git wget flex bison gperf python3 python3-pip python3-venv \
    cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0 curl unzip xz-utils

# 2. 用户加入 dialout 组
sudo usermod -a -G dialout $USER

# 3. udev 规则
cat << 'EOF' | sudo tee /etc/udev/rules.d/99-esp32.rules
SUBSYSTEM=="usb", ATTR{idVendor}=="10c4", ATTR{idProduct}=="ea60", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="1a86", ATTR{idProduct}=="7523", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="0403", ATTR{idProduct}=="6001", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="303a", ATTR{idProduct}=="1001", MODE="0666"
EOF
sudo udevadm control --reload-rules
sudo udevadm trigger

# 4. 安装 VS Code（如果未安装）
if ! command -v code &> /dev/null; then
    sudo snap install --classic code
fi

# 5. 克隆 ESP-IDF
mkdir -p ~/esp
cd ~/esp
if [ ! -d "esp-idf" ]; then
    git clone --recursive https://github.com/espressif/esp-idf.git -b v5.4
fi

# 6. 安装工具链
cd ~/esp/esp-idf
./install.sh esp32,esp32s3,esp32c3

# 7. 安装 VS Code 扩展
code --install-extension espressif.esp-idf-extension

# 8. 设置别名
grep -q "get_idf" ~/.bashrc || echo "alias get_idf='. ~/esp/esp-idf/export.sh'" >> ~/.bashrc

echo ""
echo "=== 部署完成！ ==="
echo "请执行以下操作："
echo "1. 注销并重新登录（使 dialout 组生效）"
echo "2. 打开 VS Code"
echo "3. Ctrl+Shift+P → 'ESP-IDF: Configure ESP-IDF Extension'"
echo "4. 选择 'Use Existing Setup'，IDF 路径填 ~/esp/esp-idf"
echo "========================================="
```

使用方法：

```bash
chmod +x ~/esp32/setup_esp32_env.sh
./setup_esp32_env.sh
```

---

> **参考链接**：
> - [ESP-IDF 官方文档](https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32/index.html)
> - [VS Code ESP-IDF 扩展](https://github.com/espressif/vscode-esp-idf-extension)
> - [Espressif 官网](https://www.espressif.com/)
