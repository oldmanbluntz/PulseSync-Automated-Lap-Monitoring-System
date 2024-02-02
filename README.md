# Slot-Car-Lap-Counter
Just a timer system for slot car tracks. Based around the ESP32, has a web portal that displays the data, as well as on screens on the track.

The Web Portal is a work in progress. It does work, but I am working on the webportal starting to update when the button is first pressed on the system,
like it displays on the screen.

I have other plans to add to this. MicroSD card support to save track and driver data, choices for multiple types of LCD or OLED screen for the physical track,
choices to choose how many lanes, and a few other options.

If you would like to see the wiring layout, and see how it operates with just simple buttons, check out the build on Wokwi I have made
https://wokwi.com/projects/388409586717645825

The surrent hardware consists of:
1 X Node32S (ESP32 Development board),
2 x SSD1306 I2C 0.96" OLED displays,
2 X SSCF210100/SSCF110100/KFC-V-213 Momentary Microswitches.

One of the OLED screens has had it's address changed by moving the resistor on the back. The ones I currently have use addresses 0x78 and 0x7A, yours may differ,
so update the addresses in the code on lines 64 and 65. They are wired to the default SDA and SCL pins, SDA is pin 21 and SCL is pin 22. The activation switch for lane 1 is
is wired to pin 14, the activation switch for Lane 2 is wired to pin 27, and the reset button is wired to pin 26. Make sure to update your home WiFi SSID and Password in the
code as well.
