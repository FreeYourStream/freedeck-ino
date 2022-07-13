#include <Arduino.h>
#include <SdFat.h>

#define TYPE_DISPLAY 0
#define TYPE_BUTTON 1

extern int currentPage;
extern int pageCount;
extern uint16_t timeout_sec;
extern File configFile;
extern SdFat SD;
extern unsigned long last_action;
int getBitValue(int number, int place);
void setMuxAddress(int address, uint8_t type = TYPE_DISPLAY);
void setGlobalContrast(unsigned short c);
void setSetting();
void pressKeys();
void sendText();
void pressSpecialKey();
void displayImage(int16_t imageNumber);
void load_images(int16_t pageIndex);
void load_buttons(int16_t pageIndex);
uint8_t getCommand(uint8_t button, uint8_t secondary);
void onButtonPress(uint8_t buttonIndex, uint8_t secondary, bool leave);
void onButtonRelease(uint8_t buttonIndex, uint8_t secondary, bool leave);
void loadPage(int16_t pageIndex);
void checkButtonState(uint8_t buttonIndex);
void initAllDisplays();
void loadConfigFile();
void initSdCard();
void postSetup();
void sleepTask();
void switchScreensOff();
void switchScreensOn();
bool wake_display_if_needed();