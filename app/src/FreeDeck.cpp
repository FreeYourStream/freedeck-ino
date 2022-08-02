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
uint8_t contrast = 0;
unsigned char imageCache[IMG_CACHE_SIZE];
uint8_t oled_delay = I2C_DELAY;
uint8_t pre_charge_period = PRE_CHARGE_PERIOD;
uint8_t refresh_frequency = REFRESH_FREQUENCY;
bool woke_display = 0;

#ifdef CUSTOM_ORDER
byte addressToScreen[] = ADDRESS_TO_SCREEN;
byte addressToButton[] = ADDRESS_TO_BUTTON;
#endif

unsigned long last_action;
unsigned long last_human_action;

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
    if (key > 0x80 && key < 0xe0) {
      key = key - 0x80;
      Keyboard.release(KeyboardKeycode(key));
      delay(15);
    } else {
      Keyboard.press(KeyboardKeycode(key));
    }
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

void emit_button_press(uint8_t button_index, bool secondary) {
  Serial.write(0x3);
  Serial.print("\r\n");
  Serial.write(0x10);
  Serial.print("\r\n");
  char f_size_str[10];
  sprintf(f_size_str, "%d\t%d\t%d", currentPage, button_index, secondary);
  Serial.println(f_size_str);
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

void onButtonPress(uint8_t button_index, uint8_t secondary, bool leave) {
  last_human_action = millis();
  woke_display = wake_display_if_needed();
  if (woke_display)
    return;
  uint8_t command = getCommand(button_index, secondary) & 0xf;
  if (command == 0) {
    pressKeys();
  } else if (command == 1) {
    nextPage = get_target_page(button_index, secondary);
    load_images(nextPage);
  } else if (command == 3) {
    pressSpecialKey();
  } else if (command == 4) {
    sendText();
  } else if (command == 5) {
    setSetting();
  } else if (command == 6) {
    emit_button_press(button_index, secondary);
  }
}

void onButtonRelease(uint8_t buttonIndex, uint8_t secondary, bool leave) {
  last_human_action = millis();
  if (woke_display) {
    woke_display = false;
    return;
  }
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

void initAllDisplays(uint8_t _oled_delay, uint8_t _pre_charge_period, uint8_t _refresh_frequency) {
  oled_delay = _oled_delay;
  for (uint8_t buttonIndex = 0; buttonIndex < BD_COUNT; buttonIndex++) {
    buttons[buttonIndex].index = buttonIndex;
    setMuxAddress(buttonIndex, TYPE_DISPLAY);
    delay(1);
    oledInit(0x3c, _pre_charge_period, _refresh_frequency);
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
  configFile.read(&contrast, 1);
  configFile.read(&timeout_sec, 2);

  configFile.read(NULL, 1);
  configFile.read(&oled_delay, 1);
  configFile.read(&pre_charge_period, 1);
  configFile.read(&refresh_frequency, 1);

  if (oled_delay == 0)
    oled_delay = I2C_DELAY;
  if (pre_charge_period == 0)
    pre_charge_period = PRE_CHARGE_PERIOD;
  if (refresh_frequency == 0)
    refresh_frequency = REFRESH_FREQUENCY;
}

void initSdCard() {
  while (!SD.begin(SD_CS_PIN, SD_SCK_MHZ(16))) {
    delay(1);
  }
}

void postSetup() {
  loadConfigFile();
  initAllDisplays(oled_delay, pre_charge_period, refresh_frequency);
  setGlobalContrast(contrast);
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