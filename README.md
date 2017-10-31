# PiBook-Arduino

The PiBook laptop has a Teensy controller which is used as a SMC (System Management Controller) for the PiBook. It controls:
- Power on/off
- Battery charging
- Keyboard
- LCD controls
- Rear LCD
- Ambiant light

## Power on/off
The power is controlled with a latching relay.

When the power key is pressed the relay is engaged and latched, and power is provided to the Pi and the Arduino. The Arduino has control over the unlatch relay pin and detects when the LCD has no signal (and therefore the Pi is off) and unlatches the relay, cutting power to the Pi and itself.

## Keyboard
The keyboard is a matrix, and is wired directly to the Teensy. The Teensy scanns the keyboard and works out what keys are pressed, and sends them over USB to the Raspberry Pi as keybaord input.

## LCD controls
The Teensy is connected to the LCD control board, which allows it to control the LCD and also receive feedback on whether the LCD is on, and whether it has a signal.

## Rear LCD
The rear LCD power is controlled via the Teensy. This allows it to be switched on/off and also reset (power cycle) if necessary.

## Ambiant light
The ambiant light is also controlled by the Teensy. Currently it just powers up to show that the Teensy is on, but in the future it could "sleep pulse" like the original would have done.
