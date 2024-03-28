# Slot-Car-Lap-Counter
Just a timer system for slot car tracks. Based around the ESP32, has a web portal that displays the data, as well as on screens on the track.

The Web Portal is a work in progress. It works presently, but will change.

I have other plans to add to this. MicroSD card support to save track and driver data, choices for multiple types of LCD or OLED screen for the physical track,
choices to choose how many lanes, and a few other options.

I have been using Visual Studio Code to upload the files to the ESP32. I'm using PlatformIO, and have installed the ESP-IDF extensions for this.
PlatformIO has the ability to build/upload the files to the SPIFFS file system, as well as built/upload the sketch to the ESP32. You will need
the SPIFFS file system to house the index.html file. I have also included my platformio.ini file so you can add the necessary libraries in PlatformIO.

Here are two tutorials, one on how to use Vicual Studio Code to program an ESP32, and one on how to upload files to the SPIFFS partition on the ESP32:

https://randomnerdtutorials.com/vs-code-platformio-ide-esp32-esp8266-arduino/
https://randomnerdtutorials.com/esp32-vs-code-platformio-spiffs/

If you would like to see the wiring layout, and see how it operates with just simple buttons, check out the build on Wokwi I have made. Some of the code is there as well.
https://wokwi.com/projects/388409586717645825

The surrent hardware consists of:
1 X Node32S (ESP32 Development board)
2 x SSD1306 I2C 0.96" OLED displays (could be any size SSD1306 based screen, more screen options will be added later)
2 X SSCF210100/SSCF110100/KFC-V-213 Momentary Microswitches (photointerruptors would also work here)
2 X tactile button
1 X Piezioelectric buzzer
8 X WS2812B led's
1 X 5 Volt 3 Amp Battery Eliminator Circuit (any voltage regulator circuit that provides at least 5V@2A will work)

I used the Battery Eliminator Circuit for the voltage regulator because I had some extra ones from my Radio Control Vehicle parts bin. Any regulator that will take in
a wide variety of DC voltage and drop it down to a steady 5V will work. This is so you can power it from the same power source as the rest of the track.
The whole system would use about 1.42 Amps at 5V if both screens were filled 100% at 100% brightness and all 8 WS2812B's were lit up full white at full brightness.
In reality it will draw peak around 500-650 mA and settles around 400 mA from my testing.

One of the OLED screens has had it's address changed by moving the resistor on the back. The ones I currently have use addresses 0x78 and 0x7A, yours may differ,
so you will need to change that in the code. They are wired to the default SDA and SCL pins, SDA is pin 21 and SCL is pin 22. The activation switch for lane 1 is
is wired to pin 14, the activation switch for Lane 2 is wired to pin 27, and the reset button is wired to pin 26, and the starting sequence is attached to pin 25. 
Make sure to update your home WiFi SSID and Password in the code as well.

I have added a light system for a start sequence using 8 WS2812B led's. They are attached to pin 13. I have also added a piezioelectric buzzer
with 400hz tones with the red lights and a 1000hz tone when the lights turn green. The buzzer is on pin 16. There 
is a reset button to restart the whole system, it is on pin 26. That logic will change later on. There is a randomized time between 1000ms and 2000ms between the
final phase of the red lights and the green lights for the starting sequence and this also happens with the starting tones, so you can't time in your head and get 
the best starts off the tree.

