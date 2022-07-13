#include "./FreeDeck.h"

#include "../settings.h"
#include "./Button.h"
#include "./OledTurboLight.h"
#include <HID-Project.h>
#include <SPI.h>
#include <SdFat.h>
#include <avr/power.h>

#define TYPE_DISPLAY 0
#define TYPE_BUTTON 1

SdFat SD;
File configFile;
Button buttons[BD_COUNT];

int currentPage = 0;
int nextPage = 0;
int pageCount;
uint16_t timeout_sec = TIMEOUT_TIME;
unsigned short int fileImageDataOffset = 0;
short int contrast = 0;
unsigned char imageCache[IMG_CACHE_SIZE];

#ifdef CUSTOM_ORDER
byte addressToScreen[] = ADDRESS_TO_SCREEN;
byte addressToButton[] = ADDRESS_TO_BUTTON;
#endif

unsigned long last_action;

int getBitValue(int number, int place) {
  return (number & (1 << place)) >> place;
}

void setMuxAddress(int address, uint8_t type = TYPE_DISPLAY) {
#ifdef CUSTOM_ORDER
  if (type == TYPE_DISPLAY)
    address = addressToScreen[address];
  else if (type == TYPE_BUTTON)
    address = addressToButton[address];
#endif
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

  delay(1); // wait for multiplexer to switch
}

void loadPage(int16_t pageIndex) {
  currentPage = pageIndex;
  load_images(pageIndex);
  load_buttons(pageIndex);
}

void setGlobalContrast(unsigned short c) {
  if (c == 0)
    c = 1;
  contrast = c;
  for (uint8_t buttonIndex = 0; buttonIndex < BD_COUNT; buttonIndex++) {
    setMuxAddress(buttonIndex, TYPE_DISPLAY);
    delay(1);
    oledSetContrast(c);
  }
}

bool wake_display_if_needed() {
  if (timeout_sec == 0) {
    last_action = millis();
    return false;
  }
  if (millis() - last_action > (timeout_sec * 1000L)) {
    switchScreensOn();
    return true;
  }
  last_action = millis();
  return false;
}

void setSetting() {
  uint8_t settingCommand;
  configFile.read(&settingCommand, 1);
  if (settingCommand == 1) { // decrease brightness
    contrast = max(contrast - 20, 1);
    setGlobalContrast(contrast);
  } else if (settingCommand == 2) { // increase brightness
    contrast = min(contrast + 20, 255);
    setGlobalContrast(contrast);
  } else if (settingCommand == 3) { // set brightness
    contrast = min(contrast + 20, 255);
    setGlobalContrast(configFile.read());
  }
}

void pressKeys() {
  byte i = 0;
  uint8_t key;
  configFile.read(&key, 1);
  while (key != 0 && i++ < 7) {
    Keyboard.press(KeyboardKeycode(key));
    configFile.read(&key, 1);
    delay(1);
  }
}

void sendText() {
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

uint16_t get_target_page(uint8_t buttonIndex, uint8_t secondary) {
  configFile.seekSet((BD_COUNT * currentPage + buttonIndex + 1) * 16 + 8 * secondary + 1);
  uint16_t pageIndex;
  configFile.read(&pageIndex, 2);
  return pageIndex;
}

void pressSpecialKey() {
  uint16_t key;
  configFile.read(&key, 2);
  Consumer.press((ConsumerKeycode)key);
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

uint8_t getCommand(uint8_t button, uint8_t secondary) {
  configFile.seek((BD_COUNT * currentPage + button + 1) * 16 + 8 * secondary);
  uint8_t command;
  command = configFile.read();
  return command;
}

void onButtonPress(uint8_t buttonIndex, uint8_t secondary, bool leave) {
  if (wake_display_if_needed())
    return;
  uint8_t command = getCommand(buttonIndex, secondary) & 0xf;
  if (command == 0) {
    pressKeys();
  } else if (command == 1) {
    nextPage = get_target_page(buttonIndex, secondary);
    load_images(nextPage);
  } else if (command == 3) {
    pressSpecialKey();
  } else if (command == 4) {
    sendText();
  } else if (command == 5) {
    setSetting();
  }
}

void onButtonRelease(uint8_t buttonIndex, uint8_t secondary, bool leave) {
  uint8_t command = getCommand(buttonIndex, secondary) & 0xf;
  if (command == 0) {
    Keyboard.releaseAll();
  } else if (command == 1) {
    currentPage = nextPage;
    load_buttons(currentPage);
  } else if (command == 3) {
    Consumer.releaseAll();
  }
  if (leave) {
    configFile.seek((BD_COUNT * currentPage + buttonIndex + 1) * 16 + 8);
    uint16_t pageIndex;
    configFile.read(&pageIndex, 2);
    loadPage(pageIndex);
  }
}

void load_images(int16_t pageIndex) {
  for (uint8_t buttonIndex = 0; buttonIndex < BD_COUNT; buttonIndex++) {
    setMuxAddress(buttonIndex, TYPE_DISPLAY);
    displayImage(pageIndex * BD_COUNT + buttonIndex);
  }
}

void load_buttons(int16_t pageIndex) {
  for (uint8_t buttonIndex = 0; buttonIndex < BD_COUNT; buttonIndex++) {
    uint8_t command = getCommand(buttonIndex, false);
    buttons[buttonIndex].mode = command >> 4;
    buttons[buttonIndex].onPressCallback = onButtonPress; // to do: only do this initially
    buttons[buttonIndex].onReleaseCallback = onButtonRelease;

    delay(1);
  }
}

void checkButtonState(uint8_t buttonIndex) {
  setMuxAddress(buttonIndex, TYPE_BUTTON);
  uint8_t state = digitalRead(BUTTON_PIN);
  buttons[buttonIndex].update(state);
  return;
}

void initAllDisplays() {
  for (uint8_t buttonIndex = 0; buttonIndex < BD_COUNT; buttonIndex++) {
    buttons[buttonIndex].index = buttonIndex;
    setMuxAddress(buttonIndex, TYPE_DISPLAY);
    delay(1);
    oledInit(0x3c, 0, 0);
    oledFill(255);
  }
}

void loadConfigFile() {
  configFile = SD.open(CONFIG_NAME, FILE_READ);
  configFile.seek(2);
  configFile.read(&fileImageDataOffset, 2);
  pageCount = (fileImageDataOffset - 1) / BD_COUNT;
  fileImageDataOffset = fileImageDataOffset * 16;

  // configFile.seekSet(4);
  setGlobalContrast(configFile.read());
  configFile.read(&timeout_sec, 2);
}

void initSdCard() {
  while (!SD.begin(SD_CS_PIN, SD_SCK_MHZ(16))) {
    delay(1);
  }
}

void postSetup() {
  loadConfigFile();

  loadPage(0);
}

void sleepTask() {
  if (timeout_sec == 0)
    return;
  if (millis() - last_action >= (timeout_sec * 1000L)) {
    switchScreensOff();
  }
}

void switchScreensOff() {
  for (uint8_t buttonIndex = 0; buttonIndex < BD_COUNT; buttonIndex++) {
    setMuxAddress(buttonIndex, TYPE_DISPLAY);
    delay(1);
    oledFill(0);
  }
}

void switchScreensOn() {
  last_action = millis();
  loadPage(currentPage);
}