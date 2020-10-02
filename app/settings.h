#define BD_COUNT 6

// ChipSelect pin for SD card spi
#define SD_CS_PIN 10

// address pins for the multiplexers
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

// the delay to wait for everything to "boot"
// increase to 1500-1800 or higher if some displays dont
// startup right away
#define BOOT_DELAY 0
#define CONFIG_NAME "config.bin"
#define MAX_CACHE 32

// Change this value from 0x11 up to 0xff to reduce coil whine. different
// from display to display
#define PRE_CHARGE_PERIOD 0x11

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
