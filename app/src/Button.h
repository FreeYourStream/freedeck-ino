#include <Arduino.h>
#define BUTTON_DOWN 0
#define BUTTON_UP 1

#define PRIMARY 0
#define PRIMARY_SECONDARY 1
#define PRIMARY_LEAVE 2

typedef void (*CallbackType)(uint8_t index, uint8_t secondary, bool leave);

class Button {
public:
  uint8_t index = 0;
  boolean state = BUTTON_UP;
  uint32_t pressedSince = 0;
  boolean pressExecuted = false;
  uint8_t mode = 0;

  CallbackType onPressCallback;
  CallbackType onReleaseCallback;

  void update(boolean new_state);

private:
  void callShortPress();
  void callShortRelease();
  void callLongPress();
  void callLongRelease();
};
