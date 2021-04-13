#include <Arduino.h>
extern int currentPage;
extern int pageCount;
int getCurrentPage();
int getBitValue(int number, int place);
void setMuxAddress(int address);
void displayImage(int16_t imageNumber);
void loadPage(int16_t pageIndex);
void executeButtonConfig(uint8_t buttonIndex, uint8_t buttonUp,
						 uint8_t secondary);
void checkButtonState(uint8_t buttonIndex);
void initAllDisplays();
void setGlobalContrast();
void loadConfigFile();
void initSdCard();
void dumpConfigFileOverSerial();
void _renameTempFileToConfigFile();
void saveNewConfigFileFromSerial();
void postSetup();
void checkTimeOut();
void switchScreensOff();
void switchScreensOn();