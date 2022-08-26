#include <Arduino.h>
#define BD_COUNT 6
#define ROW_SIZE 128
// for ryan aukes 5x3 pcb layout or
// if your screens are not in 1..n order
// #define CUSTOM_ORDER
#ifdef CUSTOM_ORDER
#define ADDRESS_TO_SCREEN \
  { 13, 11, 8, 5, 2, 14, 10, 7, 4, 1, 12, 9, 6, 3, 0 }
#define ADDRESS_TO_BUTTON \
  { 12, 14, 6, 5, 2, 11, 13, 7, 4, 1, 10, 9, 8, 3, 0 }
#endif

#define TIMEOUT_TIME 0L  // Screens turns never off
// #define TIMEOUT_TIME 5UL * 60UL * 1000UL // Screens turn off after 5 minutes;
// #define WAKE_ON_GET_PAGE_SERIAL // will wake up the displays everytime you focus another window
#define WAKE_ON_SET_PAGE_SERIAL  // will only wake up the display if you focus a new window that has
                                 // a configurator page

// ChipSelect pin for SD card spi
#define SD_CS_PIN 10
#define SD_MHZ 5

// address pins for the multiplexers
// ryan aukes 5x3 5,6,7,8,9
#define BUTTON_PIN 6
#define S0_PIN 7
#define S1_PIN 8
#define S2_PIN 9
#define S3_PIN 10

// the size of the image chunks send to the displays
// try different values here. good displays can go higher.
// 512 for example.
// worse need to go lower. 64 for example
#define IMG_CACHE_SIZE 128

// the duration it takes after a long press is triggered
// maybe move this to the configurator?
#define LONG_PRESS_DURATION 300
#define PAGE_CHANGE_SERIAL_TIMEOUT 1500

// the delay to wait for everything to "boot"
// increase to 1500-1800 or higher if some displays dont
// startup right away
#define BOOT_DELAY 0
#define CONFIG_NAME "config.bin"
#define TEMP_FILE "config.bin.tmp"
#define MAX_CACHE 32

// Change this value from 0x11 up to 0xff to reduce coil whine. different
// from display to display
#define PRE_CHARGE_PERIOD 0x11

// Minimum Brightness value for displays. If your displays image quality gets
// worse at lower brighness choose a bigger value here
#define MINIMUM_BRIGHTNESS 0x20
// #define MINIMUM_BRIGHTNESS 0x00 //almost dark, good displays only
// #define MINIMUM_BRIGHTNESS 0x30 //brightest for cheap displays

// if your screen is flickering, choose a lower number. the worse the screen,
// the lower the number.
#define REFRESH_FREQUENCY 0xf2
// #define REFRESH_FREQUENCY 0xf1
// #define REFRESH_FREQUENCY 0xc1
// #define REFRESH_FREQUENCY 0x80

// Pin or port numbers for SDA and SCL
// NOT THE ARDUINO PORT NUMBERS
#define BB_SDA 2  // ARDUINO:RX_PIN:D0 32U4:20:PD2
#define BB_SCL 3  // ARDUINO:TX_PIN:D1 32U4:21:PD3

#if F_CPU > 8000000L
// the time to slow down for the displays
// if your displays don't display the images 100% correct after
// decreasing the IMG_CACHE_SIZE increase this value to 3 or 4 for example
#define I2C_DELAY 2
#else
// this on if you have an 8Mhz version
#define I2C_DELAY 0
#endif
