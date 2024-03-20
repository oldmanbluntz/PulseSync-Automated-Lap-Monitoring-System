# Slot-Car-Lap-Counter
Just a timer system for slot car tracks. Based around the ESP32, has a web portal that displays the data, as well as on screens on the track.

The Web Portal is a work in progress. It works presently, but will change.

I have other plans to add to this. MicroSD card support to save track and driver data, choices for multiple types of LCD or OLED screen for the physical track,
choices to choose how many lanes, and a few other options.

If you would like to see the wiring layout, and see how it operates with just simple buttons, check out the build on Wokwi I have made. The code is there as well.
https://wokwi.com/projects/388409586717645825

The surrent hardware consists of:
1 X Node32S (ESP32 Development board),
2 x SSD1306 I2C 0.96" OLED displays,
2 X SSCF210100/SSCF110100/KFC-V-213 Momentary Microswitches,
2 X tactile button
1 X Piezioelectric buzzer
8 X WS2812B led's

One of the OLED screens has had it's address changed by moving the resistor on the back. The ones I currently have use addresses 0x78 and 0x7A, yours may differ,
so update the addresses in the code on lines 89 and 90. They are wired to the default SDA and SCL pins, SDA is pin 21 and SCL is pin 22. The activation switch for lane 1 is
is wired to pin 14, the activation switch for Lane 2 is wired to pin 27, and the reset button is wired to pin 26. Make sure to update your home WiFi SSID and Password in the
code as well.

I have added a light system for a start sequence using 8 WS2812B led's. you can change the led settings on lines 32-35. I have also added a piezioelectric buzzer
with 400hz tones with the red lights and a 1000hz tone when the lights turn green. it is on pin 16, and that can be changed on line 29. There 
is a reset button to restart the lap counter and timers, it is on pin 26, and the pin can be changed on line 27. There is another button added
to start the light and tone sequence for the start of the race. It is on pin 25, and the pin can be changed on line 28.

Here is a shot of the webportal. It's currently set to update every 25 milliseconds. You can modify that on line 130.

![Screenshot 2024-02-12 110554](https://github.com/oldmanbluntz/Slot-Car-Lap-Counter/assets/2407099/90e60df0-9c61-4c25-8916-997d475fa514)
