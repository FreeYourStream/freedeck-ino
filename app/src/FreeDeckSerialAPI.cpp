#include "./FreeDeckSerialAPI.h"
#include "../settings.h"
#include "../version.h"
#include <HID-Project.h>
#include <limits.h>

#include "./FreeDeck.h"
#include "./OledTurboLight.h"

void _dumpConfigFileOverSerial() {
  configFile.seekSet(0);
  if (configFile.available()) {
    Serial.println(configFile.fileSize());
    byte buff[SERIAL_TX_BUFFER_SIZE] = {0};
    int read;
    do {
      read = configFile.read(buff, SERIAL_TX_BUFFER_SIZE);
      Serial.write(buff, read);
    } while (read >= SERIAL_TX_BUFFER_SIZE);
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
  size_t len = Serial.readBytesUntil('\n', numberChars, 10);
  numberChars[len] = '\n';
  return atol(numberChars);
}

void _saveNewConfigFileFromSerial() {
  _openTempFile();
  long fileSize = _getSerialFileSize();

  long receivedBytes = 0;
  unsigned int chunkLength;
  do {
    byte input[SERIAL_RX_BUFFER_SIZE];
    chunkLength = Serial.readBytes(input, SERIAL_RX_BUFFER_SIZE);
    if (chunkLength == 0)
      break;
    receivedBytes += chunkLength;
    if (!(receivedBytes % 4096) || receivedBytes == fileSize)
      Serial.println(receivedBytes);
    configFile.write(input, chunkLength);
  } while (chunkLength == SERIAL_RX_BUFFER_SIZE && receivedBytes < fileSize);
  if (receivedBytes == fileSize) {
    _renameTempFileToConfigFile(CONFIG_NAME);
  }
  configFile.close();
}

unsigned long int readSerialAscii() {
  char numberChars[10];
  size_t len = Serial.readBytesUntil('\n', numberChars, 9);
  if (len == 0)
    return ULONG_MAX;
  // remove any trailing extra stuff that atol does not like
  char clean[len + 1];
  memcpy(clean, &numberChars[0], len + 1 * sizeof(char));
  clean[len] = '\0';
  return atol(clean);
}

unsigned long int readSerialBinary() {
  byte numbers[4];
  size_t len = Serial.readBytesUntil('\n', numbers, 4);
  if (len == 0) {
    return ULONG_LONG_MAX;
  }
  unsigned long int number = 0;
  for (byte i = 0; i < len; i++) {
    if (numbers[i] == 13)
      break;
    number |= numbers[i] << (i * 8);
  }
  return number;
}

void handleAPI() {
  unsigned long command = readSerialBinary();
  if (command == 0x10) { // get firmware version
    Serial.println(F(FW_VERSION));
  }
  if (command == 0x20) { // read config
    _dumpConfigFileOverSerial();
  }
  if (command == 0x21) { // write config
    _saveNewConfigFileFromSerial();
    initAllDisplays();
    delay(200);
    postSetup();
    delay(200);
  }
  if (command == 0x30) { // get current page
    if (last_action + PAGE_CHANGE_SERIAL_TIMEOUT < millis())
      Serial.println(currentPage);
    else
      Serial.println(currentPage * -1 - 1);
#ifdef WAKE_ON_GET_PAGE_SERIAL
    wake_display_if_needed();
#endif
  }
  if (command == 0x31) { // set current page
    unsigned long targetPage = readSerialAscii();
    if (targetPage == ULONG_MAX)
      return;
    if (targetPage <= pageCount) {
      Keyboard.releaseAll();
      Consumer.releaseAll();
      loadPage(targetPage);
      Serial.println(OK);
    } else {
      Serial.println(ERROR);
    }
#ifdef WAKE_ON_SET_PAGE_SERIAL
    wake_display_if_needed();
#endif
  }
  if (command == 0x32) { // get page count
    Serial.println(pageCount);
  }
}

void handleSerial() {
  if (Serial.available() > 0) {
    unsigned long read = readSerialBinary();
    if (read == 0x3) {
      handleAPI();
    }
    while (Serial.available()) {
      Serial.read();
    }
  }
}