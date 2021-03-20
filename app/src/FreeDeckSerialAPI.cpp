#include "./FreeDeckSerialAPI.h"

#include "./FreeDeck.h"
#include "./OledTurboLight.h"

unsigned long int readSerialAscii() {
	char numberChars[10];
	FILL_BUFFER();
	Serial.readBytesUntil('\n', numberChars, 9);
	return atol(numberChars);
}

unsigned long int readSerialBinary() {
	byte numbers[4];
	FILL_BUFFER();
	size_t length = Serial.readBytesUntil('\n', numbers, 4);
	unsigned long int number = 0;
	for (byte i = 0; i < length; i++) {
		if (numbers[i] == 13) break;
		number |= numbers[i] << (i * 8);
	}
	return number;
}

void handleAPI() {
	unsigned long command = readSerialBinary();
	if (command == 0x10) {	// get firmware version
		Serial.print(F("2.0.0"));
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
		Serial.print(currentPage);
	}
	if (command == 0x31) {	// set current page
		loadPage(readSerialAscii());
	}
}