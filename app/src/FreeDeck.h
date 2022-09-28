#include <Arduino.h>
#include <SdFat.h>

#define TYPE_DISPLAY 0
#define TYPE_BUTTON 1

extern uint16_t currentPage;
extern uint16_t pageCount;
extern uint16_t timeout_sec;
extern uint32_t last_data_received;
extern File configFile;
extern SdFat SD;
extern unsigned long last_action;
extern unsigned long last_human_action;
extern uint8_t contrast;
extern uint8_t oled_delay;
extern uint8_t pre_charge_period;
extern uint8_t refresh_frequency;
extern bool has_json;
int getBitValue(int number, int place);
void setMuxAddress(uint8_t address, uint8_t type);
void setGlobalContrast(unsigned short c);
void setSetting();
void press_keys();
void sendText();
void pressSpecialKey();
void displayImage(uint16_t imageNumber);
void load_images(uint16_t pageIndex);
void load_buttons(uint16_t pageIndex);
uint8_t getCommand(uint8_t button, uint8_t secondary);
void onButtonPress(uint8_t buttonIndex, uint8_t secondary, bool leave);
void onButtonRelease(uint8_t buttonIndex, uint8_t secondary, bool leave);
void loadPage(uint16_t pageIndex);
void checkButtonState(uint8_t buttonIndex);
void initAllDisplays(uint8_t oled_delay, uint8_t pre_charge_period, uint8_t refresh_frequency);
void loadConfigFile();
void initSdCard();
void postSetup();
void sleepTask();
void switchScreensOff();
void switchScreensOn();
bool wake_display_if_needed();