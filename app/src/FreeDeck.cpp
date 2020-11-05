#include <SPI.h>
#include <SdFat.h>
#include <avr/power.h>
#include <HID-Project.h>

#include "../settings.h"
#include "./FreeDeck.h"
#include "./OledTurboLight.h"

#define BUTTON_DOWN 0


SdFat SD;
File configFile;

int currentPage = 0;
unsigned short int fileImageDataOffset = 0;
short int contrast = 0;
unsigned char imageCache[IMG_CACHE_SIZE];
uint8_t buttonIsUp[BD_COUNT] = {1};
uint32_t downTime[BD_COUNT] = {0};
uint8_t longPressed[BD_COUNT] = {0};
uint8_t pageChanged = 0;

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


void setGlobalContrast(unsigned short c) {
	if (c == 0) c = 1;
	contrast = c;
	for (uint8_t buttonIndex = 0; buttonIndex < BD_COUNT; buttonIndex++) {
		setMuxAddress(buttonIndex);
		delay(1);
		oledSetContrast(c);
	}
}

void setSetting () {
	uint8_t settingCommand;
	configFile.read(&settingCommand, 1);
	if(settingCommand == 1) { // decrease brightness
		contrast = max(contrast - 20, 1);
		setGlobalContrast(contrast);
	} else if(settingCommand == 2) { // increase brightness
		contrast = min(contrast + 20, 255);
		setGlobalContrast(contrast);
	} else if(settingCommand == 3) { // set brightness
		contrast = min(contrast + 20, 255);
		setGlobalContrast(configFile.read());
	}
}

void pressKeys () {
	byte i = 0;
	uint8_t key;
	configFile.read(&key, 1);
	while (key != 0 && i++ < 7) {
		Keyboard.press(KeyboardKeycode(key));
		configFile.read(&key, 1);
		delay(1);
	}
}

void sendText () {
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

void changePage () {
	int16_t pageIndex;
	configFile.read(&pageIndex, 2);
	loadPage(pageIndex);
}

void pressSpecialKey () {
	uint16_t key;
	configFile.read(&key, 2);
	Consumer.press(key);
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
						changePage();
					} else if (command == 16) {
						pressKeys();
						delay(25);
						Keyboard.releaseAll();
					} else if (command == 19) {
						pressSpecialKey();
						delay(25);
						Consumer.releaseAll();
					} else if (command == 20) {
						sendText();
					} else if (command == 21) {
						setSetting();
					}
				}
			}
		} else if (buttonUp == 0) {
			if (command == 1) {
				pageChanged = 1;
				changePage();
			} else if (command == 0) {
				pressKeys();
			} else if (command == 3) {
				pressSpecialKey();
			} else if (command == 4) {
				sendText();
			} else if (command == 5) {
				setSetting();
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


void loadConfigFile() {
	configFile = SD.open(CONFIG_NAME, FILE_READ);
	configFile.seek(2);
	configFile.read(&fileImageDataOffset, 2);
	fileImageDataOffset = fileImageDataOffset * 16;
}

void initSdCard() {
	int i = 0;
	//, SD_SCK_MHZ(50)
	while (!SD.begin(SD_CS_PIN, SD_SCK_MHZ(16)) && i <= 100) {
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

void _renameTempFileToConfigFile(char const *path) {
	if (SD.exists(path)) {
		SD.remove(path);
	}
	configFile.rename(SD.vwd(), path);
}

void _openTempFile() {
	if (SD.exists(TEMP_FILE)) {
		SD.remove(TEMP_FILE);
	}
	configFile = SD.open(TEMP_FILE, O_WRONLY | O_CREAT);
	configFile.seekSet(0);
}

long _getSerialFileSize() {
	char numberChars[10];
	FILL_BUFFER();
	byte fileSizeLoopCounter = 0;
	while (fileSizeLoopCounter < 9) {
		numberChars[fileSizeLoopCounter++] = Serial.read();
	};
	numberChars[9] = '\n';
	return atol(numberChars);
}

void saveNewConfigFileFromSerial() {
	_openTempFile();
	long fileSize = _getSerialFileSize();

	long receivedBytes = 0;
	unsigned int chunkLength;
	FILL_BUFFER();
	do {
		FILL_BUFFER();
		byte input[512];
		chunkLength = Serial.readBytes(input, 512);
		receivedBytes += chunkLength;
		Serial.println(receivedBytes);
		if (chunkLength != 0) configFile.write(input, chunkLength);

	} while (chunkLength == 512);
	if(receivedBytes == fileSize) {
		_renameTempFileToConfigFile(CONFIG_NAME);
	}
	configFile.close();

}

void postSetup() {
	loadConfigFile();
	configFile.seekSet(4);
	setGlobalContrast(configFile.read());
	loadPage(0);
}
