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
#include <HID-Project.h>
#include "./settings.h"
#include "./src/FreeDeck.h"

/**
 * @brief Setup function, initializes everything.  
 * @returns none
 */
void setup() {
	// Setting up the Serial Communication
	Serial.begin(4000000);
	// Give the oleds some times to start up
	delay(BOOT_DELAY);
	// Init and start the keyboard
	Keyboard.begin();
	// Init and start the Media buttons
	Consumer.begin();

	// Set the pinmodes for the multiplexers
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

	// Init all the displays
	initAllDisplays();
	// Give it some time to process it all
	delay(100);
	// Init the SD card
	initSdCard();
	// Do some post setup things
	postSetup();
}

void loop() {
	// Read the incoming serial data
	int read = Serial.read();
	// Check what to do with this packet
	switch (read) {
		// If its a 1 (or 0x1)
		case 1:
		case 49:
			// Dump the content of the config file over the serial monitor
			dumpConfigFileOverSerial();
			break;
		// If its a 2 (or 0x2)
		case 2:
		case 50:
			// Recieve the new config file
			saveNewConfigFileFromSerial();
			// Reinit all the displays
			initAllDisplays();
			delay(200);
			// Do the post setup
			postSetup();
			delay(200);
			break;
	}
	
	// Check all the buttons if they have been pressed
	for (uint8_t buttonIndex = 0; buttonIndex < BD_COUNT; buttonIndex++) {
		checkButtonState(buttonIndex);
	}
}
