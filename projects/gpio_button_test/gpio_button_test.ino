/**
 * ESP32 按键控制 LED + SSH 远程上报（FreeRTOS · 精简版）
 * --------------------------------------------------------
 * 硬件：按键 D18→GND，板载 LED D2
 *
 * 优化：去 SPIFFS、去 String 类、去 kbdint 回退 → 省 Flash
 */

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <WiFi.h>
#include "time.h"
#include "libssh_esp32.h"
#include <libssh/libssh.h>

// ==================== 硬件 & 参数 ====================
#define BTN_PIN         18
#define LED_PIN         2
#define DEBOUNCE_MS     50
#define LONG_PRESS_MS   1000
#define BLINK_MS        200
#define UPLOAD_MS       10000

// ==================== 配置 ====================
const char* WIFI_SSID     = "装备集团";
const char* WIFI_PASSWORD = "88888888";
const char* SSH_HOST      = "8.148.200.227";
const int   SSH_PORT      = 22;
const char* SSH_USER      = "root";
const char* SSH_PASSWORD  = "Ysd_zbjt2025";
const char* REMOTE_LOG    = "/root/esp32/esp32_blink_botton.log";
const char* NTP_SERVER    = "pool.ntp.org";
const long  GMT_OFFSET    = 8 * 3600;

// ==================== 类型 ====================
typedef enum { EVT_SHORT_PRESS = 0, EVT_LONG_PRESS = 1 } BtnEvent_t;
typedef enum { LED_OFF = 0, LED_ON = 1, LED_BLINK = 2 } LedState_t;

// ==================== 全局 ====================
static QueueHandle_t g_btnQueue   = NULL;
static volatile int  g_blinkCount = 0;

// ==================== 任务声明 ====================
static void btnTask(void *pvParams);
static void ledTask(void *pvParams);
static void sshTask(void *pvParams);

// ==================== SSH 辅助 ====================
static bool wifiConnect();
static bool ntpSync();
static void getTimeStr(char *buf, int len);
static bool sshConnect(ssh_session &sess);
static void sshDisconnect(ssh_session &sess);
static bool sshExec(ssh_session sess, const char* cmd);

// ==================== setup ====================
void setup() {
    Serial.begin(115200);
    delay(300);
    pinMode(BTN_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    Serial.println("\n===== 按键LED + SSH上报 =====");

    g_btnQueue = xQueueCreate(3, sizeof(BtnEvent_t));
    xTaskCreatePinnedToCore(btnTask, "btn", 2048, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(ledTask, "led", 2048, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(sshTask, "ssh", 24576, NULL, 1, NULL, 0);  // 24KB
}

void loop() { vTaskDelay(pdMS_TO_TICKS(1000)); }

// ==================== 按键任务 ====================
static void btnTask(void *pvParams) {
    int state = HIGH, lastRead = HIGH;
    unsigned long debounce = 0, pressStart = 0;
    bool longFired = false;
    for (;;) {
        int r = digitalRead(BTN_PIN);
        if (r != lastRead) debounce = millis();
        if ((millis() - debounce) > DEBOUNCE_MS) {
            if (r != state) {
                state = r;
                if (state == LOW) { pressStart = millis(); longFired = false; }
                else if (!longFired) { BtnEvent_t e = EVT_SHORT_PRESS; xQueueSend(g_btnQueue, &e, 0); }
            }
        }
        lastRead = r;
        if (state == LOW && !longFired && (millis() - pressStart > LONG_PRESS_MS)) {
            longFired = true;
            BtnEvent_t e = EVT_LONG_PRESS;
            xQueueSend(g_btnQueue, &e, 0);
            Serial.println("→ 长按触发");
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ==================== LED 任务 ====================
static void ledTask(void *pvParams) {
    LedState_t st = LED_OFF;
    bool lvl = false;
    BtnEvent_t evt;
    for (;;) {
        if (xQueueReceive(g_btnQueue, &evt, pdMS_TO_TICKS(BLINK_MS)) == pdTRUE) {
            switch (st) {
            case LED_OFF:
                if (evt == EVT_SHORT_PRESS) { st = LED_ON;  lvl = true;  Serial.println("LED ON"); }
                else                        { st = LED_BLINK;              Serial.println("LED BLINK"); }
                break;
            case LED_ON:
                if (evt == EVT_SHORT_PRESS) { st = LED_OFF; lvl = false; Serial.println("LED OFF"); }
                else                        { st = LED_BLINK;              Serial.println("LED BLINK"); }
                break;
            case LED_BLINK:
                if (evt == EVT_SHORT_PRESS) { st = LED_OFF; lvl = false; Serial.println("LED OFF"); }
                break;
            }
        }
        if (st == LED_BLINK) { lvl = !lvl; g_blinkCount++; }
        digitalWrite(LED_PIN, lvl ? HIGH : LOW);
    }
}

// ==================== SSH 上报任务 ====================
static void sshTask(void *pvParams) {
    ssh_session sess = NULL;
    bool sshOk = false;
    unsigned long lastUpload = 0;

    if (!wifiConnect()) {
        Serial.println("[SSH] WiFi fail");
    } else {
        ntpSync();
        libssh_begin();
        sshOk = sshConnect(sess);
    }

    for (;;) {
        unsigned long now = millis();

        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[SSH] WiFi lost, reconnect...");
            sshDisconnect(sess); sshOk = false;
            if (wifiConnect()) { ntpSync(); sshOk = sshConnect(sess); }
        }

        if (now - lastUpload >= UPLOAD_MS) {
            lastUpload = now;
            int count = g_blinkCount;

            char ts[32];
            getTimeStr(ts, sizeof(ts));

            char line[128];
            snprintf(line, sizeof(line), "[%s] blinks: %d", ts, count);
            Serial.printf("[上报] %s\n", line);

            if (sshOk && sess) {
                char cmd[256];
                snprintf(cmd, sizeof(cmd), "echo \"%s\" >> %s", line, REMOTE_LOG);
                if (!sshExec(sess, cmd)) {
                    Serial.println("[SSH] exec fail, reconnect...");
                    sshDisconnect(sess);
                    sshOk = sshConnect(sess);
                    if (sshOk) sshExec(sess, cmd);
                }
            } else if (!sshOk) {
                sshOk = sshConnect(sess);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// ==================== WiFi ====================
static bool wifiConnect() {
    Serial.printf("[WiFi] %s", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int n = 0;
    while (WiFi.status() != WL_CONNECTED && n++ < 40) { delay(500); Serial.print("."); }
    bool ok = (WiFi.status() == WL_CONNECTED);
    Serial.println(ok ? " OK" : " FAIL");
    return ok;
}

// ==================== NTP ====================
static bool ntpSync() {
    configTime(GMT_OFFSET, 0, NTP_SERVER);
    struct tm t; int r = 0;
    while (!getLocalTime(&t) && r++ < 20) { delay(500); Serial.print("T"); }
    if (r >= 20) { Serial.println(" NTP fail"); return false; }
    Serial.printf("NTP %02d:%02d:%02d\n", t.tm_hour, t.tm_min, t.tm_sec);
    return true;
}

// ==================== 时间字符串（C 版，不用 String） ====================
static void getTimeStr(char *buf, int len) {
    struct tm t;
    if (getLocalTime(&t))
        snprintf(buf, len, "%04d-%02d-%02d %02d:%02d:%02d",
            t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
    else
        snprintf(buf, len, "N/A");
}

// ==================== SSH 连接（精简：无 SPIFFS，纯密码认证） ====================
static bool sshConnect(ssh_session &sess) {
    sshDisconnect(sess);
    sess = ssh_new();
    if (!sess) return false;
    ssh_options_set(sess, SSH_OPTIONS_HOST, SSH_HOST);
    ssh_options_set(sess, SSH_OPTIONS_USER, SSH_USER);
    int port = SSH_PORT;
    ssh_options_set(sess, SSH_OPTIONS_PORT, &port);

    if (ssh_connect(sess) != SSH_OK) {
        Serial.printf("[SSH] conn fail: %s\n", ssh_get_error(sess));
        ssh_free(sess); sess = NULL; return false;
    }

    // 简化 known_hosts：首次接受，变更拒绝
    ssh_key key = NULL;
    if (ssh_get_server_publickey(sess, &key) >= 0) {
        enum ssh_known_hosts_e st = ssh_session_is_known_server(sess);
        SSH_KEY_FREE(key);
        if (st == SSH_KNOWN_HOSTS_CHANGED) {
            Serial.println("[SSH] host key changed!");
            sshDisconnect(sess); return false;
        }
    }

    // 纯密码认证
    if (ssh_userauth_password(sess, NULL, SSH_PASSWORD) != SSH_AUTH_SUCCESS) {
        Serial.printf("[SSH] auth fail: %s\n", ssh_get_error(sess));
        sshDisconnect(sess); return false;
    }
    Serial.println("[SSH] OK");
    return true;
}

static void sshDisconnect(ssh_session &sess) {
    if (sess) { ssh_disconnect(sess); ssh_free(sess); sess = NULL; }
}

static bool sshExec(ssh_session sess, const char* cmd) {
    if (!sess) return false;
    ssh_channel ch = ssh_channel_new(sess);
    if (!ch) return false;
    if (ssh_channel_open_session(ch) != SSH_OK) { ssh_channel_free(ch); return false; }
    if (ssh_channel_request_exec(ch, cmd) != SSH_OK) {
        ssh_channel_close(ch); ssh_channel_free(ch); return false;
    }
    char buf[64]; while (ssh_channel_read(ch, buf, sizeof(buf)-1, 0) > 0) {}
    ssh_channel_close(ch); ssh_channel_free(ch);
    return true;
}
