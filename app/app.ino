// freedeck arduino code for flashing to atmega32u4 based arduinos
// and compatible Copyright (C) 2020 Kilian Gosewisch
//
// This program is free software: you can redistribute it and/or
// modify it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License,
// or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public
// License along with this program. If not, see
// <https://www.gnu.org/licenses/>.

#include <Arduino.h>
#include <HID-Project.h>

#include "./settings.h"
#include "./src/FreeDeck.h"
#include "./src/FreeDeckSerialAPI.h"
void setup() {
  Serial.begin(4000000);
  Serial.setTimeout(100);
  delay(BOOT_DELAY);
  Keyboard.begin();
  Consumer.begin();
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(S0_PIN, OUTPUT);
#if BD_COUNT > 2
  pinMode(S1_PIN, OUTPUT);
#endif
#if BD_COUNT > 4
  pinMode(S2_PIN, OUTPUT);
#endif
#if BD_COUNT > 8
  pinMode(S3_PIN, OUTPUT);
#endif
  initAllDisplays(I2C_DELAY, PRE_CHARGE_PERIOD, REFRESH_FREQUENCY);
  delay(100);
  initSdCard();
  postSetup();
}

void loop() {
  handleSerial();
  sleepTask();
  for (uint8_t buttonIndex = 0; buttonIndex < BD_COUNT; buttonIndex++) {
    checkButtonState(buttonIndex);
  }
}
