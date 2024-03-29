#include "./Button.h"

#include "../settings.h"
#define BUTTON_DOWN 0
#define BUTTON_UP 1

void Button::update(bool new_state) {
  // if the button is being held down and we are waiting for
  // the secondary function to fire
  if (state == BUTTON_UP && new_state == BUTTON_DOWN) {  // getting pressed down
    state = new_state;
    if (has_secondary) {
      // to decide if we need to execute long or short press
      // start couting for how long we press the button
      pressedSince = millis();
    } else {
      callShortPress();
    }
  } else if (state == BUTTON_DOWN && state == new_state) {  // still being pressed down
    if (!has_secondary || pressExecuted)
      return;

    uint32_t now = millis();
    uint32_t passedTime = now - pressedSince;

    if (passedTime < LONG_PRESS_DURATION)
      return;
    callLongPress();
    delay(100);
    callLongRelease();

  } else if (state == BUTTON_DOWN && new_state == BUTTON_UP) {  // getting released
    state = new_state;
    if (has_secondary) {
      uint32_t now = millis();
      uint32_t passedTime = now - pressedSince;
      pressedSince = 0;
      if (passedTime < LONG_PRESS_DURATION) {
        callShortPress();
        delay(100);
        callShortRelease();
      } else {
        pressExecuted = false;
      }
    } else {
      callShortRelease();
    }
  }
}

void Button::callShortPress() {
  if (onPressCallback != NULL && pressExecuted == false)
    onPressCallback(index, false);
  pressExecuted = true;
}
void Button::callShortRelease() {
  if (onReleaseCallback != NULL && pressExecuted == true)
    onReleaseCallback(index, false);
  pressExecuted = false;
}
void Button::callLongPress() {
  if (onPressCallback != NULL && pressExecuted == false)
    onPressCallback(index, true);
  pressExecuted = true;
}
void Button::callLongRelease() {
  if (onReleaseCallback != NULL && pressExecuted == true)
    onReleaseCallback(index, true);
}
