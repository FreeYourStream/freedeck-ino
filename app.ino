#include <SdFat.h>
#include <MemoryFree.h>
#include <SPI.h>
#include <Keyboard.h>
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
#define IMG_CACHE 256
#define DELAY 0
#define CONFIG_NAME "config.bin"

uint8_t currentPage = 0;
unsigned short int offset = 0;
unsigned char lul [IMG_CACHE];
uint8_t up[BD_COUNT] = {1};
//uint16_t downTime[BD_COUNT] = {1};
uint8_t longPressed[BD_COUNT] = {0};
SdFat SD;
File configFile;
//
// Comment out this line to gain 1K of RAM and not use a backing buffer
//
//#define USE_BACKBUFFER

// some globals
static int iScreenOffset; // current write offset of screen data
#ifdef USE_BACKBUFFER
static unsigned char ucScreen[1024]; // local copy of the image buffer
#endif
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
#ifdef USE_BACKBUFFER
    memcpy(&ucScreen[iScreenOffset], &bCache[1], bEnd - 1);
    iScreenOffset += (bEnd - 1);
#endif
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
    unsigned char ucTemp[17];
    //////Serial.println(iLen);
    ucTemp[0] = 0x40; // data command
    memcpy(&ucTemp[1], ucBuf, iLen);
    /*for(int i =0; i<17; i++) {
      ucTemp[i+1] = ucBuf[i];
      ////Serial.println(ucTemp[i+1]);
      ////Serial.println(ucBuf[i]);
    }*/
    I2CWrite(oled_addr, ucTemp, iLen + 1);
    // Keep a copy in local buffer
#ifdef USE_BACKBUFFER
    memcpy(&ucScreen[iScreenOffset], ucBuf, iLen);
    iScreenOffset += iLen;
#endif
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
#ifdef USE_BACKBUFFER
    uc = ucOld = ucScreen[i];
#else
    uc = ucOld = 0;
#endif

    uc &= ~(0x1 << (y & 7));
    if (ucColor)
    {
        uc |= (0x1 << (y & 7));
    }
    if (uc != ucOld) // pixel changed
    {
        oledSetPosition(x, y >> 3);
        oledWriteDataBlock(&uc, 1);
#ifdef USE_BACKBUFFER
        ucScreen[i] = uc;
#endif
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
int oledLoadBMPPart(uint8_t *pBMP, int bytes = 1024, int offset=0)
{
    int q, y, j; // offset to bitmap data
    int iOffBits = 0;
    int iPitch = 16;
    uint8_t x, z, b, *s;
    uint8_t dst_mask;
    uint8_t ucTemp[16]; // process 16 bytes at a time
    // rotate the data and send it to the display
    for (y = 0; y < 8 / (1024/bytes); y++) // 8 lines of 8 pixels
    {
        oledSetPosition(0, y+(offset/128));
        for (j = 0; j < 8; j++) // do 8 sections of 16 columns
        {
            s = &pBMP[iOffBits+(j * 2) + (y * iPitch * 8)]; // source line
            memset(ucTemp, 0, 16);                     // start with all black
            for (x = 0; x < 16; x += 8)                       // do each block of 16x8 pixels
            {
                dst_mask = 1;
                for (q = 0; q < 8; q++) // gather 8 rows
                {
                    b =  *(s + (q * iPitch));
                    for (z = 0; z < 8; z++) // gather up the 8 bits of this column
                    {
                        if (b & 0x80)
                            ucTemp[x + z] |= dst_mask;
                        b <<= 1;
                    } // for z
                    dst_mask <<= 1;
                }    // for q
                s++; // next source uint8_t
            }        // for x
            oledWriteDataBlock(ucTemp, 16);
            //oledCachedWrite(ucTemp, 16);
        } // for j
    }     // for y
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
    }     // for y
#ifdef USE_BACKBUFFER
    memset(ucScreen, ucData, 1024);
#endif
}

int getBitValue(int number, int place)
{
  return (number & (1 << place)) >> place;
}

void setMuxAddress(int address)
{
  int S0 = getBitValue(address, 0);
  int S1 = getBitValue(address, 1);
  int S2 = getBitValue(address, 2);
  digitalWrite(S0_PIN, S0);
  digitalWrite(S1_PIN, S1);
  digitalWrite(S2_PIN, S2);
  delayMicroseconds(10);
}

void displayImage(unsigned int imageNumber) {
  unsigned int imageOffset = offset + imageNumber*1024;
  configFile.seek(imageOffset);
  uint8_t byteI = 0;
  while(configFile.available() && byteI < (1024 / IMG_CACHE)){
    configFile.read(lul, IMG_CACHE);
    oledLoadBMPPart(lul, IMG_CACHE, byteI*IMG_CACHE);
    byteI++;
    delay(DELAY);
  }
}

void executeButtonConfig(uint8_t buttonIndex) {
  if(configFile) {
    configFile.seek((BD_COUNT * currentPage + buttonIndex+1)*16);
    uint8_t command;
    command = configFile.read();
    // if 1 then open another page
    if(command == 1){
      loadPage(configFile.read());
    }
    // if 0 then send key commands
    if(command == 0) {
      uint8_t key;
      key = configFile.read();
      while(key != 0) {
        Keyboard.press(key);
        key=configFile.read();
        delay(5);
      }
      delay(50);
      Keyboard.releaseAll();
    }
    if(command == 2) {
      //do nothing
    }
  }
}
void checkButtonUp(uint8_t buttonIndex) {
  setMuxAddress(buttonIndex);
  delay(1);
  uint8_t state = digitalRead(6);
  if(state != up[buttonIndex]) {
    if(state == BUTTON_DOWN) {
      if(longPressed[buttonIndex] == 1) {
        longPressed[buttonIndex] = 0;
        //loadConfig(configIndex, buttonIndex);
      } else {
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

void loadPage(uint8_t pageIndex) {
  currentPage = pageIndex;
  configFile.seek((pageIndex*BD_COUNT+1)*16);
  if(configFile && configFile.available()) {
    for(uint8_t j = 0; j < BD_COUNT; j++) {
      uint8_t data[16];
      configFile.read(data, 16);
      if(data[0] != 2) {
        setMuxAddress(j);
        displayImage(pageIndex*BD_COUNT+j);
      } else {
        setMuxAddress(j);
        oledFill(0);
      }
    }    
  }
}
void initAllDisplays() {
  for(uint8_t buttonIndex = 0; buttonIndex < BD_COUNT; buttonIndex++){
    setMuxAddress(buttonIndex);
    delay(DELAY);
    oledInit(0x3c, 0, 0);
    oledFill(255);
    delay(DELAY);
   }
}

void initSdCard() {
  int i = 0;
  //, SD_SCK_MHZ(50)
  while(!SD.begin(CS, SD_SCK_MHZ(250)) && i<=100){
      i++;
  }
  if (i == 100)
  {
      while (1);
  }
  configFile = SD.open(CONFIG_NAME, FILE_READ);
  configFile.seek(2);
  configFile.read(&offset,2);
  offset=offset*16;
}

void setup()
{
  clock_prescale_set(clock_div_2);
  delay(1500);
  Keyboard.begin();
  pinMode(6, INPUT_PULLUP);
  pinMode(S0_PIN, OUTPUT);
  pinMode(S1_PIN, OUTPUT);
  pinMode(S2_PIN, OUTPUT);
  initAllDisplays();
  initSdCard();
  loadPage(0);
}

void loop()
{
 for(uint8_t buttonIndex = 0; buttonIndex < BD_COUNT; buttonIndex++){
  checkButtonUp(buttonIndex);
  //checkButtonLongPress(buttonIndex);
  //delay(1);
 }
}
