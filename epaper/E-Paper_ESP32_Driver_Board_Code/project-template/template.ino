/**
 * 5.83inch e-Paper V2 项目模板
 * 
 * 使用说明:
 *   1. 将本文件夹复制到 ~/esp32/projects/ 下
 *   2. 将 template.ino 重命名为 项目名.ino（必须与文件夹同名）
 *   3. 修改 .vscode/arduino.json 中的 sketch 字段
 *   4. 如需显示图片，运行 dev-tools/convert_image.py 生成 ImageData.h/.cpp
 *   5. 编译烧录: bash dev-tools/upload.sh 项目目录/
 */

#include "DEV_Config.h"
#include "EPD.h"
#include "GUI_Paint.h"
// #include "ImageData.h"   // 如需显示图片，取消注释
#include <stdlib.h>

void setup() {
    // 初始化
    DEV_Module_Init();
    EPD_5in83_V2_Init_Fast();

    // 创建画布 (648x480, 1-bit)
    UBYTE *BlackImage;
    UDOUBLE Imagesize = ((EPD_5in83_V2_WIDTH % 8 == 0) ? (EPD_5in83_V2_WIDTH / 8) : (EPD_5in83_V2_WIDTH / 8 + 1)) * EPD_5in83_V2_HEIGHT;
    BlackImage = (UBYTE *)malloc(Imagesize);
    if (BlackImage == NULL) return;

    Paint_NewImage(BlackImage, EPD_5in83_V2_WIDTH, EPD_5in83_V2_HEIGHT, 0, WHITE);
    Paint_SelectImage(BlackImage);
    Paint_Clear(WHITE);

    // ==========================================
    //  在此处添加绘图代码
    // ==========================================

    // 示例：显示文字
    Paint_DrawString_EN(10, 10, "Hello e-Paper!", &Font20, BLACK, WHITE);
    Paint_DrawString_CN(10, 40, "你好墨水屏", &Font24CN, BLACK, WHITE);
    
    // 示例：显示图片（需先运行 convert_image.py 生成 ImageData 文件）
    // Paint_DrawBitMap(gImage_custom);

    // ==========================================

    // 刷新显示
    EPD_5in83_V2_Display(BlackImage);
    DEV_Delay_ms(500);

    // 休眠
    EPD_5in83_V2_Sleep();
    free(BlackImage);
    BlackImage = NULL;
}

void loop() {
    // 不需要循环
}
