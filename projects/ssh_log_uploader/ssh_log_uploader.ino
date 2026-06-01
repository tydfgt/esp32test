/*
 * ESP32 SSH 日志上传器  v2.1
 * -------------------------------
 * 功能：
 *   1. GPIO 2 (D2) LED 每秒闪烁一次
 *   2. 每 10 秒通过 SSH exec 远程命令追加日志到服务器
 *   3. 日志格式：闪烁次数 + 当前时间
 *   4. 串口 115200 实时报告运行状态
 *
 * 依赖：LibSSH-ESP32 v5.8.0
 * 硬件：ESP32 Dev Module (esp32:esp32:esp32)
 * 端口：/dev/ttyUSB0
 *
 * 架构：SSH 加密操作在独立 FreeRTOS 任务中运行（32KB 栈），
 *       避免默认 loop() 任务栈溢出。
 *
 * 使用前请修改下方的 WiFi 配置！
 */

#include <WiFi.h>
#include <SPIFFS.h>
#include "time.h"

// --- LibSSH-ESP32 ---
#include "libssh_esp32.h"
#include <libssh/libssh.h>

// ==================== FreeRTOS 栈大小 ====================
// SSH 加密握手需要较大栈空间
const unsigned int SSH_TASK_STACK = 32768;

// ==================== WiFi 配置（请修改为你的 WiFi） ====================
const char* WIFI_SSID     = "装备集团";
const char* WIFI_PASSWORD = "88888888";

// ==================== SSH 远程服务器配置 ====================
const char*  SSH_HOST     = "8.148.200.227";
const int    SSH_PORT     = 22;
const char*  SSH_USER     = "root";
const char*  SSH_PASSWORD = "Ysd_zbjt2025";
const char*  REMOTE_DIR   = "/root/esp32";
const char*  REMOTE_LOG   = "/root/esp32/esp32_log.txt";

// ==================== LED 配置 ====================
#define LED_PIN         2                    // ESP32 内置 LED (D2)

// ==================== NTP 时间配置 ====================
const char* NTP_SERVER          = "pool.ntp.org";
const long  GMT_OFFSET_SEC      = 8 * 3600;  // UTC+8 北京时间
const int   DAYLIGHT_OFFSET_SEC = 0;

// ==================== 时间间隔 ====================
const unsigned long BLINK_INTERVAL  = 1000;   // LED 闪烁间隔：1 秒
const unsigned long UPLOAD_INTERVAL = 10000;  // 日志上传间隔：10 秒

// ==================== 全局变量 ====================
unsigned long lastBlinkTime   = 0;
unsigned long lastUploadTime  = 0;
int           blinkCount      = 0;
bool          ledState        = false;
int           uploadFailCount = 0;
ssh_session   ssh_sess        = NULL;
bool          sshConnected    = false;
bool          wifiConnected   = false;

// ==================== 函数声明 ====================
void controlTask(void *pvParameter);
bool connectWiFi();
void syncTime();
String getCurrentTimeStr();
bool connectSSH();
void disconnectSSH();
bool sshExecCommand(const char* command);
void uploadLog();
void blinkLED();
void reportStatus(const String& msg, bool isError = false);
int  verify_knownhost_auto(ssh_session session);
int  authenticate_password(ssh_session session, const char* password);

// ==================== Arduino 初始化 ====================
void setup() {
    Serial.begin(115200);
    delay(500);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    Serial.println();
    Serial.println(F("╔══════════════════════════════════╗"));
    Serial.println(F("║  ESP32 SSH 日志上传器 v2.1      ║"));
    Serial.println(F("║  基于 LibSSH-ESP32              ║"));
    Serial.println(F("╚══════════════════════════════════╝"));

    // 创建 SSH 工作任务（独立大栈，避免 loop() 栈溢出）
    xTaskCreatePinnedToCore(
        controlTask,        // 任务函数
        "sshTask",          // 任务名称
        SSH_TASK_STACK,     // 栈大小 (32KB)
        NULL,               // 参数
        1,                  // 优先级
        NULL,               // 任务句柄
        0                   // 运行在 Core 0
    );
}

// ==================== Arduino 主循环（空闲） ====================
void loop() {
    vTaskDelay(60000 / portTICK_PERIOD_MS);  // 所有工作在 controlTask 中
}

// ==================== 主控制任务（大栈空间） ====================
void controlTask(void *pvParameter) {
    reportStatus("控制任务已启动 (栈: " + String(SSH_TASK_STACK) + " bytes)");
    reportStatus("LED 已初始化 (GPIO " + String(LED_PIN) + ")");

    // --- 挂载 SPIFFS（libssh 需要存储 known_hosts）---
    if (!SPIFFS.begin(true)) {
        reportStatus("SPIFFS 挂载失败！", true);
    } else {
        reportStatus("SPIFFS 挂载成功 (" + String(SPIFFS.totalBytes()) + " bytes)");
    }

    // --- 连接 WiFi ---
    if (connectWiFi()) {
        wifiConnected = true;
        reportStatus("WiFi 连接成功！IP: " + WiFi.localIP().toString());

        // 同步时间
        syncTime();

        // 初始化 SSH 库
        libssh_begin();
        reportStatus("LibSSH 已初始化");

        // 建立 SSH 连接
        sshConnected = connectSSH();
    } else {
        reportStatus("WiFi 连接失败！请检查 SSID 和密码", true);
    }

    lastBlinkTime  = millis();
    lastUploadTime = millis();

    reportStatus("系统就绪，开始运行...");
    Serial.println(F("──────────────────────────────────"));

    // --- 主工作循环 ---
    while (1) {
        unsigned long now = millis();

        // LED 闪烁：每秒一次
        if (now - lastBlinkTime >= BLINK_INTERVAL) {
            lastBlinkTime = now;
            blinkLED();
        }

        // 日志上传：每 10 秒一次
        if (now - lastUploadTime >= UPLOAD_INTERVAL) {
            lastUploadTime = now;
            if (wifiConnected) {
                uploadLog();
            }
        }

        // WiFi 断线重连
        if (wifiConnected && WiFi.status() != WL_CONNECTED) {
            reportStatus("WiFi 断开，尝试重连...", true);
            disconnectSSH();
            wifiConnected = connectWiFi();
            if (wifiConnected) {
                sshConnected = connectSSH();
            }
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);  // 100ms 循环周期
    }
}

// ==================== LED 闪烁 ====================
void blinkLED() {
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    blinkCount++;
}

// ==================== WiFi 连接 ====================
bool connectWiFi() {
    Serial.print(F("[WiFi] 正在连接 "));
    Serial.print(WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(F("."));
        attempts++;
    }
    Serial.println();
    return (WiFi.status() == WL_CONNECTED);
}

// ==================== NTP 时间同步 ====================
void syncTime() {
    reportStatus("正在同步 NTP 时间...");
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);

    struct tm timeinfo;
    int retry = 0;
    while (!getLocalTime(&timeinfo) && retry < 20) {
        delay(500);
        Serial.print(F("."));
        retry++;
    }
    Serial.println();

    if (retry < 20) {
        reportStatus("时间同步成功: " + getCurrentTimeStr());
    } else {
        reportStatus("时间同步失败！将使用系统运行时间", true);
    }
}

// ==================== 获取当前时间字符串 ====================
String getCurrentTimeStr() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
        return String(buf);
    }
    return "N/A";
}

// ==================== SSH: 自动接受主机密钥 ====================
int verify_knownhost_auto(ssh_session session) {
    ssh_key key = NULL;
    int rc;

    rc = ssh_get_server_publickey(session, &key);
    if (rc < 0) {
        return -1;
    }

    enum ssh_known_hosts_e state = ssh_session_is_known_server(session);

    switch (state) {
        case SSH_KNOWN_HOSTS_OK:
            SSH_KEY_FREE(key);
            return 0;  // 已知主机，安全

        case SSH_KNOWN_HOSTS_NOT_FOUND:
        case SSH_KNOWN_HOSTS_UNKNOWN:
            // 自动写入 known_hosts
            rc = ssh_session_update_known_hosts(session);
            SSH_KEY_FREE(key);
            if (rc == SSH_OK) {
                reportStatus("SSH 主机密钥已保存（首次连接）");
                return 0;
            }
            reportStatus("无法保存主机密钥", true);
            return -1;

        case SSH_KNOWN_HOSTS_CHANGED:
            reportStatus("⚠ 主机密钥已变更！可能遭受中间人攻击！", true);
            SSH_KEY_FREE(key);
            return -1;

        case SSH_KNOWN_HOSTS_OTHER:
            reportStatus("主机密钥类型不匹配", true);
            SSH_KEY_FREE(key);
            return -1;

        case SSH_KNOWN_HOSTS_ERROR:
            reportStatus("known_hosts 错误", true);
            SSH_KEY_FREE(key);
            return -1;

        default:
            SSH_KEY_FREE(key);
            return -1;
    }
}

// ==================== SSH: 密码认证 ====================
int authenticate_password(ssh_session session, const char* password) {
    int rc;

    // 尝试键盘交互认证
    rc = ssh_userauth_kbdint(session, NULL, NULL);
    while (rc == SSH_AUTH_INFO) {
        int nprompts = ssh_userauth_kbdint_getnprompts(session);
        if (strlen(password) != 0 && nprompts >= 1) {
            ssh_userauth_kbdint_setanswer(session, 0, password);
        }
        rc = ssh_userauth_kbdint(session, NULL, NULL);
    }

    if (rc == SSH_AUTH_SUCCESS) {
        return rc;
    }

    // 退而求其次：尝试 password 认证
    rc = ssh_userauth_password(session, NULL, password);
    return rc;
}

// ==================== SSH: 建立连接 ====================
bool connectSSH() {
    if (ssh_sess != NULL) {
        disconnectSSH();
    }

    reportStatus("正在建立 SSH 连接...");

    ssh_sess = ssh_new();
    if (ssh_sess == NULL) {
        reportStatus("ssh_new() 失败", true);
        return false;
    }

    // 设置连接选项
    ssh_options_set(ssh_sess, SSH_OPTIONS_HOST, SSH_HOST);
    ssh_options_set(ssh_sess, SSH_OPTIONS_USER, SSH_USER);
    int port = SSH_PORT;
    ssh_options_set(ssh_sess, SSH_OPTIONS_PORT, &port);

    // 连接
    int rc = ssh_connect(ssh_sess);
    if (rc != SSH_OK) {
        reportStatus("SSH 连接失败: " + String(ssh_get_error(ssh_sess)), true);
        ssh_free(ssh_sess);
        ssh_sess = NULL;
        return false;
    }
    reportStatus("SSH 连接已建立 → " + String(SSH_HOST) + ":" + String(SSH_PORT));

    // 验证主机密钥（自动接受新主机）
    if (verify_knownhost_auto(ssh_sess) < 0) {
        reportStatus("主机密钥验证失败", true);
        disconnectSSH();
        return false;
    }

    // 密码认证
    rc = authenticate_password(ssh_sess, SSH_PASSWORD);
    if (rc != SSH_AUTH_SUCCESS) {
        reportStatus("SSH 认证失败: " + String(ssh_get_error(ssh_sess)), true);
        disconnectSSH();
        return false;
    }
    reportStatus("SSH 认证成功 ✓");

    // 确保远程目录存在
    String mkdirCmd = "mkdir -p " + String(REMOTE_DIR);
    sshExecCommand(mkdirCmd.c_str());

    return true;
}

// ==================== SSH: 断开连接 ====================
void disconnectSSH() {
    if (ssh_sess != NULL) {
        ssh_disconnect(ssh_sess);
        ssh_free(ssh_sess);
        ssh_sess = NULL;
    }
    sshConnected = false;
    reportStatus("SSH 已断开");
}

// ==================== SSH: 执行远程命令 ====================
bool sshExecCommand(const char* command) {
    if (ssh_sess == NULL || !sshConnected) {
        return false;
    }

    ssh_channel channel = ssh_channel_new(ssh_sess);
    if (channel == NULL) {
        return false;
    }

    int rc = ssh_channel_open_session(channel);
    if (rc != SSH_OK) {
        ssh_channel_free(channel);
        return false;
    }

    rc = ssh_channel_request_exec(channel, command);
    if (rc != SSH_OK) {
        reportStatus("SSH exec 失败: " + String(ssh_get_error(ssh_sess)), true);
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        return false;
    }

    // 持续读取远程输出，直到命令执行完毕（收到 EOF）
    char buffer[256];
    int nbytes;
    String output = "";
    while ((nbytes = ssh_channel_read(channel, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[nbytes] = '\0';
        output += String(buffer);
    }
    // 同样读取 stderr
    while ((nbytes = ssh_channel_read(channel, buffer, sizeof(buffer) - 1, 1)) > 0) {
        buffer[nbytes] = '\0';
        output += String(buffer);
    }

    // 获取命令退出状态
    int exit_status = ssh_channel_get_exit_status(channel);

    ssh_channel_close(channel);
    ssh_channel_free(channel);

    if (exit_status != 0) {
        reportStatus("SSH cmd 退出码=" + String(exit_status) + " cmd=" + String(command), true);
        if (output.length() > 0) {
            reportStatus("SSH output: " + output, true);
        }
        return false;
    }

    return true;
}

// ==================== 上传日志（SSH exec 方式追加） ====================
void uploadLog() {
    String timeStr = getCurrentTimeStr();

    // 组装日志内容
    String logLine = "[闪烁次数: " + String(blinkCount) + ", 时间: " + timeStr + "]";

    // 串口报告
    reportStatus("上传中 → " + logLine);

    // 检查 WiFi
    if (WiFi.status() != WL_CONNECTED) {
        reportStatus("上传失败：WiFi 未连接", true);
        uploadFailCount++;
        sshConnected = false;
        if (connectWiFi()) {
            sshConnected = connectSSH();
        }
        return;
    }

    // 检查 SSH 连接，必要时重连
    if (!sshConnected || ssh_sess == NULL) {
        reportStatus("SSH 未连接，尝试重连...", true);
        sshConnected = connectSSH();
        if (!sshConnected) {
            reportStatus("SSH 重连失败", true);
            uploadFailCount++;
            return;
        }
    }

    // 构造 SSH 命令：追加日志到文件
    // 使用 echo 追加，对特殊字符做转义
    logLine.replace("\"", "\\\"");
    logLine.replace("`", "\\`");
    logLine.replace("$", "\\$");

    String command = "echo \"" + logLine + "\" >> " + String(REMOTE_LOG);

    if (sshExecCommand(command.c_str())) {
        reportStatus("上传成功 ✓ → " + String(REMOTE_LOG));
        uploadFailCount = 0;
    } else {
        reportStatus("SSH exec 失败，尝试重连...", true);
        sshConnected = connectSSH();
        uploadFailCount++;

        // 重连后重试一次
        if (sshConnected) {
            if (sshExecCommand(command.c_str())) {
                reportStatus("重试上传成功 ✓");
                uploadFailCount = 0;
                return;
            }
        }
        reportStatus("上传失败", true);
    }

    // 连续失败告警
    if (uploadFailCount >= 3) {
        reportStatus("⚠ 已连续 " + String(uploadFailCount) + " 次上传失败！将重启 SSH 连接", true);
        disconnectSSH();
        sshConnected = connectSSH();
        if (sshConnected) uploadFailCount = 0;
    }
}

// ==================== 状态报告（串口输出） ====================
void reportStatus(const String& msg, bool isError) {
    String prefix = isError ? "[ERROR] " : "[INFO]  ";

    unsigned long uptime = millis() / 1000;
    char buf[64];
    snprintf(buf, sizeof(buf), "[%5lus] ", uptime);

    Serial.print(buf);
    Serial.print(prefix);
    Serial.println(msg);
}
