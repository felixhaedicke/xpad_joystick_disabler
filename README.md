Disables all joystick devices using the hid-generic driver, when a device using
the Xpad driver becomes active, and re-enables these joysticks when all Xpad
gamepads are switched off.

This solves the following problem for me: Some games which are designed for
being used with gamepads show unexpected behaviour (e.g. in the menu) when
particular game controllers (like a throttle and a rudder pedal) are enabled.
So it makes sure that only gamepads, or my flight simulator equipment, is
active.

This helper program is an ugly hack, use with caution!

Designed for GNU/Linux only, and does most probably not work on other operating
systems!

Installation:
```
gcc xpad_joystick_disabler.c -o xpad_joystick_disabler -ludev
sudo install -o root -g root xpad_joystick_disabler /usr/local/bin
sudo install -o root -g root 99-xpad_joystick_disabler.rules /etc/udev/rules.d
```
