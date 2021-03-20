#include "./FreeDeckSerialAPI.h"

#include "./FreeDeck.h"
#include "./OledTurboLight.h"

long int readSerialData() {
	char numberChars[10];
	FILL_BUFFER();
	Serial.readBytesUntil('\n', numberChars, 9);
	return atol(numberChars);
}

void handleAPI() {
	long int command = readSerialData();
	if (command == 1) {	 // read config
		dumpConfigFileOverSerial();
	}
	if (command == 2) {	 // write config
		saveNewConfigFileFromSerial();
		initAllDisplays();
		delay(200);
		postSetup();
		delay(200);
	}
	if (command == 3) {	 // get firmware version
		Serial.print(F("2.0.0"));
	}
	if (command == 16) {  // set current page
		loadPage(readSerialData());
	}
	if (command == 17) {  // get current page
		Serial.print(currentPage);
	}
}