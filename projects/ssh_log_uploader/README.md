# ESP32 SSH 日志上传器

基于 **LibSSH-ESP32** 的物联网 SSH 客户端，通过 SSH 协议向 Linux 服务器上传运行日志。

## 功能

| 功能 | 描述 |
|------|------|
| LED 闪烁 | GPIO 2 每秒翻转，累计计数 |
| SSH 上传 | 每 10 秒通过 `ssh_channel_request_exec` 追加日志 |
| 日志格式 | `[闪烁次数: N, 时间: YYYY-MM-DD HH:MM:SS]` |
| NTP 时间 | pool.ntp.org 自动同步 |
| 容错 | WiFi/SSH 自动重连 |

## 快速开始

### 1. 安装依赖

```bash
export PATH="$HOME/.local/bin:$PATH"
arduino-cli lib install "LibSSH-ESP32"
```

### 2. 修改配置

编辑 `ssh_log_uploader.ino`，填入你的 WiFi 和 SSH 凭据：

```cpp
const char* WIFI_SSID     = "<your-wifi-ssid>";
const char* WIFI_PASSWORD = "<your-wifi-password>";
const char* SSH_HOST      = "<your-server-ip>";
const char* SSH_USER      = "<ssh-username>";
const char* SSH_PASSWORD  = "<ssh-password>";
const char* REMOTE_LOG    = "/path/to/your/log.txt";
```

### 3. 编译烧录

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 -u -p /dev/ttyUSB0
```

### 4. 串口监视

```bash
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

## 架构

```
Arduino setup() → xTaskCreate(controlTask, 32KB 栈, Core 0)
Arduino loop()  → 空闲 (vTaskDelay)

controlTask():
  ├─ SPIFFS 挂载 (存储 known_hosts)
  ├─ WiFi 连接
  ├─ NTP 时间同步
  ├─ libssh_begin() → SSH 持久连接
  └─ while(1):
       ├─ LED 闪烁 (每 1s)
       ├─ SSH exec 上传 (每 10s)
       └─ WiFi 断线检测
```

## 资源占用

| 资源 | 已用 | 总量 |
|------|------|------|
| Flash | 1,122 KB (85%) | 1,310 KB |
| RAM | 58 KB (17%) | 327 KB |
| 栈 (sshTask) | 32 KB | - |

## 文件说明

| 文件 | 用途 |
|------|------|
| `ssh_log_uploader.ino` | 主程序 |
| `.vscode/arduino.json` | VS Code 配置 |
| `技术交接文档.md` | 开发交接手册 |
| `CSDN发布文章.md` | CSDN 技术文章 |
| `server_receiver.py` | HTTP 备选方案（已废弃） |

## 依赖

- **ESP32 Arduino Core** ≥ 3.3.8
- **LibSSH-ESP32** ≥ 5.8.0
- SPIFFS（内置）

## License

GNU Lesser General Public License v2.1 — 与 LibSSH-ESP32 保持一致。详见 [LICENSE](LICENSE)。
