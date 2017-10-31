/*
  PowerBook G4 Teensy Controller
  by David Ferguson, 25 April 2017

  Used to control components of a PowerBook G4 / Raspberry Pi laptop, including:
  - keyboard matrix to USB
  - battery current sensors
  - main relay off control
  - rear LCD enable control
  - main LCD controller buttons
  - ambient light control

*/

#define USB_LED_NUM_LOCK 0
#define USB_LED_CAPS_LOCK 1
#define USB_LED_SCROLL_LOCK 2
#define USB_LED_COMPOSE 3
#define USB_LED_KANA 4

// 5v battery charge level
int battery5vLed1 = 0;
int battery5vLed2 = 0;
int battery5vLed3 = 0;
int battery5vLed4 = 0;

// bool to store whether the keyboard has been reset after no keys pushed yet
bool keyboardCleared = true;

// variable to store timings for the lcd power off delay
unsigned long lcdDelay = 0;
bool noSignalLcd = false;

// variable to store timings for the keyboard brightness keys - to see if the lcd menu is still on the screen
unsigned long brightnessDelay = 0;
bool brightnessMenuOnDisplay = false;

// buttons on the lcd controller
const int LCD_BTN_PWR = 0;
const int LCD_BTN_MENU = 1;
const int LCD_BTN_PLUS = 2;
const int LCD_BTN_MINUS = 3;

// keyboard pins
const int keyboardOutputPins[15] = {5, 6, 7, 8, 9, 10, 11, 12, 13, 16, 17, 18, 19, 20, 21};
const int keyboardInputPins[9]   = {22, 23, 24, 25, 26, 27, 28, 29, 30};
const int physicalKeyboardOutputPins[15] = {4, 5, 7, 8, 9, 10, 11, 12, 13, 17, 18, 19, 20, 21, 22};
const int physicalKeyboardInputPins[9]   = {23, 28, 29, 30, 31, 32, 33, 34, 35};
const int capsLockPin = 6;

// off relay pin
const int relayOffPin = 15;

// current sensors pins
const int currentSensor5vPin = A1;
const int currentSensor12vPin = A0;

// rear lcd enable pin
const int rearLcdEnablePin = 14;

// lcd controller pins
const int lcdPwrPin = 24;
const int lcdMenuPin = 27;
const int lcdPlusPin = 25;
const int lcdMinusPin = 26;
const int lcdRedLedPin = A2;
const int lcdGreenLedPin = A3;

// ambient light pin
const int ambientLightPin = 16;

void setup() {
  Serial.begin(9600);
  Serial.println("SETUP");

  setupKeyboard();
  setupRelay();
  setupRearLcd();
  setupLcdController();

  // ambi-light
  pinMode(ambientLightPin, OUTPUT);
  digitalWrite(ambientLightPin, HIGH);
}

// SETUP FUNCTIONS

void setupKeyboard() {
  for (int i = 0; i < 9; i++) {
    pinMode(physicalKeyboardInputPins[i], INPUT_PULLUP);
  }

  for (int i = 0; i < 15; i++) {
    pinMode(physicalKeyboardOutputPins[i], OUTPUT);
    keyboardDigitalWrite(physicalKeyboardOutputPins[i], LOW);
  }

  // caps lock
  pinMode(capsLockPin, OUTPUT);
  // get the current caps lock led state - for when the teensy unexpectedly restarts
  /*if (keyboard_leds & (1<<USB_LED_CAPS_LOCK)) {
    digitalWrite(capsLockPin, HIGH);
  } else {*/
    digitalWrite(capsLockPin, LOW);
  //}
}

void setupRelay() {
  pinMode(relayOffPin, OUTPUT);
  digitalWrite(relayOffPin, LOW);
}

void setupRearLcd() {
  pinMode(rearLcdEnablePin, OUTPUT);
  digitalWrite(rearLcdEnablePin, HIGH);
}

void setupLcdController() {
  pinMode(lcdPwrPin, OUTPUT);
  pinMode(lcdMenuPin, OUTPUT);
  pinMode(lcdPlusPin, OUTPUT);
  pinMode(lcdMinusPin, OUTPUT);
  //pinMode(lcdRedLedPin, INPUT);
  //pinMode(lcdGreenLedPin, INPUT);

  digitalWrite(lcdPwrPin, HIGH);
  digitalWrite(lcdMenuPin, HIGH);
  digitalWrite(lcdPlusPin, HIGH);
  digitalWrite(lcdMinusPin, HIGH);
}

// END SETUP FUNCTIONS

void loop() {
  // check for serial commands
  checkSerial();

  // check for keyboard inputs
  checkKeyboard();

  // check if the lcd has signal (the pi is on)
  checkLcdSignal();
}

void checkSerial() {
  if (Serial.available() == 0) {
    return;
  }

  String serialMsg = Serial.readString();

  if (serialMsg == "set relay-off") {
    switchOffRelay();
    return;
  }

  if (serialMsg == "get battery-status") {
    batteryStatus();
    return;
  }

  if (serialMsg == "set rearlcd-on") {
    setRearLcdState(true);
    return;
  }

  if (serialMsg == "set rearlcd-off") {
    setRearLcdState(false);
    return;
  }

  if (serialMsg == "set lcdbtn-pwr") {
    lcdControllerBtn(LCD_BTN_PWR);
    return;
  }

  if (serialMsg == "set lcdbtn-menu") {
    lcdControllerBtn(LCD_BTN_MENU);
    return;
  }

  if (serialMsg == "set lcdbtn-plus") {
    lcdControllerBtn(LCD_BTN_PLUS);
    return;
  }

  if (serialMsg == "set lcdbtn-minus") {
    lcdControllerBtn(LCD_BTN_MINUS);
    return;
  }

  if (serialMsg == "get lcd-status") {
    lcdStatus();
    return;
  }
}

void switchOffRelay() {
  digitalWrite(relayOffPin, HIGH);
  delay(100);
  digitalWrite(relayOffPin, LOW);
  delay(1000); // just wait for the relay coil to activate and the power to be cut - just so we don't run any more instructions by accident
}

void batteryStatus() {
  int mVperAmp = 185; // use 100 for 20A Module and 66 for 30A Module
  int ACSoffset = 2500; 

  int batteryCurrentVoltage12v = analogRead(currentSensor12vPin);
  int batteryCurrentVoltage5v = analogRead(currentSensor5vPin);

  double voltage12v = (batteryCurrentVoltage12v / 1024.0) * 5000;
  double amps12v = ((voltage12v - 2500) / 185);

  double voltage15v = (batteryCurrentVoltage5v / 1024.0) * 5000;
  double amps5v = ((voltage15v - 2500) / 185);

  // spit out the status in JSON format
  String batteryStatusString = "{state:\"ok\", response:{ current12v:";
  batteryStatusString.concat(amps12v);
  batteryStatusString.concat(", current5v:");
  batteryStatusString.concat(amps5v);
  batteryStatusString.concat("}}");
  Serial.println(batteryStatusString);
}

void setRearLcdState(bool state) {
  digitalWrite(rearLcdEnablePin, state);
}

void lcdControllerBtn(int lcdButton) {
  if (lcdButton == LCD_BTN_PWR) {
    digitalWrite(lcdPwrPin, LOW);
    delay(100);
    digitalWrite(lcdPwrPin, HIGH);
    return;
  }

  if (lcdButton == LCD_BTN_MENU) {
    digitalWrite(lcdMenuPin, LOW);
    delay(100);
    digitalWrite(lcdMenuPin, HIGH);
    return;
  }

  if (lcdButton == LCD_BTN_PLUS) {
    digitalWrite(lcdPlusPin, LOW);
    delay(100);
    digitalWrite(lcdPlusPin, HIGH);
    return;
  }

  if (lcdButton == LCD_BTN_MINUS) {
    digitalWrite(lcdMinusPin, LOW);
    delay(100);
    digitalWrite(lcdMinusPin, HIGH);
    return;
  }
}

void lcdStatus() {
  int redLedVoltage = analogRead(lcdRedLedPin);
  int greenLedVoltage = analogRead(lcdGreenLedPin);

  // spit out the status in JSON format
  String ledStatusString = "{state:\"ok\", response:{ redLedVoltage:";
  ledStatusString.concat(redLedVoltage);
  ledStatusString.concat(", greenLedVoltage:");
  ledStatusString.concat(greenLedVoltage);
  ledStatusString.concat("}}");
  Serial.println(ledStatusString);
}

void checkLcdSignal() {
  int redLedVoltage = analogRead(lcdRedLedPin);

  if (noSignalLcd == false) {
    // over 400 is on, due to voltage differences
    if (redLedVoltage > 400) {
      Serial.println("no lcd signal, setting delay time");
      // store the current time and state in variables
      noSignalLcd = true;
      lcdDelay = millis();
    }
  } else {
    // lcd has had no signal already
    if (redLedVoltage > 400) {
      if ((lcdDelay + 4000) < millis()) {
        Serial.println("4 seconds has passed, cutting power");
        // four seconds has passed, cut the power
        switchOffRelay();
      }
    } else {
      Serial.println("got signal again, resetting variables");
      // lcd has signal again, reset the variables
      noSignalLcd = false;
    }
  }
}


// FUNCTIONS AND PROCEDURES FOR KEYBOARD
// -------------------------------------
// -------------------------------------
// -------------------------------------
// -------------------------------------
void keyboardDigitalWrite(int pin, boolean state) {
  if (state == HIGH) {
    pinMode(pin, INPUT);
  } else {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }
}

bool arrayContains(long *arrayToSearch, long searchItem) {
  int arraySize = sizeof(arrayToSearch) / sizeof(long);

  for (int i = 0; i < arraySize; i++) {
    if (arrayToSearch[i] == searchItem) {
      return true;
    }
  }

  return false;
}

int arraySearch(long *arrayToSearch, long searchItem) {
  int arraySize = sizeof(arrayToSearch) / sizeof(long);

  for (int i = 0; i < arraySize; i++) {
    if (arrayToSearch[i] == searchItem) {
      return i;
    }
  }

  return -1;
}

void getMofifierKeys(long *modifierKeys, int outputPin, int inputPin) {
  // modifierKeys passed by reference

  // get the starting index
  int counter = 0;
  for (int i = 0; i < 4; i++) {
    if (modifierKeys[i] == 0) {
      counter = i;
      break;
    }
  }

  if (outputPin == 6 && inputPin == 28) {
    modifierKeys[counter] = MODIFIERKEY_CTRL;
    counter = counter + 1;
  }

  if (outputPin == 7 && inputPin == 28) {
    modifierKeys[counter] = MODIFIERKEY_GUI;
    counter = counter + 1;
  }

  if (outputPin == 8 && inputPin == 28) {
    modifierKeys[counter] = MODIFIERKEY_ALT;
    counter = counter + 1;
  }

  if (outputPin == 9 && inputPin == 28) {
    modifierKeys[counter] = MODIFIERKEY_SHIFT;
    counter = counter + 1;
  }
}

void getKeys(long *keys, int outputPin, int inputPin, long *modifierKeys) {
  // keys and modifierKeys passed by reference

  // get the starting index
  int counter = 0;
  for (int i = 0; i < 6; i++) {
    if (keys[i] == 0) {
      counter = i;
      break;
    }
  }

  bool ctrlModifier = false;
  bool shiftModifier = false;
  bool altModifier = false;
  bool guiModifier = false;
  bool fnModifier = false;

  if (arrayContains(modifierKeys, MODIFIERKEY_CTRL)) {
    ctrlModifier = true;
  }

  if (arrayContains(modifierKeys, MODIFIERKEY_SHIFT)) {
    shiftModifier = true;
  }

  if (arrayContains(modifierKeys, MODIFIERKEY_ALT)) {
    altModifier = true;
  }

  if (arrayContains(modifierKeys, MODIFIERKEY_GUI)) {
    guiModifier = true;
  }


  if (outputPin == 5 && inputPin == 28) {
    // fn
    fnModifier = true;
  }

  if (outputPin == 10) {
    if (inputPin == 22) {
      // f1 / brightness down
      if (fnModifier) {
        // use the LCD controller buttons to change the brightness
        // only proceed if the green lcd light is on
        if (analogRead(lcdGreenLedPin) > 100) {
          // is the menu on screen already (from a previous brightness request
          if (brightnessMenuOnDisplay) {
            // menu bight be open already - check the timer
            if ((brightnessDelay + 5000) < millis()) {
              // display is still open - we just need to push the minus button
              // and now minus one from the brightness
              lcdControllerBtn(LCD_BTN_MINUS);

              // update the status variables
              brightnessMenuOnDisplay = true;
              brightnessDelay = millis();
              return;
            }
          }
          // menu is not being displayed (or was but now closed), so open it
          lcdControllerBtn(LCD_BTN_MENU);
          delay(5);
          lcdControllerBtn(LCD_BTN_MINUS);
          delay(5);
          lcdControllerBtn(LCD_BTN_MENU);
          delay(5);
          lcdControllerBtn(LCD_BTN_MINUS);
          delay(5);
          lcdControllerBtn(LCD_BTN_MINUS);
          delay(5);
          lcdControllerBtn(LCD_BTN_MENU);
          delay(5);

          // and now minus one from the brightness
          lcdControllerBtn(LCD_BTN_MINUS);
          brightnessMenuOnDisplay = true;
          brightnessDelay = millis();
        }
      } else {
        keys[counter] = KEY_F1;
        counter = counter + 1;
        if (counter >= 6) {
          return;
        }
      }
    }
    if (inputPin == 23) {
      // 1 / ! */
      keys[counter] = KEY_1;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 24) {
      // q / Q
      keys[counter] = KEY_Q;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 25) {
      // a / A
      keys[counter] = KEY_A;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 26) {
      // z /Z
      keys[counter] = KEY_Z;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 27) {
      // f11 /
      keys[counter] = KEY_F11;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 29) {
      // f12 /
      keys[counter] = KEY_F12;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 30) {
      // backspace
      keys[counter] = KEY_BACKSPACE;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
  }

  if (outputPin == 11) {
    if (inputPin == 22) {
      // f2 / brightness up
      if (fnModifier) {
        // use the LCD controller buttons to change the brightness
        // only proceed if the green lcd light is on
        if (analogRead(lcdGreenLedPin) > 100) {
          // is the menu on screen already (from a previous brightness request
          if (brightnessMenuOnDisplay) {
            // menu bight be open already - check the timer
            if ((brightnessDelay + 5000) < millis()) {
              // display is still open - we just need to push the minus button
              // and now minus one from the brightness
              lcdControllerBtn(LCD_BTN_PLUS);

              // update the status variables
              brightnessMenuOnDisplay = true;
              brightnessDelay = millis();
              return;
            }
          }
          // menu is not being displayed (or was but now closed), so open it
          lcdControllerBtn(LCD_BTN_MENU);
          delay(5);
          lcdControllerBtn(LCD_BTN_MINUS);
          delay(5);
          lcdControllerBtn(LCD_BTN_MENU);
          delay(5);
          lcdControllerBtn(LCD_BTN_MINUS);
          delay(5);
          lcdControllerBtn(LCD_BTN_MINUS);
          delay(5);
          lcdControllerBtn(LCD_BTN_MENU);
          delay(5);

          // and now minus one from the brightness
          lcdControllerBtn(LCD_BTN_PLUS);
          brightnessMenuOnDisplay = true;
          brightnessDelay = millis();
        }
      } else {
        keys[counter] = KEY_F2;
        counter = counter + 1;
        if (counter >= 6) {
          return;
        }
      }
    }
    if (inputPin == 23) {
      // 2 / @/euro
      if (shiftModifier) {
        keys[counter] = ASCII_40;
        counter = counter + 1;
        if (counter >= 6) {
          return;
        }
      } else {
        keys[counter] = KEY_2;
        counter = counter + 1;
        if (counter >= 6) {
          return;
        }
      }
    }
    if (inputPin == 24) {
      // w / W
      keys[counter] = KEY_W;
      Serial.print("w");
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 25) {
      // s /S
      keys[counter] = KEY_S;
      Serial.print("s");
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 26) {
      // x / X
      keys[counter] = KEY_X;
      Serial.print("x");
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 27) {
      // esc
      keys[counter] = KEY_ESC;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 29) {
      // backtick / tilda
      keys[counter] = KEY_TILDE;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 30) {
      // tab
      keys[counter] = KEY_TAB;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
  }

  if (outputPin == 12) {
    if (inputPin == 22) {
      // f3 / mute
      if (fnModifier) {

      } else {
        keys[counter] = KEY_F3;
        counter = counter + 1;
        if (counter >= 6) {
          return;
        }
      }
    }
    if (inputPin == 23) {
      // 3 / £
      keys[counter] = KEY_3;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 24) {
      // e / E
      keys[counter] = KEY_E;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 25) {
      // d / D
      keys[counter] = KEY_D;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 26) {
      // c / C
      keys[counter] = KEY_C;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 29) {
      // § / plusminus
      if (shiftModifier) {
        keys[counter] = ISO_8859_1_B1;
        counter = counter + 1;
        if (counter >= 6) {
          return;
        }
      } else {
        keys[counter] = ISO_8859_1_A7;
        counter = counter + 1;
        if (counter >= 6) {
          return;
        }
      }
    }
  }

  if (outputPin == 13) {
    if (inputPin == 22) {
      // f4 / vol min
      if (fnModifier) {
        // push the minus button on the lcd controller
        lcdControllerBtn(LCD_BTN_MINUS);
      } else {
        keys[counter] = KEY_F4;
        counter = counter + 1;
        if (counter >= 6) {
          return;
        }
      }
    }
    if (inputPin == 23) {
      // 4 / dollar
      keys[counter] = KEY_4;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 24) {
      // r / R
      keys[counter] = KEY_R;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 25) {
      // f / F
      keys[counter] = KEY_F;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 26) {
      // v / V
      keys[counter] = KEY_V;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 29) {
      // caps lock
      keys[counter] = KEY_CAPS_LOCK;
      counter = counter + 1;

      // flip the state of the caps lock led
      digitalWrite(capsLockPin, (!digitalRead(capsLockPin)));

      if (counter >= 6) {
        return;
      }
    }
  }

  if (outputPin == 16) {
    if (inputPin == 22) {
      // f5 / vol up
      if (fnModifier) {
        // push the plus button on the lcd controller board
        lcdControllerBtn(LCD_BTN_MINUS);
      } else {
        keys[counter] = KEY_F5;
        counter = counter + 1;
        if (counter >= 6) {
          return;
        }
      }
    }
    if (inputPin == 23) {
      // 5 / percentage
      keys[counter] = KEY_5;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 24) {
      // t / T
      keys[counter] = KEY_T;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 25) {
      // g / G
      keys[counter] = KEY_G;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 26) {
      // b / B
      keys[counter] = KEY_B;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 27) {
      // - / underscore
      keys[counter] = KEY_MINUS;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 29) {
      // [ / {
      keys[counter] = KEY_LEFT_BRACE;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 30) {
      // ' / doublequote
      if (shiftModifier) {
        keys[counter] = ASCII_22;
        counter = counter + 1;
        if (counter >= 6) {
          return;
        }
      } else {
        keys[counter] = KEY_QUOTE;
        counter = counter + 1;
        if (counter >= 6) {
          return;
        }
      }
    }
  }

  if (outputPin == 17) {
    if (inputPin == 22) {
      // f6 / numlock
      if (fnModifier) {

      } else {
        keys[counter] = KEY_F6;
        counter = counter + 1;
        if (counter >= 6) {
          return;
        }
      }
    }
    if (inputPin == 23) {
      // 6 / ^
      keys[counter] = KEY_6;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 24) {
      // y / Y
      keys[counter] = KEY_Y;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 25) {
      // h / H
      keys[counter] = KEY_H;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 26) {
      // n / N
      keys[counter] = KEY_N;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 30) {
      // space
      keys[counter] = KEY_SPACE;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
  }

  if (outputPin == 18) {
    if (inputPin == 22) {
      // f7 / dashboard
      if (fnModifier) {

      } else {
        keys[counter] = KEY_F7;
        counter = counter + 1;
        if (counter >= 6) {
          return;
        }
      }
    }
    if (inputPin == 23) {
      // 7 / &
      keys[counter] = KEY_7;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 24) {
      // u / U
      keys[counter] = KEY_U;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 25) {
      // j / J
      keys[counter] = KEY_J;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 26) {
      // m / M
      keys[counter] = KEY_M;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 29) {
      // eject
      keys[counter] = KEY_MEDIA_EJECT;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
  }

  if (outputPin == 19) {
    if (inputPin == 22) {
      // f8
      keys[counter] = KEY_F8;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 23) {
      // 8 / *
      keys[counter] = KEY_8;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 24) {
      // i / I
      keys[counter] = KEY_I;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 25) {
      // k / K
      keys[counter] = KEY_K;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 26) {
      // , / <
      keys[counter] = KEY_COMMA;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 27) {
      // enter
      keys[counter] = KEY_ENTER;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 29) {
      // right / end
      if (guiModifier) {
        keys[counter] = KEY_END;
        counter = counter + 1;
        if (counter >= 6) {
          return;
        }
      } else {
        keys[counter] = KEY_RIGHT;
        counter = counter + 1;
        if (counter >= 6) {
          return;
        }
      }
    }
    if (inputPin == 30) {
      // left / home
      if (guiModifier) {
        keys[counter] = KEY_HOME;
        counter = counter + 1;
        if (counter >= 6) {
          return;
        }
      } else {
        keys[counter] = KEY_LEFT;
        counter = counter + 1;
        if (counter >= 6) {
          return;
        }
      }
    }
  }

  if (outputPin == 20) {
    if (inputPin == 22) {
      // f9
      keys[counter] = KEY_F9;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 23) {
      // 9 / (
      keys[counter] = KEY_9;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 24) {
      // o / O
      keys[counter] = KEY_O;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 25) {
      // l / L
      keys[counter] = KEY_L;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 26) {
      // . / ?
      if (shiftModifier) {
        keys[counter] = ASCII_3F;
        counter = counter + 1;
        if (counter >= 6) {
          return;
        }
      } else {
        keys[counter] = KEY_PERIOD;
        counter = counter + 1;
        if (counter >= 6) {
          return;
        }
      }
    }
    if (inputPin == 27) {
      // \ / |
      if (shiftModifier) {
        keys[counter] = ASCII_7C;
        counter = counter + 1;
        if (counter >= 6) {
          return;
        }
      } else {
        keys[counter] = KEY_BACKSLASH;
        counter = counter + 1;
        if (counter >= 6) {
          return;
        }
      }
    }
    if (inputPin == 29) {
      // up / pageup
      if (guiModifier) {
        keys[counter] = KEY_PAGE_UP;
        counter = counter + 1;
        if (counter >= 6) {
          return;
        }
      } else {
        keys[counter] = KEY_UP;
        counter = counter + 1;
        if (counter >= 6) {
          return;
        }
      }
    }
    if (inputPin == 30) {
      // down / pagedown
      if (guiModifier) {
        keys[counter] = KEY_PAGE_DOWN;
        counter = counter + 1;
        if (counter >= 6) {
          return;
        }
      } else {
        keys[counter] = KEY_DOWN;
        counter = counter + 1;
        if (counter >= 6) {
          return;
        }
      }
    }
  }

  if (outputPin == 21) {
    if (inputPin == 22) {
      // f10
      keys[counter] = KEY_F10;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 23) {
      // 0 / )
      keys[counter] = KEY_0;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 24) {
      // p / P
      keys[counter] = KEY_P;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 25) {
      // ; / :
      keys[counter] = KEY_SEMICOLON;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 26) {
      // / / ?
      keys[counter] = KEY_SLASH;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 27) {
      // cr
      keys[counter] = KEY_ENTER;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 29) {
      // = / +
      keys[counter] = KEY_EQUAL;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
    if (inputPin == 30) {
      // ] / }
      keys[counter] = KEY_RIGHT_BRACE;
      counter = counter + 1;
      if (counter >= 6) {
        return;
      }
    }
  }
}

void allOutputPinsHighExcept(int pin) {
  for (int i = 0; i < 15; i++) {
    if (physicalKeyboardOutputPins[i] != pin) {
      keyboardDigitalWrite(physicalKeyboardOutputPins[i], HIGH);
    } else {
      keyboardDigitalWrite(physicalKeyboardOutputPins[i], LOW);
    }
  }
}

void checkKeyboard() {
  bool keyPressed = false;

  long oldKeys[6] = {0, 0, 0, 0, 0, 0};
  long positionedKeys[6] = {0, 0, 0, 0, 0, 0};

  // are there any keys pressed - saves looping through multiple times
  for (int i = 0; i < 9; i++) {
    if (!digitalRead(physicalKeyboardInputPins[i])) {
      keyPressed = true;
    }
  }

  if (!keyPressed && !keyboardCleared) {
    // clear the keys
    Keyboard.set_key1(0);
    Keyboard.set_key2(0);
    Keyboard.set_key3(0);
    Keyboard.set_key4(0);
    Keyboard.set_key5(0);
    Keyboard.set_key6(0);
    Keyboard.set_modifier(0);
    Keyboard.send_now();
    keyboardCleared = true;
  }

  if (!keyPressed) {
    return;
  }

  // from here on, a key has been pressed
  keyboardCleared = false;

  // setup keys and modifier arrays
  long modifierKeys[4];
  for (int i = 0; i < 4; i++) {
    modifierKeys[i] = 0;
  }
  long keys[6];
  for (int i = 0; i < 6; i++) {
    keys[i] = 0;
  }

  // get the keys pressed into an array (in any order)
  // loop through the output pins
  for (int i = 0; i < 15; i++) {
    int outputPinToCheck = physicalKeyboardOutputPins[i];
    allOutputPinsHighExcept(outputPinToCheck);

    // loop through the input pins
    for (int j = 0; j < 9; j++) {
      int inputPinToCheck = physicalKeyboardInputPins[j];

      if (!digitalRead(inputPinToCheck)) {
        // this pair of pins is active, get the keys and modifier keys
        getMofifierKeys(modifierKeys, keyboardOutputPins[i], keyboardInputPins[j]);
        getKeys(keys, keyboardOutputPins[i], keyboardInputPins[j], modifierKeys);
      }
    }
  }

  // we've now got keys in a random order in the keys array
  // we need to compare these against the currently pressed keys
  for (int i = 0; i < 6; i++) {
    // if the new keys contains an existing key
    if (arrayContains(keys, oldKeys[i]) && oldKeys[i] != 0) {
      // keep this key in the same place, so it's held down
      positionedKeys[i] = oldKeys[i];
      // get rid of the older key so we don't repeat it
      oldKeys[i] = 0;
    }
  }

  // fill in the blanks with the keys that didn't appear last time
  for (int i = 0; i < 6; i++) {
    if (keys[i] != 0) {
      for (int j = 0; j < 6; j++) {
        if (positionedKeys[j] == 0) {
          positionedKeys[j] = keys[i];
          break;
        }
      }
    }
  }

  // update the oldKeys to the new positioned keys
  for (int i = 0; i < 6; i++) {
    oldKeys[i] = positionedKeys[i];
  }

  // set the keys now we've reordered them
  Keyboard.set_key1(positionedKeys[0]);
  Keyboard.set_key2(positionedKeys[1]);
  Keyboard.set_key3(positionedKeys[2]);
  Keyboard.set_key4(positionedKeys[3]);
  Keyboard.set_key5(positionedKeys[4]);
  Keyboard.set_key6(positionedKeys[5]);

  // modifier keys are simple, just set them using set_modifier
  Keyboard.set_modifier(modifierKeys[0] | modifierKeys[1] | modifierKeys[2] | modifierKeys[3]);

  // send the keys + modifiers to the computer
  Keyboard.send_now();

  // enable all the output pins again
  for (int i = 0; i < 15; i++) {
    keyboardDigitalWrite(physicalKeyboardOutputPins[i], LOW);
  }
}
// -----------------------------------------
// -----------------------------------------
// -----------------------------------------
// -----------------------------------------
// END FUNCTIONS AND PROCEDURES FOR KEYBOARD
