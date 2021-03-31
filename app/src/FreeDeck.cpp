// Include all the needed libraries
#include <SPI.h>
#include <SdFat.h>
#include <avr/power.h>
#include <HID-Project.h>

#include "../settings.h"
#include "./FreeDeck.h"
#include "./OledTurboLight.h"

// Define when the button is down
#define BUTTON_DOWN 0

// Define the objects for the config file
SdFat SD;
File configFile;

// Store the current page
int currentPage = 0;
// Define the offset for the image
unsigned short int fileImageDataOffset = 0;
// Define the contrast
short int contrast = 0;
// Define the imageCache
unsigned char imageCache[IMG_CACHE_SIZE];
// An array to store the state of the buttons
uint8_t buttonIsUp[BD_COUNT] = {1};
// An array to store how long the button is down
uint32_t downTime[BD_COUNT] = {0};
// An array to store if the button was long pressed
uint8_t longPressed[BD_COUNT] = {0};
// Variable to check if the page has changed
uint8_t pageChanged = 0;

/**
 * @brief Get the 0 or zero from a integer
 * 
 * @param number The variable you want to check
 * @param place The place of the bit you want to read
 * @return int The value of the bit
 */
int getBitValue(int number, int place) {
	// Some fancy bit shifting
	return (number & (1 << place)) >> place;
}

/**
 * @brief Set the mux to a certain address
 * 
 * @param address What address to set the Mux to
 */
void setMuxAddress(int address) {
	// Check if the bit is set in the address
	int S0 = getBitValue(address, 0);
	// Write it
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


/**
 * @brief Set the contrast
 * 
 * @param c The value of the contrast
 */
void setGlobalContrast(unsigned short c) {
	// It can never be zero. So if its zero, it will be one
	if (c == 0) c = 1;
	// Save the contrast
	contrast = c;
	// For each screen
	for (uint8_t buttonIndex = 0; buttonIndex < BD_COUNT; buttonIndex++) {
		// Set the address
		setMuxAddress(buttonIndex);
		// Wait a bit
		delay(1);
		// Set the contrast
		oledSetContrast(c);
	}
}

/**
 * @brief Set the Setting object
 * 
 */
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

/**
 * @brief Press the keyboard key
 * 
 */
void pressKeys () {
	byte i = 0;
	uint8_t key;
	// Read the key
	configFile.read(&key, 1);
	// For all the keys
	while (key != 0 && i++ < 7) {
		// Press the key
		Keyboard.press(KeyboardKeycode(key));
		configFile.read(&key, 1);
		delay(1);
	}
}

/**
 * @brief Type the text on the computer
 * 
 */
void sendText () {
	byte i = 0;
	uint8_t key;
	configFile.read(&key, 1);
	// For all letters
	while (key != 0 && i++ < 15) {
		// Press the keys
		Keyboard.press(KeyboardKeycode(key));
		// Wait a bit so its typed in the correct order
		delay(8);
		if (key < 224) {
			Keyboard.releaseAll();
		}
		configFile.read(&key, 1);
	}
	// Release all keys
	Keyboard.releaseAll();
}

/**
 * @brief Change the page
 * 
 */
void changePage () {
	int16_t pageIndex;
	// Read the config for the page index
	configFile.read(&pageIndex, 2);
	// Change the key
	loadPage(pageIndex);
}

/**
 * @brief Send a special key
 * 
 */
void pressSpecialKey () {
	uint16_t key;
	configFile.read(&key, 2);
	Consumer.press(key);
}

/**
 * @brief Display an image
 * 
 * @param imageNumber The number of the image
 */
void displayImage(int16_t imageNumber) {
	// Seek the image
	configFile.seekSet(fileImageDataOffset + imageNumber * 1024L);
	uint8_t byteI = 0;
	// While there is data
	while (configFile.available() && byteI < (1024 / IMG_CACHE_SIZE)) {
		// Read the data
		configFile.read(imageCache, IMG_CACHE_SIZE);
		// Write it to the screen
		oledLoadBMPPart(imageCache, IMG_CACHE_SIZE, byteI * IMG_CACHE_SIZE);
		byteI++;
	}
}

/**
 * @brief Load a page
 * 
 * @param pageIndex The page index
 */
void loadPage(int16_t pageIndex) {
	currentPage = pageIndex;
	// For the screens
	for (uint8_t j = 0; j < BD_COUNT; j++) {
		// Set the address
		setMuxAddress(j);
		delay(1);
		// Write the image to the display
		displayImage(pageIndex * BD_COUNT + j);
	}
}

/**
 * @brief Executes a button config change
 * 
 * @param buttonIndex The index of the button
 * @param buttonUp  Check if the button is unpressed
 * @param secondary ??
 */
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

/**
 * @brief Check the button state
 * 
 * @param buttonIndex What button we should check
 */
void checkButtonState(uint8_t buttonIndex) {
	// Set the address
	setMuxAddress(buttonIndex);
	// Read the state of the button
	uint8_t state = digitalRead(6);
	// Get the current run time
	uint32_t ms = millis();
	// Get the duration the button has been pressed
	uint32_t duration = ms - downTime[buttonIndex];
	if (duration == ms) duration = 0;
	// A long if statement, that is checking if the button is long pressed
	if (state != buttonIsUp[buttonIndex] ||
		(duration >= LONG_PRESS_DURATION && longPressed[buttonIndex] == 0 &&
		 buttonIsUp[buttonIndex] == BUTTON_DOWN)) {
		if (duration >= LONG_PRESS_DURATION) {
			longPressed[buttonIndex] = 1;
		}
		// Execute the button press
		executeButtonConfig(buttonIndex, state, 0);
	}
	// Set the buttonstate
	buttonIsUp[buttonIndex] = state;
}

/**
 * @brief Init the displays
 * 
 */
void initAllDisplays() {
	// For all the screens
	for (uint8_t buttonIndex = 0; buttonIndex < BD_COUNT; buttonIndex++) {
		// Set the variables for this screen
		buttonIsUp[buttonIndex] = 1;
		downTime[buttonIndex] = 0;
		longPressed[buttonIndex] = 0;
		// Set the mux
		setMuxAddress(buttonIndex);
		// Wait a sec
		delay(1);
		// init the oled
		oledInit(0x3c, 0, 0);
		// Clear the screen
		oledFill(255);
	}
}

/**
 * @brief Load the config file
 * 
 */
void loadConfigFile() {
	// Open the config file
	configFile = SD.open(CONFIG_NAME, FILE_READ);
	// Seek for a thing
	configFile.seek(2);
	// Read the image offset
	configFile.read(&fileImageDataOffset, 2);
	// Write the image offset
	fileImageDataOffset = fileImageDataOffset * 16;
}

/**
 * @brief Init the SD card
 * 
 */
void initSdCard() {
	int i = 0;
	//, SD_SCK_MHZ(50)
	// While its starting, count how long it takes
	while (!SD.begin(SD_CS_PIN, SD_SCK_MHZ(16)) && i <= 100) {
		i++;
	}
	// If it took too long
	if (i == 100) {
		// Hang up.
		// ToDo: Do something to notify the user
		while (1)
			;
	}
}

/**
 * @brief Dump the config over the serial connection
 * 
 */
void dumpConfigFileOverSerial() {
	// Set the config seek to 0
	configFile.seekSet(0);
	// While there is a config file
	if (configFile.available()) {
		// Print the size of the fileSize
		Serial.println(configFile.fileSize());
		// A little buffer
		byte buff[512];
		int available;
		do {
			// Read the buffer
			available = configFile.read(buff, 512);
			// Write the buffer.
			Serial.write(buff, 512);
			// Do as long as there is data
		} while (available >= 512);
	}
}

/**
 * @brief A function to rename the config file
 * 
 * @param path The new filename
 */
void _renameTempFileToConfigFile(char const *path) {
	// If the file exist
	if (SD.exists(path)) {
		// Remove it
		SD.remove(path);
	}
	// Rename the file
	configFile.rename(SD.vwd(), path);
}

/**
 * @brief Open the temp file
 * 
 */
void _openTempFile() {
	// Check if the tempfile is there
	if (SD.exists(TEMP_FILE)) {
		// Remove it
		SD.remove(TEMP_FILE);
	}
	// Open the file
	configFile = SD.open(TEMP_FILE, O_WRONLY | O_CREAT);
	// Set the configFile seek to -
	configFile.seekSet(0);
}

/**
 * @brief Get the size of the incoming data
 * 
 * @return long length of the incoming data
 */
long _getSerialFileSize() {
	// Make a buffer for the data
	char numberChars[10];
	// Fill the buffer (Where is this function?)
	FILL_BUFFER();
	byte fileSizeLoopCounter = 0;
	while (fileSizeLoopCounter < 9) {
		// Read the data
		numberChars[fileSizeLoopCounter++] = Serial.read();
	};
	// Add an enter
	numberChars[9] = '\n';
	// Return the string as a number
	return atol(numberChars);
}

/**
 * @brief Save the incoming data as a config file
 * 
 */
void saveNewConfigFileFromSerial() {
	// Open a temp file
	_openTempFile();
	// Get the filesize
	long fileSize = _getSerialFileSize();

	// A variable to count how many bytes we have got
	long receivedBytes = 0;
	// The chunklenght variabe
	unsigned int chunkLength;
	// Where is this? But filling the buffer
	FILL_BUFFER();
	do {
		// Fill the buffer again
		FILL_BUFFER();
		// An input array
		byte input[512];
		// Read 512 bytes from serial
		chunkLength = Serial.readBytes(input, 512);
		// Add chunklength to the the receivedBytes
		receivedBytes += chunkLength;
		// Print the recieved bytes number back
		Serial.println(receivedBytes);
		// If the chunklength isnt 0, write it to the inputfile
		if (chunkLength != 0) configFile.write(input, chunkLength);
		//While there is data
	} while (chunkLength == 512);
	// If we have got as much bytes as promised
	if(receivedBytes == fileSize) {
		// Rename the file
		_renameTempFileToConfigFile(CONFIG_NAME);
	}
	// Close writing to the config file
	configFile.close();

}

/**
 * @brief The post setup script
 * 
 */
void postSetup() {
	// Load the config file
	loadConfigFile();
	// Set the config file seek
	configFile.seekSet(4);
	// Read the global constrast and set it
	setGlobalContrast(configFile.read());
	// Go to page 0
	loadPage(0);
}
