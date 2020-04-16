//    freedeck arduino code for flashing to atmega32u4 based arduinos and compatible
//   Copyright (C) 2020  Kilian Gosewisch
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <SdFat.h>
#include <SPI.h>
#include "HID-Project.h"
#include <avr/power.h>
#define BD_COUNT 6
#define CS 10
#define FONT_NORMAL 0
#define FONT_LARGE 1
#define BUTTON_UP 1
#define BUTTON_DOWN 0
#define S0_PIN 7
#define S1_PIN 8
#define S2_PIN 9
#define S3_PIN 10
#define IMG_CACHE_SIZE 512
#define DELAY 0
#define BOOT_DELAY 0 // increase to 1500-1800 or higher if some displays dont startup right away
#define CONFIG_NAME "config.bin"

int currentPage = 0;
unsigned short int offset = 0;
unsigned char buffer_fache[IMG_CACHE_SIZE];
uint8_t up[BD_COUNT] = {1};
//uint16_t downTime[BD_COUNT] = {1};
uint8_t longPressed[BD_COUNT] = {0};
SdFat SD;
File configFile;

// some globals
static int iScreenOffset; // current write offset of screen data
static uint8_t oled_addr;
#define MAX_CACHE 32
static uint8_t bCache[MAX_CACHE] = {0x40}; // for faster character drawing
static uint8_t bEnd = 1;
static void oledWriteCommand(unsigned char c);

#define DIRECT_PORT
#define I2CPORT PORTD
// A bit set to 1 in the DDR is an output, 0 is an INPUT
#define I2CDDR DDRD

// Pin or port numbers for SDA and SCL
#define BB_SDA 2
#define BB_SCL 3

#ifdef F_CPU
#undef F_CPU
#define F_CPU 8000000L
#endif
#if F_CPU > 8000000L
#define I2C_CLK_LOW() I2CPORT &= ~(1 << BB_SCL) //compiles to cbi instruction taking 2 clock cycles, extending the clock pulse
#else
#define I2C_CLK_LOW() I2CPORT = bOld //setting a port instruction takes 1 clock cycle
#endif

//
// Transmit a uint8_t and ack bit
//
static inline void i2cByteOut(uint8_t b)
{
  uint8_t i;
  uint8_t bOld = I2CPORT & ~((1 << BB_SDA) | (1 << BB_SCL));
  for (i = 0; i < 8; i++)
  {
    bOld &= ~(1 << BB_SDA);
    if (b & 0x80)
      bOld |= (1 << BB_SDA);
    I2CPORT = bOld;
    I2CPORT |= (1 << BB_SCL);
    I2C_CLK_LOW();
    b <<= 1;
  }                                // for i
                                   // ack bit
  I2CPORT = bOld & ~(1 << BB_SDA); // set data low
  I2CPORT |= (1 << BB_SCL);        // toggle clock
  I2C_CLK_LOW();
} /* i2cByteOut() */

void i2cBegin(uint8_t addr)
{
  I2CPORT |= ((1 << BB_SDA) + (1 << BB_SCL));
  I2CDDR |= ((1 << BB_SDA) + (1 << BB_SCL));
  I2CPORT &= ~(1 << BB_SDA); // data line low first
  I2CPORT &= ~(1 << BB_SCL); // then clock line low is a START signal
  i2cByteOut(addr << 1);     // send the slave address
} /* i2cBegin() */

void i2cWrite(uint8_t *pData, uint8_t bLen)
{
  uint8_t i, b;
  uint8_t bOld = I2CPORT & ~((1 << BB_SDA) | (1 << BB_SCL));

  while (bLen--)
  {
    b = *pData++;
    if (b == 0 || b == 0xff) // special case can save time
    {
      bOld &= ~(1 << BB_SDA);
      if (b & 0x80)
        bOld |= (1 << BB_SDA);
      I2CPORT = bOld;
      for (i = 0; i < 8; i++)
      {
        I2CPORT |= (1 << BB_SCL); // just toggle SCL, SDA stays the same
        I2C_CLK_LOW();
      } // for i
    }
    else // normal uint8_t needs every bit tested
    {
      for (i = 0; i < 8; i++)
      {

        bOld &= ~(1 << BB_SDA);
        if (b & 0x80)
          bOld |= (1 << BB_SDA);

        I2CPORT = bOld;
        I2CPORT |= (1 << BB_SCL);
        I2C_CLK_LOW();
        b <<= 1;
      } // for i
    }
    // ACK bit seems to need to be set to 0, but SDA line doesn't need to be tri-state
    I2CPORT &= ~(1 << BB_SDA);
    I2CPORT |= (1 << BB_SCL); // toggle clock
    I2CPORT &= ~(1 << BB_SCL);
  } // for each uint8_t
} /* i2cWrite() */

//
// Send I2C STOP condition
//
void i2cEnd()
{
  I2CPORT &= ~(1 << BB_SDA);
  I2CPORT |= (1 << BB_SCL);
  I2CPORT |= (1 << BB_SDA);
  I2CDDR &= ~((1 << BB_SDA) | (1 << BB_SCL)); // let the lines float (tri-state)
} /* i2cEnd() */

// Wrapper function to write I2C data on Arduino
static void I2CWrite(int iAddr, unsigned char *pData, int iLen)
{
  i2cBegin(oled_addr);
  i2cWrite(pData, iLen);
  i2cEnd();
} /* I2CWrite() */

static void oledCachedFlush(void)
{
  I2CWrite(oled_addr, bCache, bEnd); // write the old data
  bEnd = 1;
} /* oledCachedFlush() */

static void oledCachedWrite(uint8_t *pData, uint8_t bLen)
{

  if (bEnd + bLen > MAX_CACHE) // need to flush it
  {
    oledCachedFlush(); // write the old data
  }
  memcpy(&bCache[bEnd], pData, bLen);
  bEnd += bLen;

} /* oledCachedWrite() */
//
// Initializes the OLED controller into "page mode"
//
void oledInit(uint8_t bAddr, int bFlip, int bInvert)
{
  unsigned char uc[4];
  unsigned char oled_initbuf[] = {0x00, 0xae, 0xa8, 0x3f, 0xd3, 0x00, 0x40, 0xa1, 0xc8,
                                  0xda, 0x12, 0x81, 0xff, 0xa4, 0xa6, 0xd5, 0x80, 0x8d, 0x14,
                                  0xaf, 0x20, 0x00};

  oled_addr = bAddr;
  I2CDDR &= ~(1 << BB_SDA);
  I2CDDR &= ~(1 << BB_SCL); // let them float high
  I2CPORT |= (1 << BB_SDA); // set both lines to get pulled up
  I2CPORT |= (1 << BB_SCL);

  I2CWrite(oled_addr, oled_initbuf, sizeof(oled_initbuf));
  if (bInvert)
  {
    uc[0] = 0;    // command
    uc[1] = 0xa7; // invert command
    I2CWrite(oled_addr, uc, 2);
  }
  if (bFlip) // rotate display 180
  {
    uc[0] = 0; // command
    uc[1] = 0xa0;
    I2CWrite(oled_addr, uc, 2);
    uc[1] = 0xc0;
    I2CWrite(oled_addr, uc, 2);
  }
} /* oledInit() */
//
// Sends a command to turn off the OLED display
//
void oledShutdown()
{
  oledWriteCommand(0xaE); // turn off OLED
}

// Send a single uint8_t command to the OLED controller
static void oledWriteCommand(unsigned char c)
{
  unsigned char buf[2];

  buf[0] = 0x00; // command introducer
  buf[1] = c;
  I2CWrite(oled_addr, buf, 2);
} /* oledWriteCommand() */

static void oledWriteCommand2(unsigned char c, unsigned char d)
{
  unsigned char buf[3];

  buf[0] = 0x00;
  buf[1] = c;
  buf[2] = d;
  I2CWrite(oled_addr, buf, 3);
} /* oledWriteCommand2() */

//
// Sets the brightness (0=off, 255=brightest)
//
void oledSetContrast(unsigned char ucContrast)
{
  oledWriteCommand2(0x81, ucContrast);
} /* oledSetContrast() */

//
// Send commands to position the "cursor" (aka memory write address)
// to the given row and column
//
static void oledSetPosition(int x, int y)
{
  oledWriteCommand(0xb0 | y);                // go to page Y
  oledWriteCommand(0x00 | (x & 0xf));        // // lower col addr
  oledWriteCommand(0x10 | ((x >> 4) & 0xf)); // upper col addr
  iScreenOffset = (y * 128) + x;
}

//
// Write a block of pixel data to the OLED
// Length can be anything from 1 to 1024 (whole display)
//
static void oledWriteDataBlock(unsigned char *ucBuf, int iLen)
{
  unsigned char ucTemp[iLen + 1];
  //////Serial.println(iLen);
  ucTemp[0] = 0x40; // data command
  memcpy(&ucTemp[1], ucBuf, iLen);
  I2CWrite(oled_addr, ucTemp, iLen + 1);
  // Keep a copy in local buffer
}

// Set (or clear) an individual pixel
// The local copy of the frame buffer is used to avoid
// reading data from the display controller
int oledSetPixel(int x, int y, unsigned char ucColor)
{
  int i;
  unsigned char uc, ucOld;

  i = ((y >> 3) * 128) + x;
  if (i < 0 || i > 1023) // off the screen
    return -1;

  uc = ucOld = 0;

  uc &= ~(0x1 << (y & 7));
  if (ucColor)
  {
    uc |= (0x1 << (y & 7));
  }
  if (uc != ucOld) // pixel changed
  {
    oledSetPosition(x, y >> 3);
    oledWriteDataBlock(&uc, 1);
  }
  return 0;
} /* oledSetPixel() */

//
// Invert font data
//
void InvertBytes(uint8_t *pData, uint8_t bLen)
{
  uint8_t i;
  for (i = 0; i < bLen; i++)
  {
    *pData = ~(*pData);
    pData++;
  }
} /* InvertBytes() */

//
// Load a 128x64 1-bpp Windows bitmap
// Pass the pointer to the beginning of the BMP file
// First pass version assumes a full screen bitmap
//
int oledLoadBMPPart(uint8_t *pBMP, int bytes = 1024, int offset = 0)
{
  int y; // offset to bitmap data
  int iPitch = 128;
  uint8_t factor = bytes / iPitch; //512/128 = 4
  oledSetPosition(0, offset / 16 / 8);
  for (y = 0; y < factor; y++) // 8 lines of 8 pixels
  {
    oledWriteDataBlock(&pBMP[y * iPitch], iPitch);
  } // for y
    //oledCachedFlush();
} /* oledLoadBMP() */
//
// Fill the frame buffer with a uint8_t pattern
// e.g. all off (0x00) or all on (0xff)
//
void oledFill(unsigned char ucData)
{
  int x, y;
  unsigned char temp[16];

  memset(temp, ucData, 16);
  for (y = 0; y < 8; y++)
  {
    oledSetPosition(0, y); // set to (0,Y)
    for (x = 0; x < 8; x++)
    {
      oledWriteDataBlock(temp, 16);
    } // for x
  }   // for y
}

int getBitValue(int number, int place)
{
  return (number & (1 << place)) >> place;
}

void setMuxAddress(int address)
{
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

  delayMicroseconds(10);
}

void displayImage(int16_t imageNumber)
{
  configFile.seekSet(offset + imageNumber * 1024L);
  uint8_t byteI = 0;
  while (configFile.available() && byteI < (1024 / IMG_CACHE_SIZE))
  {
    configFile.read(buffer_fache, IMG_CACHE_SIZE);
    oledLoadBMPPart(buffer_fache, IMG_CACHE_SIZE, byteI * IMG_CACHE_SIZE);
    byteI++;
  }
}

void executeButtonConfig(uint8_t buttonIndex)
{
  if (configFile)
  {
    // + 1 because of the 1 header row with 16 bytes
    configFile.seek((BD_COUNT * currentPage + buttonIndex + 1) * 16);
    uint8_t command;
    command = configFile.read();
    // if 1 then open another page
    if (command == 1)
    {
      int16_t pageIndex;
      configFile.read(&pageIndex, 2);
      loadPage(pageIndex);
    }
    // if 0 then send key commands
    if (command == 0)
    {
      uint16_t key;
      configFile.read(&key, 2);
      while (key != 0)
      {
        Keyboard.press(key);
        configFile.read(&key, 2);
        delay(5);
      }
      delay(50);
      Keyboard.releaseAll();
    }
    if (command == 2)
    {
      //do nothing
    }
    // consumer keys
    if (command == 3)
    {
      uint16_t key;
      configFile.read(&key, 2);
      Consumer.write(key);
    }
  }
}
void checkButtonUp(uint8_t buttonIndex)
{
  setMuxAddress(buttonIndex);
  delay(1);
  uint8_t state = digitalRead(6);
  if (state != up[buttonIndex])
  {
    if (state == BUTTON_DOWN)
    {
      if (longPressed[buttonIndex] == 1)
      {
        longPressed[buttonIndex] = 0;
        //loadConfig(configIndex, buttonIndex);
      }
      else
      {
        executeButtonConfig(buttonIndex);
      }
    } //else if(state == BUTTON_DOWN && longPressed[buttonIndex] == 0) {
      //downTime[buttonIndex] = millis()/10;
    //}
  }
  up[buttonIndex] = state;
}

/*void checkButtonLongPress(uint8_t buttonIndex) {
  uint8_t state = digitalRead(7+buttonIndex);
  if(state == 0 && !longPressed[buttonIndex]){
    
    uint16_t diff = (millis()/10)- downTime[buttonIndex];
    ////Serial.println(diff);
    if(diff >= 30) {
      longPressed[buttonIndex] = 1;
      //oledFill(0);
      ////Serial.print("Available: ");
      ////Serial.println(freeMemory());
      oledWriteString(0,0,"ASD");
    }
  }
}*/

void loadPage(int16_t pageIndex)
{
  currentPage = pageIndex;
  for (uint8_t j = 0; j < BD_COUNT; j++)
  {
    setMuxAddress(j);
    displayImage(pageIndex * BD_COUNT + j);
  }
}
void initAllDisplays()
{
  for (uint8_t buttonIndex = 0; buttonIndex < BD_COUNT; buttonIndex++)
  {
    setMuxAddress(buttonIndex);
    oledInit(0x3c, 0, 0);
    oledFill(255);
  }
}

void initSdCard()
{
  int i = 0;
  //, SD_SCK_MHZ(50)
  while (!SD.begin(CS, SD_SCK_MHZ(250)) && i <= 100)
  {
    i++;
  }
  if (i == 100)
  {
    while (1)
      ;
  }
  configFile = SD.open(CONFIG_NAME, FILE_READ);
  configFile.seek(2);
  configFile.read(&offset, 2);
  offset = offset * 16;
}

void setup()
{
  clock_prescale_set(clock_div_2);
  Serial.begin(115200);
  delay(BOOT_DELAY);
  Keyboard.begin();
  Consumer.begin();
  pinMode(6, INPUT_PULLUP);
  pinMode(S0_PIN, OUTPUT);
#if BD_COUNT > 2
  pinMode(S1_PIN, OUTPUT);
#endif
#if BD_COUNT > 4
  pinMode(S2_PIN, OUTPUT);
#endif
#if BD_COUNT > 8
  pinMode(S3_PIN, OUTPUT);
#endif
  initAllDisplays();
  initSdCard();
  loadPage(0);
}

void loop()
{
  for (uint8_t buttonIndex = 0; buttonIndex < BD_COUNT; buttonIndex++)
  {
    checkButtonUp(buttonIndex);
    //checkButtonLongPress(buttonIndex);
    //delay(1);
  }
}
