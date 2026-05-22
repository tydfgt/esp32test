#ifndef _GIFFRAMES_H_
#define _GIFFRAMES_H_

#include <Arduino.h>

#define GIF_FRAME_COUNT 40
#define GIF_FRAME_W 648
#define GIF_FRAME_H 364
#define GIF_FRAME_BYTES 29484
#define GIF_Y_OFFSET 58
#define GIF_X_OFFSET 0

extern const unsigned char gGifFrames[40][29484];

#endif
