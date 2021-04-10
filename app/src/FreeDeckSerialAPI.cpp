#include "./FreeDeckSerialAPI.h"

#include <limits.h>

#include "./FreeDeck.h"
#include "./OledTurboLight.h"

unsigned long int readSerialAscii() {
	char numberChars[10];
	FILL_BUFFER();
	size_t len = Serial.readBytesUntil('\n', numberChars, 9);
	if (len == 0) return ULONG_MAX;
	// remove any trailing extra stuff that atol does not like
	char clean[len + 1];
	memcpy(clean, &numberChars[0], len + 1 * sizeof(char));
	clean[len] = '\0';
	return atol(clean);
}

unsigned long int readSerialBinary() {
	byte numbers[4];
	FILL_BUFFER();
	size_t len = Serial.readBytesUntil('\n', numbers, 4);
	if (len == 0) return ULONG_MAX;
	unsigned long int number = 0;
	for (byte i = 0; i < len; i++) {
		if (numbers[i] == 13) break;
		number |= numbers[i] << (i * 8);
	}
	return number;
}

void handleAPI() {
	unsigned long command = readSerialBinary();
	if (command == 0x10) {	// get firmware version
		Serial.println(F("2.0.0"));
	}
	if (command == 0x20) {	// read config
		dumpConfigFileOverSerial();
	}
	if (command == 0x21) {	// write config
		saveNewConfigFileFromSerial();
		initAllDisplays();
		delay(200);
		postSetup();
		delay(200);
	}
	if (command == 0x30) {	// get current page
		Serial.println(currentPage);
	}
	if (command == 0x31) {	// set current page
		unsigned long targetPage = readSerialAscii();
		if (targetPage == ULONG_MAX) return;
		if (targetPage <= pageCount) {
			loadPage(targetPage);
			Serial.println(OK);
		} else {
			Serial.println(ERROR);
		}
	}
	if (command == 0x32) {	// get page count
		Serial.println(pageCount);
	}
}