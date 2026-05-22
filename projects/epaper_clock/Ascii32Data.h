// 32×32 ASCII 位图 (与中文字模同高)
#ifndef _ASCII32DATA_H_
#define _ASCII32DATA_H_

#include <Arduino.h>

#define ASCII32_W 32
#define ASCII32_H 32
#define ASCII32_BYTES 128
extern const unsigned char ascii32_30[];
extern const unsigned char ascii32_31[];
extern const unsigned char ascii32_32[];
extern const unsigned char ascii32_33[];
extern const unsigned char ascii32_34[];
extern const unsigned char ascii32_35[];
extern const unsigned char ascii32_36[];
extern const unsigned char ascii32_37[];
extern const unsigned char ascii32_38[];
extern const unsigned char ascii32_39[];
extern const unsigned char ascii32_25[];
extern const unsigned char ascii32_B0[];
extern const unsigned char ascii32_3A[];
extern const unsigned char ascii32_2D[];
extern const unsigned char ascii32_2F[];
extern const unsigned char ascii32_2E[];
extern const unsigned char ascii32_sp[];
const unsigned char* getAscii32Bitmap(char c);
#endif

