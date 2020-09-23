// freedeck arduino code for flashing to atmega32u4 based arduinos
// and compatible Copyright (C) 2020 Kilian Gosewisch
//
// This program is free software: you can redistribute it and/or
// modify it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License,
// or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public
// License along with this program. If not, see
// <https://www.gnu.org/licenses/>.

#include <SPI.h>
#include <SdFat.h>
#include <avr/power.h>

#include "./settings.h"
#include "HID-Project.h"

#define BUTTON_DOWN 0

#define I2CPORT PORTD
// A bit set to 1 in the DDR is an output, 0 is an INPUT
#define I2CDDR DDRD
// setting a port instruction takes 1 clock cycle
#define I2C_CLK_LOW() I2CPORT = bOld
#define FILL_BUFFER() while (!Serial.available())

SdFat SD;
File configFile;

int currentPage = 0;
unsigned short int fileImageDataOffset = 0;
unsigned char imageCache[IMG_CACHE_SIZE];
uint8_t buttonIsUp[BD_COUNT] = {1};
uint32_t downTime[BD_COUNT] = {0};
uint8_t longPressed[BD_COUNT] = {0};
uint8_t pageChanged = 0;

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
		0x81, 0xff, 0xa4, 0xa6, 0xd5, 0x80, 0x8d, 0x14, 0xaf, 0x20, 0x00};

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

int getBitValue(int number, int place) {
	return (number & (1 << place)) >> place;
}

void setMuxAddress(int address) {
	int S0 = getBitValue(address, 0);
	digitalWrite(S0_PIN, S0);

#if BD_COUNT > 2
	int S1 = getBitValue(address, 1);
	digitalWrite(S1_PIN, S1);
#endif

#if BD_COUNT > 4
	int S2 = getBitValue(address, 2);
	digitalWrite(S2_PIN, S2);
#endif

#if BD_COUNT > 8
	int S3 = getBitValue(address, 3);
	digitalWrite(S3_PIN, S3);
#endif

	delay(1);  // wait for multiplexer to switch
}

void displayImage(int16_t imageNumber) {
	configFile.seekSet(fileImageDataOffset + imageNumber * 1024L);
	uint8_t byteI = 0;
	while (configFile.available() && byteI < (1024 / IMG_CACHE_SIZE)) {
		configFile.read(imageCache, IMG_CACHE_SIZE);
		oledLoadBMPPart(imageCache, IMG_CACHE_SIZE, byteI * IMG_CACHE_SIZE);
		byteI++;
	}
}

void loadPage(int16_t pageIndex) {
	currentPage = pageIndex;
	for (uint8_t j = 0; j < BD_COUNT; j++) {
		setMuxAddress(j);
		delay(1);
		displayImage(pageIndex * BD_COUNT + j);
	}
}

void executeButtonConfig(uint8_t buttonIndex, uint8_t buttonUp,
						 uint8_t secondary) {
	if (configFile) {
		// + 1 because of the 1 header row with 16 bytes
		configFile.seek((BD_COUNT * currentPage + buttonIndex + 1) * 16 +
						8 * secondary);
		uint8_t command;
		command = configFile.read();
		if (buttonUp == 1) {
			if (pageChanged) {
				pageChanged = 0;
				longPressed[buttonIndex] = 0;
				downTime[buttonIndex] = 0;
				return;
			}
			if (command == 0) {
				Keyboard.releaseAll();
			} else if (command == 3) {
				Consumer.releaseAll();
			}
			if (command >= 16) {
				if (longPressed[buttonIndex] == 1) {
					executeButtonConfig(buttonIndex, buttonUp, 1);
					longPressed[buttonIndex] = 0;
					downTime[buttonIndex] = 0;
				} else {
					longPressed[buttonIndex] = 0;
					downTime[buttonIndex] = 0;
					if (command == 17) {
						int16_t pageIndex;
						configFile.read(&pageIndex, 2);
						loadPage(pageIndex);
					} else if (command == 16) {
						uint8_t key;
						configFile.read(&key, 1);
						while (key != 0) {
							Keyboard.press(KeyboardKeycode(key));
							configFile.read(&key, 1);
							delay(1);
						}
						delay(25);
						Keyboard.releaseAll();
					} else if (command == 19) {
						uint16_t key;
						configFile.read(&key, 2);
						Consumer.press(key);
						delay(25);
						Consumer.releaseAll();
					}
				}
			}
		} else if (buttonUp == 0) {
			if (command == 1) {
				int16_t pageIndex;
				configFile.read(&pageIndex, 2);
				pageChanged = 1;
				loadPage(pageIndex);
			} else if (command == 0) {
				byte i = 0;
				uint8_t key;
				configFile.read(&key, 1);
				while (key != 0 && i++ < 7) {
					Keyboard.press(KeyboardKeycode(key));
					configFile.read(&key, 1);
					delay(1);
				}
			} else if (command == 3) {
				uint16_t key;
				configFile.read(&key, 2);
				Consumer.press(key);
			} else if (command == 4) {
				byte i = 0;
				uint8_t key;
				configFile.read(&key, 1);
				while (key != 0 && i++ < 15) {
					Keyboard.press(KeyboardKeycode(key));
					delay(8);
					if (key < 224) {
						Keyboard.releaseAll();
					}
					configFile.read(&key, 1);
				}
				Keyboard.releaseAll();
			}
			if (command >= 16) {
				if (longPressed[buttonIndex] == 1) {
					executeButtonConfig(buttonIndex, buttonUp, 1);
				} else {
					downTime[buttonIndex] = millis();
				}
			}
		}
	}
}
void checkButtonState(uint8_t buttonIndex) {
	setMuxAddress(buttonIndex);
	uint8_t state = digitalRead(6);
	uint32_t ms = millis();
	uint32_t duration = ms - downTime[buttonIndex];
	if (duration == ms) duration = 0;
	if (state != buttonIsUp[buttonIndex] ||
		(duration >= LONG_PRESS_DURATION && longPressed[buttonIndex] == 0 &&
		 buttonIsUp[buttonIndex] == BUTTON_DOWN)) {
		if (duration >= LONG_PRESS_DURATION) {
			longPressed[buttonIndex] = 1;
		}
		executeButtonConfig(buttonIndex, state, 0);
	}
	buttonIsUp[buttonIndex] = state;
}

void initAllDisplays() {
	for (uint8_t buttonIndex = 0; buttonIndex < BD_COUNT; buttonIndex++) {
		buttonIsUp[buttonIndex] = 1;
		downTime[buttonIndex] = 0;
		longPressed[buttonIndex] = 0;
		setMuxAddress(buttonIndex);
		delay(1);
		oledInit(0x3c, 0, 0);
		oledFill(255);
	}
}

void setGlobalContrast() {
	unsigned short c;
	configFile.seekSet(4);
	c = configFile.read();
	if (c == 0) c = 1;
	for (uint8_t buttonIndex = 0; buttonIndex < BD_COUNT; buttonIndex++) {
		setMuxAddress(buttonIndex);
		delay(1);
		oledSetContrast(c);
	}
}

void loadConfigFile() {
	configFile = SD.open(CONFIG_NAME, FILE_READ);
	configFile.seek(2);
	configFile.read(&fileImageDataOffset, 2);
	fileImageDataOffset = fileImageDataOffset * 16;
}

void initSdCard() {
	int i = 0;
	//, SD_SCK_MHZ(50)
	while (!SD.begin(SD_CS_PIN, SD_SCK_MHZ(50)) && i <= 100) {
		i++;
	}
	if (i == 100) {
		while (1)
			;
	}
}

void dumpConfigFileOverSerial() {
	configFile.seekSet(0);
	if (configFile.available()) {
		Serial.println(configFile.fileSize());
		byte buff[512];
		int available;
		do {
			available = configFile.read(buff, 512);
			Serial.write(buff, 512);
		} while (available >= 512);
	}
}

void renameConfigFile() {
	char const *path = "config.bak";
	if (SD.exists(path)) {
		SD.remove(path);
	}
	configFile.rename(SD.vwd(), path);
}

void saveNewConfigFileFromSerial() {
	configFile = SD.open(CONFIG_NAME, O_WRONLY | O_CREAT);
	long bytes = 0;
	char numberChars[10];
	FILL_BUFFER();
	byte fileSizeLoopCounter = 0;
	while (fileSizeLoopCounter < 9) {
		numberChars[fileSizeLoopCounter++] = Serial.read();
	};
	numberChars[9] = '\n';
	long fileSize = atol(numberChars);
	unsigned int length;
	FILL_BUFFER();
	do {
		unsigned int batchI = 0;
		FILL_BUFFER();
		byte input[512];
		length = Serial.readBytes(input, 512);
		if (length != 0) configFile.write(input, length);

	} while (length == 512);
	configFile.sync();
	configFile.close();
}

void postSetup() {
	loadConfigFile();
	setGlobalContrast();
	loadPage(0);
}
