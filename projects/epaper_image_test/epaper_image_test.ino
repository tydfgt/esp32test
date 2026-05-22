#include "DEV_Config.h"
#include "EPD.h"
#include "GUI_Paint.h"
#include "ImageData.h"
#include <stdlib.h>

void setup() {
    printf("EPD_5IN83_V2 Custom Image Demo\r\n");
    DEV_Module_Init();

    printf("e-Paper Init...\r\n");
    EPD_5in83_V2_Init_Fast();

    // Create image buffer
    UBYTE *BlackImage;
    UDOUBLE Imagesize = ((EPD_5in83_V2_WIDTH % 8 == 0) ? (EPD_5in83_V2_WIDTH / 8) : (EPD_5in83_V2_WIDTH / 8 + 1)) * EPD_5in83_V2_HEIGHT;
    if ((BlackImage = (UBYTE *)malloc(Imagesize)) == NULL) {
        printf("Failed to allocate memory...\r\n");
        return;
    }

    printf("Drawing custom image...\r\n");
    Paint_NewImage(BlackImage, EPD_5in83_V2_WIDTH, EPD_5in83_V2_HEIGHT, 0, WHITE);
    Paint_SelectImage(BlackImage);
    Paint_Clear(WHITE);
    Paint_DrawBitMap(gImage_custom);

    printf("Displaying...\r\n");
    EPD_5in83_V2_Display(BlackImage);
    DEV_Delay_ms(1000);

    printf("Sleep...\r\n");
    EPD_5in83_V2_Sleep();
    free(BlackImage);
    BlackImage = NULL;
    printf("Done!\r\n");
}

void loop() {
    // Nothing to do
}
