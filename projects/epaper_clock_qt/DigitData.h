#pragma once
#include <Arduino.h>

#define DIGIT_W 48
#define DIGIT_H 72

extern const unsigned char gDigit_d_0[];
extern const unsigned char gDigit_d_1[];
extern const unsigned char gDigit_d_2[];
extern const unsigned char gDigit_d_3[];
extern const unsigned char gDigit_d_4[];
extern const unsigned char gDigit_d_5[];
extern const unsigned char gDigit_d_6[];
extern const unsigned char gDigit_d_7[];
extern const unsigned char gDigit_d_8[];
extern const unsigned char gDigit_d_9[];
extern const unsigned char gDigit_d_colon[];

// 根据字符获取位图指针和宽高
const unsigned char* getDigit(char c, int &w, int &h);
