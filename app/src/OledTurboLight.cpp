#include "./OledTurboLight.h"
#include "../settings.h"

// some globals
static int iScreenOffset;  // current write offset of screen data
static uint8_t oled_addr;
static uint8_t bCache[MAX_CACHE] = {0x40};	// for faster character drawing
static uint8_t bEnd = 1;
static void oledWriteCommand(unsigned char c);


//
// Transmit a uint8_t and ack bit
//
static inline void i2cByteOut(uint8_t b) {
	uint8_t i;
	uint8_t bOld = I2CPORT & ~((1 << BB_SDA) | (1 << BB_SCL));
	for (i = 0; i < 8; i++) {
		bOld &= ~(1 << BB_SDA);
		if (b & 0x80) bOld |= (1 << BB_SDA);
		I2CPORT = bOld;
		delayMicroseconds(I2C_DELAY);
		I2CPORT |= (1 << BB_SCL);
		delayMicroseconds(I2C_DELAY);
		I2C_CLK_LOW();
		b <<= 1;
	}								  // for i
									  // ack bit
	I2CPORT = bOld & ~(1 << BB_SDA);  // set data low
	delayMicroseconds(I2C_DELAY);
	I2CPORT |= (1 << BB_SCL);  // toggle clock
	delayMicroseconds(I2C_DELAY);
	I2C_CLK_LOW();
} /* i2cByteOut() */

void i2cBegin(uint8_t addr) {
	I2CPORT |= ((1 << BB_SDA) + (1 << BB_SCL));
	I2CDDR |= ((1 << BB_SDA) + (1 << BB_SCL));
	I2CPORT &= ~(1 << BB_SDA);				 // data line low first
	delayMicroseconds((I2C_DELAY + 1) * 2);	 // compatibility reasons
	I2CPORT &= ~(1 << BB_SCL);	// then clock line low is a START signal
	i2cByteOut(addr << 1);		// send the slave address
} /* i2cBegin() */

void i2cWrite(uint8_t *pData, uint8_t bLen) {
	uint8_t i, b;
	uint8_t bOld = I2CPORT & ~((1 << BB_SDA) | (1 << BB_SCL));

	while (bLen--) {
		b = *pData++;
		if (b == 0 || b == 0xff) {	// special case can save time
			bOld &= ~(1 << BB_SDA);
			if (b & 0x80) bOld |= (1 << BB_SDA);
			I2CPORT = bOld;
			delayMicroseconds(I2C_DELAY);
			for (i = 0; i < 8; i++) {
				I2CPORT |=
					(1 << BB_SCL);	// just toggle SCL, SDA stays the same
				delayMicroseconds(I2C_DELAY);
				I2C_CLK_LOW();
			}	  // for i
		} else {  // normal uint8_t needs every bit tested
			for (i = 0; i < 8; i++) {
				bOld &= ~(1 << BB_SDA);
				if (b & 0x80) bOld |= (1 << BB_SDA);

				I2CPORT = bOld;
				delayMicroseconds(I2C_DELAY);
				I2CPORT |= (1 << BB_SCL);
				delayMicroseconds(I2C_DELAY);
				I2C_CLK_LOW();
				b <<= 1;
			}  // for i
		}
		// ACK bit seems to need to be set to 0, but SDA
		// line doesn't need to be tri-state
		I2CPORT &= ~(1 << BB_SDA);
		I2CPORT |= (1 << BB_SCL);  // toggle clock
		I2CPORT &= ~(1 << BB_SCL);
	}  // for each uint8_t
} /* i2cWrite() */

//
// Send I2C STOP condition
//
void i2cEnd() {
	I2CPORT &= ~(1 << BB_SDA);
	I2CPORT |= (1 << BB_SCL);
	I2CPORT |= (1 << BB_SDA);
	// let the lines float (tri-state)
	I2CDDR &= ~((1 << BB_SDA) | (1 << BB_SCL));
} /* i2cEnd() */

// Wrapper function to write I2C data on Arduino
static void I2CWrite(int iAddr, unsigned char *pData, int iLen) {
	i2cBegin(oled_addr);
	i2cWrite(pData, iLen);
	i2cEnd();
} /* I2CWrite() */

//
// Initializes the OLED controller into "page mode"
//
void oledInit(uint8_t bAddr, int bFlip, int bInvert) {
	unsigned char uc[4];
	unsigned char oled_initbuf[] = {
		0x00, 0xae, 0xa8, 0x3f, 0xd3, 0x00, 0x40, 0xa1, 0xc8, 0xda, 0x12,
		0x81, 0xff, 0xa4, 0xa6, 0xd5, REFRESH_FREQUENCY, 0x8d, 0x14, 0xaf, 0x20, 0x00, 0xd9, PRE_CHARGE_PERIOD, 0xdb, MINIMUM_BRIGHTNESS};

	oled_addr = bAddr;
	I2CDDR &= ~(1 << BB_SDA);
	I2CDDR &= ~(1 << BB_SCL);  // let them float high
	I2CPORT |= (1 << BB_SDA);  // set both lines to get pulled up
	// delayMicroseconds(I2C_DELAY); // compatibility reasons
	I2CPORT |= (1 << BB_SCL);

	I2CWrite(oled_addr, oled_initbuf, sizeof(oled_initbuf));
	if (bInvert) {
		uc[0] = 0;	   // command
		uc[1] = 0xa7;  // invert command
		I2CWrite(oled_addr, uc, 2);
	}
	if (bFlip) {	// rotate display 180
		uc[0] = 0;	// command
		uc[1] = 0xa0;
		I2CWrite(oled_addr, uc, 2);
		uc[1] = 0xc0;
		I2CWrite(oled_addr, uc, 2);
	}
} /* oledInit() */
//
// Sends a command to turn off the OLED display
//
void oledShutdown() {
	oledWriteCommand(0xaE);	 // turn off OLED
}

// Send a single uint8_t command to the OLED controller
static void oledWriteCommand(unsigned char c) {
	unsigned char buf[2];

	buf[0] = 0x00;	// command introducer
	buf[1] = c;
	I2CWrite(oled_addr, buf, 2);
} /* oledWriteCommand() */

static void oledWriteCommand2(unsigned char c, unsigned char d) {
	unsigned char buf[3];

	buf[0] = 0x00;
	buf[1] = c;
	buf[2] = d;
	I2CWrite(oled_addr, buf, 3);
} /* oledWriteCommand2() */

//
// Sets the brightness (0=off, 255=brightest)
//
void oledSetContrast(unsigned char ucContrast) {
	oledWriteCommand2(0x81, ucContrast);
} /* oledSetContrast() */

//
// Send commands to position the "cursor" (aka memory write address)
// to the given row and column
//
static void oledSetPosition(int x, int y) {
	oledWriteCommand(0xb0 | y);					// go to page Y
	oledWriteCommand(0x00 | (x & 0xf));			// // lower col addr
	oledWriteCommand(0x10 | ((x >> 4) & 0xf));	// upper col addr
	iScreenOffset = (y * 128) + x;
}

//
// Write a block of pixel data to the OLED
// Length can be anything from 1 to 1024 (whole display)
//
static void oledWriteDataBlock(unsigned char *ucBuf, int iLen) {
	unsigned char ucTemp[iLen + 1];
	ucTemp[0] = 0x40;  // data command
	memcpy(&ucTemp[1], ucBuf, iLen);
	I2CWrite(oled_addr, ucTemp, iLen + 1);
	// Keep a copy in local buffer
}

// Set (or clear) an individual pixel
// The local copy of the frame buffer is used to avoid
// reading data from the display controller
int oledSetPixel(int x, int y, unsigned char ucColor) {
	int i;
	unsigned char uc, ucOld;

	i = ((y >> 3) * 128) + x;
	if (i < 0 || i > 1023)	// off the screen
		return -1;

	uc = ucOld = 0;

	uc &= ~(0x1 << (y & 7));
	if (ucColor) {
		uc |= (0x1 << (y & 7));
	}
	if (uc != ucOld) {	// pixel changed
		oledSetPosition(x, y >> 3);
		oledWriteDataBlock(&uc, 1);
	}
	return 0;
} /* oledSetPixel() */

//
// Load a 128x64 1-bpp Windows bitmap
// Pass the pointer to the beginning of the BMP file
// First pass version assumes a full screen bitmap
//
void oledLoadBMPPart(uint8_t *pBMP, int bytes = 1024, int offset = 0) {
	int y;	// offset to bitmap data
	int iPitch = 128;
	uint8_t factor = bytes / iPitch;  // 512/128 = 4
	oledSetPosition(0, offset / 16 / 8);
	for (y = 0; y < factor; y++) {	// 8 lines of 8 pixels
		oledWriteDataBlock(&pBMP[y * iPitch], iPitch);
	}  // for y
	   // oledCachedFlush();
} /* oledLoadBMP() */
//
// Fill the frame buffer with a uint8_t pattern
// e.g. all off (0x00) or all on (0xff)
//
void oledFill(unsigned char ucData) {
	int x, y;
	unsigned char temp[16];

	memset(temp, ucData, 16);
	for (y = 0; y < 8; y++) {
		oledSetPosition(0, y);	// set to (0,Y)
		for (x = 0; x < 8; x++) {
			oledWriteDataBlock(temp, 16);
		}  // for x
	}	   // for y
}
