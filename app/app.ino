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

// button display count
// increase if your freedeck has more displays
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

#include "./lib.h"

void setup() {
	Serial.begin(4000000);
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
	delay(100);
	initSdCard();
	postSetup();
}

void loop() {
	int read = Serial.read();
	switch (read) {
		case 1:
		case 49:
			dumpConfigFileOverSerial();
			break;
		case 2:
		case 50:
			renameConfigFile();
			saveNewConfigFileFromSerial();
			initAllDisplays();
			delay(200);
			postSetup();
			delay(200);
			break;
	}
	for (uint8_t buttonIndex = 0; buttonIndex < BD_COUNT; buttonIndex++) {
		checkButtonState(buttonIndex);
	}
}
