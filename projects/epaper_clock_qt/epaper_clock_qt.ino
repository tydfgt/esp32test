/**
 * 5.83inch e-Paper V2 — 桌面闹钟 (Qt风格移植)
 * 
 * 布局: 天气区 | 分隔线 | 大号时间 HH:mm:ss | 日期 | Logo
 * WiFi: ysdlab / NTP授时 / uapis.cn天气 (广州增城)
 * 
 * 刷新策略:
 *   - 全刷: 初始 + 每10分钟 + 天气更新时
 *   - 局刷: 每秒更新时间行
 */

#include "DEV_Config.h"
#include "EPD.h"
#include "GUI_Paint.h"
#include "LogoData.h"
#include "DigitData.h"
#include "FontClockCN.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

// ===================== 配置 =====================
const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASS = "YOUR_WIFI_PASSWORD";
const long   GMT_OFFSET = 8 * 3600;
const char *WX_HOST = "uapis.cn";
const char *WX_PATH = "/api/v1/misc/weather?city=增城";
const char *WX_KEY  = "YOUR_UAPIS_KEY";

// 刷新间隔 (ms)
const unsigned long WX_INTERVAL    = 30UL * 60 * 1000;
const unsigned long FULL_INTERVAL  = 10UL * 60 * 1000;
const unsigned long NTP_INTERVAL   = 60UL * 60 * 1000;

// 屏幕尺寸
#define SW  EPD_5in83_V2_WIDTH   // 648
#define SH  EPD_5in83_V2_HEIGHT  // 480

// ===================== 全局状态 =====================
UBYTE *g_FullBuf = NULL;
UBYTE *g_TimeBuf = NULL;
bool   g_wifiOk  = false;

// 天气数据
char g_wx[32]   = "加载中…";
char g_tmp[16]  = "--";
char g_hum[16]  = "--";
char g_wind[32] = "--";
char g_upd[32]  = "";
char g_wxCode[16] = "sun";   // sun/cloud-sun/cloud/rain/snow/fog

// 时钟
unsigned long g_lWx, g_lFull, g_lNtp;
int g_lSec = -1;

// ===================== 星期 =====================
const char *wdayCN(int d) {
    static const char *w[] = {"日","一","二","三","四","五","六"};
    return w[d % 7];
}

// ===================== WiFi =====================
bool wifiConnect() {
    if (WiFi.status() == WL_CONNECTED) return true;
    Serial.print("WiFi: ");
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    for (int n = 0; n < 40 && WiFi.status() != WL_CONNECTED; n++) {
        delay(500); Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(" OK " + WiFi.localIP().toString());
        g_wifiOk = true; return true;
    }
    Serial.println(" FAIL");
    g_wifiOk = false; return false;
}

// ===================== NTP =====================
bool ntpSync() {
    configTime(GMT_OFFSET, 0, "ntp.aliyun.com", "ntp.ntsc.ac.cn", "pool.ntp.org");
    struct tm t; int r = 0;
    while (!getLocalTime(&t) && r++ < 20) { delay(500); Serial.print("T"); }
    if (r >= 20) { Serial.println(" NTP fail"); return false; }
    Serial.printf("NTP: %d-%d-%d %02d:%02d:%02d\n",
        t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
    g_lNtp = millis();
    return true;
}

// ===================== 天气 =====================
const char *mapWeatherCode(const char *w) {
    if (strstr(w, "晴")) return "sun";
    if (strstr(w, "多云")) return "cloud-sun";
    if (strstr(w, "阴")) return "cloud";
    if (strstr(w, "雨") || strstr(w, "阵") || strstr(w, "雷")) return "rain";
    if (strstr(w, "雪")) return "snow";
    if (strstr(w, "雾") || strstr(w, "霾")) return "fog";
    return "cloud";
}

bool wxFetch() {
    if (!g_wifiOk) return false;
    HTTPClient http;
    http.setTimeout(10000);
    String url = "https://" + String(WX_HOST) + WX_PATH + "&key=" + WX_KEY;
    http.begin(url);
    int code = http.GET();
    if (code != 200) { Serial.printf("WX %d\n", code); http.end(); return false; }
    String payload = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, payload)) { Serial.println("WX json err"); return false; }

    const char *w  = doc["weather"] | "?";
    float t        = doc["temperature"] | -999.0f;
    int hum        = doc["humidity"] | -1;
    const char *wd = doc["wind_direction"] | "";
    const char *wp = doc["wind_power"] | "";
    const char *rt = doc["report_time"] | "";

    snprintf(g_wx,  sizeof(g_wx),  "%s", w);
    snprintf(g_tmp, sizeof(g_tmp), "%.0f°", t);
    snprintf(g_hum, sizeof(g_hum), "%d%%", hum);
    snprintf(g_wind,sizeof(g_wind),"%s%s", wd, wp);
    snprintf(g_upd, sizeof(g_upd), "%s", rt);
    snprintf(g_wxCode,sizeof(g_wxCode),"%s", mapWeatherCode(w));

    Serial.printf("天气: %s %s°C 湿度%s%% %s [%s]\n", g_wx, g_tmp, g_hum, g_wind, g_upd);
    g_lWx = millis();
    return true;
}

// ===================== 绘制Logo (底部居中) =====================
void drawLogo() {
    int lx = (SW - LOGO_W) / 2;
    int ly = SH - LOGO_H - 14;  // 距底部14px
    int bb = (LOGO_W + 7) / 8;
    for (int y = 0; y < LOGO_H; y++) {
        for (int x = 0; x < LOGO_W; x++) {
            int idx = y * bb + (x / 8);
            int bit = (pgm_read_byte(&gImage_logo[idx]) >> (7 - (x % 8))) & 1;
            Paint_SetPixel(lx + x, ly + y, bit ? WHITE : BLACK);
        }
    }
}

// ===================== 绘制天气图标 (48×48区域) =====================
void drawWeatherIcon(int ox, int oy, const char *code) {
    int cx = ox + 24, cy = oy + 24;

    if (strcmp(code, "sun") == 0) {
        // 太阳: 圆 + 8条光芒
        for (int i = 0; i < 8; i++) {
            float a = i * 3.14159f / 4;
            int x1 = cx + (int)(cos(a) * 13);
            int y1 = cy + (int)(sin(a) * 13);
            int x2 = cx + (int)(cos(a) * 19);
            int y2 = cy + (int)(sin(a) * 19);
            Paint_DrawLine(x1, y1, x2, y2, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
        }
        Paint_DrawCircle(cx, cy, 10, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    }
    else if (strcmp(code, "cloud-sun") == 0) {
        // 小太阳 + 云
        int sx = ox + 14, sy = oy + 16;
        for (int i = 0; i < 5; i++) {
            float a = i * 3.14159f * 2 / 5;
            int x1 = sx + (int)(cos(a) * 8);
            int y1 = sy + (int)(sin(a) * 8);
            int x2 = sx + (int)(cos(a) * 12);
            int y2 = sy + (int)(sin(a) * 12);
            Paint_DrawLine(x1, y1, x2, y2, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
        }
        Paint_DrawCircle(sx, sy, 6, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
        // 云 (3个交叠圆)
        Paint_DrawCircle(cx - 2, cy + 8, 8, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
        Paint_DrawCircle(cx + 6, cy + 6, 10, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
        Paint_DrawCircle(cx + 14, cy + 10, 7, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
        // 擦除重叠部分的线条 (用白色填充底部)
        Paint_DrawRectangle(ox, cy + 6, ox + 48, oy + 48, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    }
    else if (strcmp(code, "cloud") == 0) {
        Paint_DrawCircle(cx - 2, cy + 6, 9, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
        Paint_DrawCircle(cx + 8, cy + 2, 11, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
        Paint_DrawCircle(cx + 16, cy + 8, 8, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
        Paint_DrawRectangle(ox + 12, cy + 10, ox + 36, oy + 48, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    }
    else if (strcmp(code, "rain") == 0) {
        // 云
        Paint_DrawCircle(cx - 2, cy - 2, 9, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
        Paint_DrawCircle(cx + 8, cy - 6, 11, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
        Paint_DrawCircle(cx + 16, cy, 8, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
        Paint_DrawRectangle(ox + 12, cy, ox + 36, oy + 28, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
        // 雨滴
        Paint_DrawLine(cx - 6, cy + 10, cx - 10, cy + 20, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
        Paint_DrawLine(cx + 2, cy + 8, cx - 2, cy + 18, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
        Paint_DrawLine(cx + 10, cy + 12, cx + 6, cy + 22, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    }
    else if (strcmp(code, "snow") == 0) {
        Paint_DrawCircle(cx - 2, cy - 4, 9, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
        Paint_DrawCircle(cx + 8, cy - 8, 11, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
        Paint_DrawCircle(cx + 16, cy - 2, 8, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
        Paint_DrawRectangle(ox + 12, cy - 2, ox + 36, oy + 26, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
        // 雪花点
        for (int i = 0; i < 4; i++) {
            int sx = cx - 10 + i * 7;
            Paint_DrawPoint(sx, cy + 10, BLACK, DOT_PIXEL_3X3, DOT_FILL_AROUND);
            Paint_DrawPoint(sx + 3, cy + 17, BLACK, DOT_PIXEL_2X2, DOT_FILL_AROUND);
        }
    }
    else if (strcmp(code, "fog") == 0) {
        for (int i = 0; i < 5; i++) {
            int ly = oy + 8 + i * 8;
            int lw = 30 - abs(i - 2) * 6;
            Paint_DrawLine(cx - lw/2, ly, cx + lw/2, ly, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
        }
    }
}

// ===================== 绘制大号数字 =====================
void drawLargeDigit(int x, int y, char c) {
    int dw, dh;
    const unsigned char *bits = getDigit(c, dw, dh);
    int bb = (dw + 7) / 8;
    for (int row = 0; row < dh; row++) {
        for (int col = 0; col < dw; col++) {
            int idx = row * bb + (col / 8);
            int bit = (pgm_read_byte(&bits[idx]) >> (7 - (col % 8))) & 1;
            Paint_SetPixel(x + col, y + row, bit ? WHITE : BLACK);
        }
    }
}

void drawTimeStr(int y, const char *timeStr) {
    // timeStr: "HH:mm:ss" (8 chars)
    int digitW = DIGIT_W;   // 48
    int digitH = DIGIT_H;   // 72
    int colonW = 28;        // 冒号间距
    int gap = 7;            // 数字间距

    // 总宽度: 数字*6 + 冒号位*2 + 间距*7
    int totalW = digitW * 6 + colonW * 2 + gap * 7;
    int startX = (SW - totalW) / 2;

    for (int i = 0; i < 8; i++) {
        char ch = timeStr[i];
        if (ch == ':') {
            drawLargeDigit(startX, y, ':');
            startX += colonW + gap;
        } else {
            drawLargeDigit(startX, y, ch);
            startX += digitW + gap;
        }
    }
}

// ===================== 全屏刷新 =====================
void fullRefresh() {
    struct tm t;
    if (!getLocalTime(&t)) return;
    Serial.println("[FULL]");

    EPD_5in83_V2_Init();

    if (!g_FullBuf) g_FullBuf = (UBYTE *)malloc(((SW + 7) / 8) * SH);
    Paint_NewImage(g_FullBuf, SW, SH, 0, WHITE);
    Paint_SelectImage(g_FullBuf);
    Paint_Clear(WHITE);

    // ── 1. 天气区域 (y: 0 ~ 112) ──
    int wy = 8;

    // 天气图标 (48x48)
    drawWeatherIcon(16, wy + 4, g_wxCode);

    // 天气描述 + 温度
    char wxLine[128];
    snprintf(wxLine, sizeof(wxLine), "%s  %s°C", g_wx, g_tmp);
    Paint_DrawString_CN(76, wy + 8, wxLine, &FontClockCN, BLACK, WHITE);

    // 湿度 + 风力
    char infoLine[128];
    snprintf(infoLine, sizeof(infoLine), "湿度%s  %s风", g_hum, g_wind);
    Paint_DrawString_CN(76, wy + 44, infoLine, &FontClockCN, BLACK, WHITE);

    // 右侧城市名
    char cityLine[32] = "广州·增城";
    int cw = strlen(cityLine) * 16;  // ASCII半宽
    Paint_DrawString_CN(SW - cw - 16, wy + 12, cityLine, &FontClockCN, BLACK, WHITE);

    // ── 分隔线1 ──
    int sep1Y = wy + 84;
    Paint_DrawLine(20, sep1Y, SW - 20, sep1Y, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

    // ── 2. 大号时间 ──
    int timeY = sep1Y + 16;
    char ts[16];
    snprintf(ts, sizeof(ts), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
    drawTimeStr(timeY, ts);

    // ── 3. 日期 ──
    int dateY = timeY + DIGIT_H + 16;
    char ds[80];
    snprintf(ds, sizeof(ds), "%d年%d月%d日  周%s",
        t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, wdayCN(t.tm_wday));
    // 居中估算: 32px字体, 约12字符 ≈ 384px
    int dx = (SW - 400) / 2;
    Paint_DrawString_CN(dx > 0 ? dx : 10, dateY, ds, &FontClockCN, BLACK, WHITE);

    // ── 分隔线2 ──
    int sep2Y = dateY + 48;
    Paint_DrawLine(60, sep2Y, SW - 60, sep2Y, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

    // ── 4. Logo ──
    drawLogo();

    // 显示
    EPD_5in83_V2_Display(g_FullBuf);
    EPD_5in83_V2_Sleep();

    g_lFull = millis();
    g_lSec = t.tm_sec;
    Serial.println("[FULL DONE]");
}

// ===================== 局部刷新时间 =====================
void partialTime() {
    struct tm t;
    if (!getLocalTime(&t)) return;
    if (t.tm_sec == g_lSec) return;
    g_lSec = t.tm_sec;

    EPD_5in83_V2_Init_Part();

    // 时间区域: 648 × (DIGIT_H + 20)
    int pw = 648;
    int ph = DIGIT_H + 20;
    int px = 0;
    // 计算Y位置（与全刷一致）
    int sep1Y = 8 + 84;     // 天气区 + 分隔线
    int timeY = sep1Y + 16;
    int py = timeY - 4;

    if (!g_TimeBuf) g_TimeBuf = (UBYTE *)malloc(((pw + 7) / 8) * ph);
    Paint_NewImage(g_TimeBuf, pw, ph, 0, WHITE);
    Paint_SelectImage(g_TimeBuf);
    Paint_Clear(WHITE);

    char ts[16];
    snprintf(ts, sizeof(ts), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
    // 绘制时间到局部buffer (x偏移到居中位置)
    drawTimeStr(2, ts);  // y=2 留一点padding

    EPD_5in83_V2_Display_Partial(g_TimeBuf, px, py, px + pw, py + ph);
    EPD_5in83_V2_Sleep();
}

// ===================== Setup =====================
void setup() {
    Serial.begin(115200); delay(100);
    Serial.println("\n=== 墨水屏桌面闹钟 v4.0 (Qt风格) ===\n");
    DEV_Module_Init();

    if (wifiConnect()) { ntpSync(); wxFetch(); }
    fullRefresh();
    Serial.println("就绪\n");
}

// ===================== Loop =====================
void loop() {
    unsigned long now = millis();

    if (WiFi.status() != WL_CONNECTED) {
        g_wifiOk = false;
        if (wifiConnect()) { ntpSync(); wxFetch(); fullRefresh(); }
    }
    if (g_wifiOk && now - g_lNtp > NTP_INTERVAL) ntpSync();
    if (g_wifiOk && now - g_lWx > WX_INTERVAL) { if (wxFetch()) fullRefresh(); }
    if (now - g_lFull > FULL_INTERVAL) fullRefresh();

    partialTime();
    delay(80);
}
