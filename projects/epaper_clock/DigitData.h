// 大号数字位图 (84×148), 用于时钟时间显示
#ifndef _DIGITDATA_H_
#define _DIGITDATA_H_

#include <Arduino.h>

#define DIGIT_W 84
#define DIGIT_H 148
#define DIGIT_BYTES 1554
extern const unsigned char g_digit_0[];
extern const unsigned char g_digit_1[];
extern const unsigned char g_digit_2[];
extern const unsigned char g_digit_3[];
extern const unsigned char g_digit_4[];
extern const unsigned char g_digit_5[];
extern const unsigned char g_digit_6[];
extern const unsigned char g_digit_7[];
extern const unsigned char g_digit_8[];
extern const unsigned char g_digit_9[];
extern const unsigned char g_colon_bitmap[];
// 根据字符获取对应位图指针
inline const unsigned char* getDigitBitmap(char c) {
    if (c >= '0' && c <= '9') {
        static const unsigned char* const tbl[] = {
            g_digit_0,
            g_digit_1,
            g_digit_2,
            g_digit_3,
            g_digit_4,
            g_digit_5,
            g_digit_6,
            g_digit_7,
            g_digit_8,
            g_digit_9,
        };
        return tbl[c - '0'];
    }
    return nullptr;
}
#endif
