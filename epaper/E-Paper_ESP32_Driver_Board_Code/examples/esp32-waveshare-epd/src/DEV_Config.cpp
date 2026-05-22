/*****************************************************************************
* | File      	:   DEV_Config.c
* | Author      :   Waveshare team
* | Function    :   Hardware underlying interface
* | Info        :
*----------------
* |	This version:   V1.0
* | Date        :   2020-02-19
* | Info        :
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
******************************************************************************/
#include "DEV_Config.h"
#include "soc/gpio_reg.h"
#include "soc/io_mux_reg.h"

// 预计算的 SCK/MOSI/CS 位掩码（全部 < 32，用 GPIO_OUT_W1TS/TC_REG）
static const uint32_t SCK_MASK  = 1UL << EPD_SCK_PIN;
static const uint32_t MOSI_MASK = 1UL << EPD_MOSI_PIN;
static const uint32_t CS_MASK   = 1UL << EPD_CS_PIN;

void GPIO_Config(void)
{
    pinMode(EPD_BUSY_PIN,  INPUT);
    pinMode(EPD_RST_PIN , OUTPUT);
    pinMode(EPD_DC_PIN  , OUTPUT);

    pinMode(EPD_SCK_PIN, OUTPUT);
    pinMode(EPD_MOSI_PIN, OUTPUT);
    pinMode(EPD_CS_PIN , OUTPUT);

    digitalWrite(EPD_CS_PIN , HIGH);
    digitalWrite(EPD_SCK_PIN, LOW);
}

void GPIO_Mode(UWORD GPIO_Pin, UWORD Mode)
{
    if(Mode == 0) {
        pinMode(GPIO_Pin , INPUT);
	} else {
		pinMode(GPIO_Pin , OUTPUT);
	}
}
/******************************************************************************
function:	Module Initialize, the BCM2835 library and initialize the pins, SPI protocol
parameter:
Info:
******************************************************************************/
UBYTE DEV_Module_Init(void)
{
	//gpio
	GPIO_Config();

	//serial printf
	Serial.begin(115200);

	return 0;
}

/******************************************************************************
function:
			SPI write — 直接寄存器 bit-bang（比 digitalWrite 快 10-50 倍）
******************************************************************************/
void DEV_SPI_WriteByte(UBYTE data)
{
    WRITE_PERI_REG(GPIO_OUT_W1TC_REG, CS_MASK);  // CS LOW

    for (int i = 0; i < 8; i++)
    {
        if (data & 0x80) WRITE_PERI_REG(GPIO_OUT_W1TS_REG, MOSI_MASK);
        else             WRITE_PERI_REG(GPIO_OUT_W1TC_REG, MOSI_MASK);

        data <<= 1;
        WRITE_PERI_REG(GPIO_OUT_W1TS_REG, SCK_MASK);  // SCK HIGH
        WRITE_PERI_REG(GPIO_OUT_W1TC_REG, SCK_MASK);  // SCK LOW
    }

    WRITE_PERI_REG(GPIO_OUT_W1TS_REG, CS_MASK);  // CS HIGH
}

UBYTE DEV_SPI_ReadByte()
{
    UBYTE j=0xff;
    GPIO_Mode(EPD_MOSI_PIN, 0);
    WRITE_PERI_REG(GPIO_OUT_W1TC_REG, CS_MASK);  // CS LOW
    for (int i = 0; i < 8; i++)
    {
        j = j << 1;
        if (digitalRead(EPD_MOSI_PIN))  j = j | 0x01;
        else                            j = j & 0xfe;

        WRITE_PERI_REG(GPIO_OUT_W1TS_REG, SCK_MASK);  // SCK HIGH
        WRITE_PERI_REG(GPIO_OUT_W1TC_REG, SCK_MASK);  // SCK LOW
    }
    WRITE_PERI_REG(GPIO_OUT_W1TS_REG, CS_MASK);  // CS HIGH
    GPIO_Mode(EPD_MOSI_PIN, 1);
    return j;
}

void DEV_SPI_Write_nByte(UBYTE *pData, UDOUBLE len)
{
    WRITE_PERI_REG(GPIO_OUT_W1TC_REG, CS_MASK);  // CS LOW — 整块传输只切换一次
    for (UDOUBLE i = 0; i < len; i++)
    {
        UBYTE data = pData[i];
        for (int b = 0; b < 8; b++)
        {
            if (data & 0x80) WRITE_PERI_REG(GPIO_OUT_W1TS_REG, MOSI_MASK);
            else             WRITE_PERI_REG(GPIO_OUT_W1TC_REG, MOSI_MASK);
            data <<= 1;
            WRITE_PERI_REG(GPIO_OUT_W1TS_REG, SCK_MASK);
            WRITE_PERI_REG(GPIO_OUT_W1TC_REG, SCK_MASK);
        }
    }
    WRITE_PERI_REG(GPIO_OUT_W1TS_REG, CS_MASK);  // CS HIGH
}
