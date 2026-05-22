/**
 * 墨水屏桌面闹钟 — Qt风格移植版
 * 硬件: 5.83inch e-Paper V2 (648×480) + ESP32
 * 天气: uapis.cn (广州增城新塘)
 * 布局: 天气(上) → 大时间(中) → 日期 → Logo(下)
 */

#include "DEV_Config.h"
#include "EPD.h"
#include "GUI_Paint.h"
#include "ImageData.h"
#include "LogoOnly.h"
#include "DigitData.h"
#include "Ascii32Data.h"
#include "Font24CN_Clock.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

// ===================== WiFi & API =====================
const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASS = "YOUR_WIFI_PASSWORD";
const long   GMT_OFFSET = 8 * 3600;

const char *WX_HOST = "uapis.cn";
const char *WX_PATH = "/api/v1/misc/weather?city=增城";
const char *WX_KEY  = "YOUR_UAPIS_KEY";

// ===================== 时间间隔 =====================
const unsigned long WX_INTERVAL   = 10UL * 60 * 1000;
const unsigned long FULL_INTERVAL = 10UL * 60 * 1000;
const unsigned long NTP_INTERVAL  = 60UL * 60 * 1000;

// ===================== 全局状态 =====================
UBYTE *g_FullBuf = NULL, *g_TimeBuf = NULL;
bool g_wifiOk = false;
int  g_lastSec = -1;

char g_wx[32]   = "加载中";
char g_tmp[16]  = "--";
char g_hum[16]  = "--";
char g_wind[32] = "--";
char g_upd[32]  = "";

unsigned long g_lastWx, g_lastFull, g_lastNtp;

// ===================== 中文星期 =====================
const char *wdayCN(int d) {
    const char *tbl[] = {"日","一","二","三","四","五","六"};
    return tbl[d % 7];
}

// ===================== WiFi =====================
bool wifiConnect() {
    if (WiFi.status() == WL_CONNECTED) return true;
    Serial.print("WiFi");
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    int n = 0;
    while (WiFi.status() != WL_CONNECTED && n++ < 40) { delay(500); Serial.print("."); }
    bool ok = (WiFi.status() == WL_CONNECTED);
    if (ok) { Serial.println(" OK " + WiFi.localIP().toString()); g_wifiOk = true; }
    else    { Serial.println(" FAIL"); g_wifiOk = false; }
    return ok;
}

// ===================== NTP =====================
bool ntpSync() {
    configTime(GMT_OFFSET, 0, "ntp.aliyun.com", "ntp.ntsc.ac.cn", "pool.ntp.org");
    struct tm t; int r = 0;
    while (!getLocalTime(&t) && r++ < 20) { delay(500); Serial.print("T"); }
    if (r >= 20) { Serial.println(" NTP fail"); return false; }
    Serial.printf("NTP: %04d-%02d-%02d %02d:%02d:%02d\n",
        t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
    g_lastNtp = millis();
    return true;
}

// ===================== 天气 =====================
bool wxFetch() {
    if (!g_wifiOk) return false;
    HTTPClient http;
    http.setTimeout(10000);
    String url = "https://" + String(WX_HOST) + WX_PATH + "&key=" + WX_KEY;
    http.begin(url);
    int code = http.GET();
    if (code != 200) { Serial.printf("WX HTTP %d\n", code); http.end(); return false; }
    String payload = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, payload)) { Serial.println("WX json err"); return false; }

    const char *w  = doc["weather"]      | "?";
    float t        = doc["temperature"]  | -999.0f;
    int hum        = doc["humidity"]     | -1;
    const char *wd = doc["wind_direction"] | "";
    const char *wp = doc["wind_power"]   | "";

    snprintf(g_wx,   sizeof(g_wx),   "%s", w);
    snprintf(g_tmp,  sizeof(g_tmp),  "%.0f", t);  // 纯数字，后面拼中文"度"
    snprintf(g_hum,  sizeof(g_hum),  "%d%%", hum);
    snprintf(g_wind, sizeof(g_wind), "%s%s", wd, wp);
    // 用 ESP32 本地时间作为"更新时间"
    {
        struct tm now;
        if (getLocalTime(&now))
            snprintf(g_upd, sizeof(g_upd), "%02d:%02d", now.tm_hour, now.tm_min);
        else
            snprintf(g_upd, sizeof(g_upd), "--:--");
    }

    Serial.printf("天气: %s %s 湿%s 风[%s] 更[%s]\n",
        g_wx, g_tmp, g_hum, g_wind, g_upd);
    g_lastWx = millis();
    return true;
}

// ==================== 天气图标 ====================
int wxIconType() {
    String s = String(g_wx);
    if (s.indexOf("晴") >= 0)  return 0;
    if (s.indexOf("多云") >= 0) return 1;
    if (s.indexOf("阴") >= 0)   return 2;
    if (s.indexOf("雨") >= 0 || s.indexOf("阵") >= 0 || s.indexOf("雷") >= 0) return 3;
    if (s.indexOf("雪") >= 0)   return 4;
    if (s.indexOf("雾") >= 0 || s.indexOf("霾") >= 0) return 5;
    return 2;
}

// ==================== 32×32 ASCII 绘制（与中文字模同高） ====================
void drawAscii32(int x, int y, char c) {
    const unsigned char *bmp = getAscii32Bitmap(c);
    if (!bmp) return;
    for (int row = 0; row < ASCII32_H; row++) {
        for (int col = 0; col < ASCII32_W / 8; col++) {
            uint8_t byte = pgm_read_byte(&bmp[row * (ASCII32_W / 8) + col]);
            for (int bit = 0; bit < 8; bit++) {
                bool white = (byte >> (7 - bit)) & 1;
                if (!white)  // 只画黑色笔画，白色=透明不覆盖
                    Paint_SetPixel(x + col * 8 + bit, y + row, BLACK);
            }
        }
    }
}

// ==================== 混合文本绘制（中英文字符同高32px） ====================
void drawMixedText32(int x, int y, const char *str) {
    const char *p = str;
    int cx = x;
    while (*p) {
        if ((unsigned char)*p >= 0x80) {
            char ch[4] = {p[0], p[1], p[2], 0};
            Paint_DrawString_CN(cx, y, ch, &Font24CN_Clock, BLACK, WHITE);
            cx += 32;
            p += 3;
        } else {
            drawAscii32(cx, y, *p);
            cx += 22;
            p++;
        }
    }
}

// 计算混合文本像素宽度
int mixedTextWidth(const char *str) {
    int w = 0;
    while (*str) {
        w += ((unsigned char)*str >= 0x80) ? 32 : 22;
        str += ((unsigned char)*str >= 0x80) ? 3 : 1;
    }
    return w;
}

// 居中绘制混合文本
void drawMixedCentered(int y, const char *str, int areaW) {
    int tw = mixedTextWidth(str);
    int x = (areaW - tw) / 2;
    if (x < 0) x = 0;
    drawMixedText32(x, y, str);
}

void drawWeatherIcon(int iconType, int cx, int cy) {
    int r = 28;
    switch (iconType) {
    case 0: // 太阳
        for (int i = 0; i < 8; i++) {
            float a = i * 3.14159f / 4;
            int x1 = cx + cos(a) * (r + 3);
            int y1 = cy + sin(a) * (r + 3);
            int x2 = cx + cos(a) * (r + 12);
            int y2 = cy + sin(a) * (r + 12);
            Paint_DrawLine(x1, y1, x2, y2, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
        }
        Paint_DrawCircle(cx, cy, r, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
        break;
    case 1: // 晴转多云
        for (int i = 0; i < 6; i++) {
            float a = i * 3.14159f / 3;
            int x1 = cx - 10 + cos(a) * (r/2 + 3);
            int y1 = cy - 6 + sin(a) * (r/2 + 3);
            int x2 = cx - 10 + cos(a) * (r/2 + 8);
            int y2 = cy - 6 + sin(a) * (r/2 + 8);
            Paint_DrawLine(x1, y1, x2, y2, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
        }
        Paint_DrawCircle(cx - 10, cy - 6, r/2, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
        for (int i = 0; i < 5; i++) {
            int xo = cx + 4 - 10 + i * 10;
            int yo = cy + 4 + (i % 2) * 4;
            Paint_DrawCircle(xo, yo, r/3, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
            Paint_DrawCircle(xo, yo, r/3, WHITE, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
        }
        break;
    case 2: // 阴/云
        for (int i = 0; i < 5; i++) {
            int xo = cx - 16 + i * 9;
            int yo = cy + 4 + (i % 2) * 5;
            Paint_DrawCircle(xo, yo, r/3, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
            Paint_DrawCircle(xo, yo, r/3, WHITE, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
        }
        break;
    case 3: // 雨
        for (int i = 0; i < 5; i++) {
            int xo = cx - 16 + i * 9;
            int yo = cy - 6 + (i % 2) * 5;
            Paint_DrawCircle(xo, yo, r/3, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
            Paint_DrawCircle(xo, yo, r/3, WHITE, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
        }
        for (int i = 0; i < 3; i++)
            Paint_DrawLine(cx - 8 + i * 8, cy + 12, cx - 11 + i * 8, cy + 24, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
        break;
    case 4: // 雪
        for (int i = 0; i < 5; i++) {
            int xo = cx - 16 + i * 9;
            int yo = cy - 6 + (i % 2) * 5;
            Paint_DrawCircle(xo, yo, r/3, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
            Paint_DrawCircle(xo, yo, r/3, WHITE, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
        }
        for (int i = 0; i < 5; i++) {
            Paint_SetPixel(cx - 14 + i * 7, cy + 14, BLACK);
            Paint_SetPixel(cx - 11 + i * 7, cy + 20, BLACK);
        }
        break;
    case 5: // 雾
        for (int i = 0; i < 5; i++)
            Paint_DrawLine(cx - 22, cy - 6 + i * 7, cx + 22, cy - 6 + i * 7, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
        break;
    }
}

// ==================== 大号数字绘制 ====================
void drawBigDigit(int x, int y, char c) {
    const unsigned char *bmp = getDigitBitmap(c);
    if (!bmp) return;
    int bw = DIGIT_W / 8;
    for (int row = 0; row < DIGIT_H; row++) {
        for (int col = 0; col < bw; col++) {
            uint8_t byte = pgm_read_byte(&bmp[row * bw + col]);
            for (int bit = 0; bit < 8; bit++) {
                bool white = (byte >> (7 - bit)) & 1;
                Paint_SetPixel(x + col * 8 + bit, y + row, white ? WHITE : BLACK);
            }
        }
    }
}

// 手绘冒号（两个小圆点）
void drawColonDots(int x, int midY) {
    int dotR = 4;
    int spacing = 12;
    Paint_DrawCircle(x, midY - spacing, dotR, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawCircle(x, midY + spacing, dotR, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
}

void drawBigTime(int x, int y, const char *hhmmss) {
    int gap = 2;
    int colonW = 14;
    int midY = y + DIGIT_H / 2;

    drawBigDigit(x, y, hhmmss[0]);
    drawBigDigit(x + DIGIT_W + gap, y, hhmmss[1]);
    drawColonDots(x + 2*DIGIT_W + gap + colonW/2, midY);
    drawBigDigit(x + 2*DIGIT_W + gap + colonW, y, hhmmss[3]);
    drawBigDigit(x + 3*DIGIT_W + 2*gap + colonW, y, hhmmss[4]);
    drawColonDots(x + 4*DIGIT_W + 2*gap + colonW + colonW/2, midY);
    drawBigDigit(x + 4*DIGIT_W + 2*gap + 2*colonW, y, hhmmss[6]);
    drawBigDigit(x + 5*DIGIT_W + 3*gap + 2*colonW, y, hhmmss[7]);
}

int timeTotalWidth() {
    return DIGIT_W * 6 + 2 * 3 + 2 * 14;
}

void fullRefresh() {
    struct tm t;
    if (!getLocalTime(&t)) return;
    Serial.println("[FULL]");

    EPD_5in83_V2_Init();
    int W = EPD_5in83_V2_WIDTH, H = EPD_5in83_V2_HEIGHT;

    if (!g_FullBuf) g_FullBuf = (UBYTE *)malloc(((W + 7) / 8) * H);
    Paint_NewImage(g_FullBuf, W, H, 0, WHITE);
    Paint_SelectImage(g_FullBuf);
    Paint_Clear(WHITE);

    // ── 顶部: 天气栏 ──
    int iconCx = 38, iconCy = 56;
    drawWeatherIcon(wxIconType(), iconCx, iconCy);

    // 城市名(左) + 更新时间(右) 同行
    Paint_DrawString_CN(12, 6, "广州增城", &Font24CN_Clock, BLACK, WHITE);
    {
        char buf[80];
        snprintf(buf, sizeof(buf), "更新时间 %s", g_upd);
        int tw = mixedTextWidth(buf);
        drawMixedText32(W - 12 - tw, 6, buf);
    }

    // 第1行: 天气 + 温度 (居中)
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "天气%s  温度%s度", g_wx, g_tmp);
        drawMixedCentered(44, buf, W);
    }

    // 第2行: 湿度 + 风力 (居中)
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "湿度%s  %s", g_hum, g_wind);
        drawMixedCentered(78, buf, W);
    }

    // 分隔线
    Paint_DrawLine(12, 114, W - 12, 114, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

    // ── 中部: 大时间 ──
    char ts[16];
    snprintf(ts, sizeof(ts), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
    int digitTotalW = timeTotalWidth();
    int timeX = (W - digitTotalW) / 2;
    int timeY = 132;
    drawBigTime(timeX, timeY, ts);

    // ── 日期 ──
    int dateY = timeY + DIGIT_H + 10;
    char ds[80];
    snprintf(ds, sizeof(ds), "%d年%d月%d日 周%s",
        t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, wdayCN(t.tm_wday));
    const char *dp = ds;
    int totalW = 0;
    while (*dp) {
        totalW += ((unsigned char)*dp >= 0x80) ? 32 : 22;
        dp += ((unsigned char)*dp >= 0x80) ? 3 : 1;
    }
    int dateX = (W - totalW) / 2;
    if (dateX < 10) dateX = 10;
    drawMixedText32(dateX, dateY, ds);

    // 分隔线
    Paint_DrawLine(60, dateY + 42, W - 60, dateY + 42, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

    // ── 底部: Logo ──
    int logoBw = LOGO_ONLY_W / 8;
    for (int row = 0; row < LOGO_ONLY_H; row++) {
        for (int col = 0; col < logoBw; col++) {
            uint8_t byte = pgm_read_byte(&gImage_logoOnly[row * logoBw + col]);
            for (int bit = 0; bit < 8; bit++) {
                bool white = (byte >> (7 - bit)) & 1;
                Paint_SetPixel(LOGO_ONLY_X + col * 8 + bit, LOGO_ONLY_Y + row, white ? WHITE : BLACK);
            }
        }
    }

    EPD_5in83_V2_Display(g_FullBuf);
    EPD_5in83_V2_Sleep();

    g_lastFull = millis();
    g_lastSec  = t.tm_sec;
    Serial.println("[FULL OK]");
}

// ==================== 局部刷新时间 ====================
void partialTime() {
    struct tm t;
    if (!getLocalTime(&t)) return;
    if (t.tm_sec == g_lastSec) return;
    g_lastSec = t.tm_sec;

    int W = EPD_5in83_V2_WIDTH;
    int digitTotalW = timeTotalWidth();
    int timeX = (W - digitTotalW) / 2;
    int timeY = 132;

    int pw = digitTotalW + 16, ph = DIGIT_H + 8;
    int px = timeX - 8, py = timeY - 4;

    EPD_5in83_V2_Init_Part();

    if (!g_TimeBuf) g_TimeBuf = (UBYTE *)malloc(((pw + 7) / 8) * ph);
    Paint_NewImage(g_TimeBuf, pw, ph, 0, WHITE);
    Paint_SelectImage(g_TimeBuf);
    Paint_Clear(WHITE);

    char ts[16];
    snprintf(ts, sizeof(ts), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
    drawBigTime(8, 4, ts);

    EPD_5in83_V2_Display_Partial(g_TimeBuf, px, py, px + pw, py + ph);
    EPD_5in83_V2_Sleep();
}

// ===================== Setup =====================
void setup() {
    Serial.begin(115200); delay(100);
    Serial.println("\n=== 墨水屏桌面闹钟 (Qt风格) ===\n");
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
    if (g_wifiOk && now - g_lastNtp > NTP_INTERVAL) ntpSync();
    if (g_wifiOk && now - g_lastWx > WX_INTERVAL)  { if (wxFetch()) fullRefresh(); }
    if (now - g_lastFull > FULL_INTERVAL) fullRefresh();

    partialTime();
    delay(50);
}
