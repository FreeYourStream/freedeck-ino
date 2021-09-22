#include <Arduino.h>
#include <SdFat.h>

#define TYPE_DISPLAY 0
#define TYPE_BUTTON 1

extern int currentPage;
extern int pageCount;
extern File configFile;
extern SdFat SD;

int getBitValue(int number, int place);
void setMuxAddress(int address, uint8_t type = TYPE_DISPLAY);
void setGlobalContrast(unsigned short c);
void setSetting();
void pressKeys();
void sendText();
void changePage();
void pressSpecialKey();
void displayImage(int16_t imageNumber);
uint8_t getCommand(uint8_t button, uint8_t secondary);
void onButtonPress(uint8_t buttonIndex, uint8_t secondary);
void onButtonRelease(uint8_t buttonIndex, uint8_t secondary);
void loadPage(int16_t pageIndex);
void checkButtonState(uint8_t buttonIndex);
void initAllDisplays();
void loadConfigFile();
void initSdCard();
void postSetup();
void checkTimeOut();
void switchScreensOff();
void switchScreensOn();