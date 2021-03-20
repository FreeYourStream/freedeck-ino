#include <Arduino.h>

#define I2CPORT PORTD
// A bit set to 1 in the DDR is an output, 0 is an INPUT
#define I2CDDR DDRD
// setting a port instruction takes 1 clock cycle
#define I2C_CLK_LOW() I2CPORT = bOld
#define FILL_BUFFER()             \
	while (!Serial.available()) { \
		delay(1);                 \
	};

static inline void i2cByteOut(uint8_t b);
void i2cBegin(uint8_t addr);
void i2cWrite(uint8_t *pData, uint8_t bLen);
void i2cEnd();
static void I2CWrite(int iAddr, unsigned char *pData, int iLen);
void oledInit(uint8_t bAddr, int bFlip, int bInvert);
void oledShutdown();
static void oledWriteCommand(unsigned char c);
void oledSetContrast(unsigned char ucContrast);
static void oledSetPosition(int x, int y);
static void oledWriteDataBlock(unsigned char *ucBuf, int iLen);
int oledSetPixel(int x, int y, unsigned char ucColor);
void oledLoadBMPPart(uint8_t *pBMP, int bytes = 1024, int offset = 0);
void oledFill(unsigned char ucData);