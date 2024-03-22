// Include necessary libraries
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <FastLED.h>

// Define Wi-Fi credentials
const char* ssid = "SSID HERE";
const char* password = "PASSWORD HERE";

// Create instances of SSD1306 displays and AsyncWebServer
Adafruit_SSD1306 display1(128, 64, &Wire, -1);
Adafruit_SSD1306 display2(128, 64, &Wire, -1);
AsyncWebServer server(80);

// Variables for lap timing and counting
unsigned long startTime1, startTime2;
unsigned long lapTime1, lapTime2;
unsigned long bestLap1, bestLap2, recentLap1, recentLap2;
unsigned long redLightStartTime = 0;
unsigned long greenLightStartTime = 0;
unsigned long delayBeforeGreen = 0;
int lapCount1, lapCount2;

// Pins for buttons and reset
const int buttonPin1 = 14;
const int buttonPin2 = 27;
const int resetPin = 26;
const int startButtonPin = 25;
const int buzzerPin = 16;

// Pins for start lights
#define LED_PIN     13  // Pin connected to the LED strip
#define NUM_LEDS    8   // Number of LEDs in the strip
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB

// Create instace of CRGB array
CRGB leds[NUM_LEDS];

// Debouncing variables
unsigned long lastDebounceTime1 = 0;
unsigned long lastDebounceTime2 = 0;
unsigned long debounceDelay = 50;

// Lap counting and initialization flags
bool lapCounting1 = false;
bool lapCounting2 = false;
bool lapCountInitialized1 = false;
bool lapCountInitialized2 = false;
bool ledTonePlayed[NUM_LEDS] = {false}; // Array to keep track of whether tone has been played for each LED


// Lap press counters (not currently used in the code)
int lapPressCount1 = 0;
int lapPressCount2 = 0;

// Define the delay duration (500 milliseconds = 0.5 seconds)
const unsigned long lapCountDelay = 500;
const unsigned long minDelayBeforeGreen = 1000; // 1 second
const unsigned long maxDelayBeforeGreen = 2000; // 2 seconds

// Function to play tone
void playTone(int frequency, int duration) {
  tone(buzzerPin, frequency);
  delay(duration);
  noTone(buzzerPin);
}

void updateLapInfo(int lane);
void resetLapInfo();
void displayLapInfo(int lane, int lapCount, unsigned long currentLap, String bestLap, String recentLap);

// Setup function
void setup() {
  Serial.begin(115200);
  randomSeed(esp_random()); // Use ESP hardware number generator for a random seed
  unsigned long seed = esp_random();
  randomSeed(seed);
  Serial.print("Random seed: ");
  Serial.println(seed); // Print the random seed to the serial monitor

  // Initialize LEDC for FastLED
  ledcSetup(0, 5000, 8);  // LEDC channel 0, 5 kHz PWM, 8-bit resolution
    ledcAttachPin(LED_PIN, 13);  // Attach LED_PIN to LEDC channel 0

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // Set up button and reset pin modes
  pinMode(buttonPin1, INPUT_PULLUP);
  pinMode(buttonPin2, INPUT_PULLUP);
  pinMode(resetPin, INPUT_PULLUP);
  pinMode(startButtonPin, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);

  // Initialize LED strip
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

  // Initialize displays and clear them
  Wire.begin();
  display1.begin(SSD1306_SWITCHCAPVCC, 0x3D);
  display2.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display1.clearDisplay();
  display2.clearDisplay();

  // Initialize lap counts and display initial lap information
  lapCount1 = 0;
  lapCount2 = 0;
  displayLapInfo(1, lapCount1, 0, "--", "--");
  displayLapInfo(2, lapCount2, 0, "--", "--");

  // Set up the web server route and response
server.on("/lap-info", HTTP_GET, [](AsyncWebServerRequest *request){
  StaticJsonDocument<400> doc;
  doc["lane1"]["lapCount"] = lapCount1;
  doc["lane1"]["recentLap"] = recentLap1 / 1000.0;
  doc["lane1"]["bestLap"] = bestLap1 / 1000.0;
  doc["lane1"]["currentLap"] = lapCounting1 ? (millis() - startTime1) / 1000.0 : static_cast<double>(0);
  doc["lane2"]["lapCount"] = lapCount2;
  doc["lane2"]["recentLap"] = recentLap2 / 1000.0;
  doc["lane2"]["bestLap"] = bestLap2 / 1000.0;
  doc["lane2"]["currentLap"] = lapCounting2 ? (millis() - startTime2) / 1000.0 : static_cast<double>(0);
  String json;
  serializeJson(doc, json);
  request->send(200, "application/json", json);
});

  // Set up the web server route and response
server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = "<html><head>";
    html += "<style>";
    html += ".lane-container { display: flex; justify-content: space-around; }";
    html += ".lane { flex: 1; text-align: center; }";
    html += "</style>";
    html += "</head><body>";
    html += "<div class='lane-container'>";
    html += "<div class='lane'>";
    html += "<h1>Lane 1</h1>";
    html += "<p>Laps: <span id='lapCount1'>--</span></p>";
    html += "<p>Current Lap: <span id='currentLap1'>--</span> s</p>";
    html += "<p>Recent Lap: <span id='recentLap1'>--</span> s</p>";
    html += "<p>Best Lap: <span id='bestLap1'>--</span> s</p>";
    html += "</div>";
    html += "<div class='lane'>";
    html += "<h1>Lane 2</h1>";
    html += "<p>Laps: <span id='lapCount2'>--</span></p>";
    html += "<p>Current Lap: <span id='currentLap2'>--</span> s</p>";
    html += "<p>Recent Lap: <span id='recentLap2'>--</span> s</p>";
    html += "<p>Best Lap: <span id='bestLap2'>--</span> s</p>";
    html += "</div>";
    html += "</div>";
    html += "<script>";
    html += "setInterval(function() {";
    html += "fetch('/lap-info')";
    html += ".then(response => response.json())";
    html += ".then(data => {";
    html += "document.getElementById('lapCount1').textContent = data.lane1.lapCount;";
    html += "document.getElementById('currentLap1').textContent = data.lane1.currentLap;";
    html += "document.getElementById('recentLap1').textContent = data.lane1.recentLap;";
    html += "document.getElementById('bestLap1').textContent = data.lane1.bestLap;";
    html += "document.getElementById('lapCount2').textContent = data.lane2.lapCount;";
    html += "document.getElementById('currentLap2').textContent = data.lane2.currentLap;";
    html += "document.getElementById('recentLap2').textContent = data.lane2.recentLap;";
    html += "document.getElementById('bestLap2').textContent = data.lane2.bestLap;";
    html += "});";
    html += "}, 25);"; // Decreased interval for more frequent updates
    html += "</script>";
    html += "</body></html>";
    request->send(200, "text/html", html);
});

  // Start the web server
  server.begin();
}

// Define the enum for light sequence state
enum LightSequenceState {
  IDLE,
  RED_LIGHTS,
  GREEN_LIGHTS,
  TURN_OFF_LIGHTS,
  WAIT_FOR_GREEN_DELAY
};

// Declare variables for delay before green and start time of delay
LightSequenceState lightState = IDLE;
unsigned long waitStartTime = 0; // Initialize the variable at the global scope

void loop() {
  // Button 1 handling
  if ((millis() - lastDebounceTime1) > debounceDelay) {
    if (digitalRead(buttonPin1) == LOW) {
      Serial.println("Button 1 pressed");
      if (!lapCounting1) {
        lapCounting1 = true;
        startTime1 = millis();
      } else {
        // Check if enough time has passed since the last button press
        if ((millis() - lastDebounceTime1) > lapCountDelay) {
          updateLapInfo(1);
          lastDebounceTime1 = millis();
        }
      }
      lastDebounceTime1 = millis();
    }
  }

  // Button 2 handling
  if ((millis() - lastDebounceTime2) > debounceDelay) {
    if (digitalRead(buttonPin2) == LOW) {
      Serial.println("Button 2 pressed");
      if (!lapCounting2) {
        lapCounting2 = true;
        startTime2 = millis();
      } else {
        // Check if enough time has passed since the last button press
        if ((millis() - lastDebounceTime2) > lapCountDelay) {
          updateLapInfo(2);
          lastDebounceTime2 = millis();
        }
      }
      lastDebounceTime2 = millis();
    }
  }
  switch (lightState) {
  case IDLE:
    if (digitalRead(startButtonPin) == LOW) {
        // Set a random delay before transitioning to green lights
        delayBeforeGreen = random(minDelayBeforeGreen, maxDelayBeforeGreen + 1);
        Serial.println("Before random delay generation:");
        Serial.print("Current delayBeforeGreen: ");
        Serial.println(delayBeforeGreen);
        
        lightState = RED_LIGHTS;
        redLightStartTime = millis();
    }
    break;
case RED_LIGHTS:
    // Turn on outer LEDs one by one with a tone
    if (millis() - redLightStartTime >= 1000) {
        static int ledIndex = 0; // Variable to track the LED index
        static unsigned long previousLEDTime = 0; // Variable to track the previous LED turning on time

        // Calculate the time elapsed since the previous LED turning on time
        unsigned long currentTime = millis();
        unsigned long elapsedTime = currentTime - previousLEDTime;

        // Check if it's time to turn on the next LED
        if (elapsedTime >= 1000 && ledIndex < NUM_LEDS / 2) {
            // Play tone for the current LED
            if (!ledTonePlayed[ledIndex]) {
                playTone(400, 50); // Play a short beep
                ledTonePlayed[ledIndex] = true; // Mark the tone as played for this LED
            }

            // Turn on the current LED
            leds[ledIndex] = CRGB::Red;
            leds[NUM_LEDS - 1 - ledIndex] = CRGB::Red;
            FastLED.show();

            // Update the previous LED turning on time
            previousLEDTime = currentTime;

            // Move to the next LED
            ledIndex++;
        }

        // Check if all LEDs have been turned on
        if (ledIndex >= NUM_LEDS / 2) {
            // Check if the random delay before transitioning to green lights has passed
            if (delayBeforeGreen == 0) {
                // If delayBeforeGreen is 0, proceed to green lights immediately
                lightState = GREEN_LIGHTS;
            } else {
                // If delayBeforeGreen is non-zero, wait for the delay before proceeding to green lights
                lightState = WAIT_FOR_GREEN_DELAY;
                waitStartTime = currentTime; // Record the start time of the wait period
            }
            return; // Exit the loop
        }
    }
    break;
case WAIT_FOR_GREEN_DELAY:
    // Check if the random delay before transitioning to green lights has passed
    if (millis() - waitStartTime >= delayBeforeGreen) {
        lightState = GREEN_LIGHTS; // Move to the green light phase
        return; // Exit the loop
    }
    break;
case GREEN_LIGHTS:
  // Turn all LEDs green after a random delay
  if (millis() - redLightStartTime >= 5000) {
    fill_solid(leds, NUM_LEDS, CRGB::Green);
    FastLED.show();
    playTone(1000, 250); // Play a short beep
    greenLightStartTime = millis();
    lightState = TURN_OFF_LIGHTS;
  }
  break;
case TURN_OFF_LIGHTS:
  // Turn off lights after 3 seconds
  if (millis() - greenLightStartTime >= 3000) {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    lightState = IDLE;
  }
  break;
}

  // Reset button handling
  if (digitalRead(resetPin) == LOW) {
    resetLapInfo();
  }

  // Update and display lap information for Lane 1
  if (lapCounting1) {
    displayLapInfo(1, lapCount1, millis() - startTime1, bestLap1 == 0 ? "--" : String(bestLap1 / 1000.0, 3), recentLap1 == 0 ? "--" : String(recentLap1 / 1000.0, 3));
  }

  // Update and display lap information for Lane 2
  if (lapCounting2) {
    displayLapInfo(2, lapCount2, millis() - startTime2, bestLap2 == 0 ? "--" : String(bestLap2 / 1000.0, 3), recentLap2 == 0 ? "--" : String(recentLap2 / 1000.0, 3));
  }
}

// Function to update lap information
void updateLapInfo(int lane) {
  Serial.println("Updating lap info for lane " + String(lane));
  unsigned long currentTime = millis();
  unsigned long *startTime, *lapTime, *bestLap, *recentLap;
  int *lapCount;
  bool *lapCounting;

  // Determine which lane to update based on the input
  if (lane == 1) {
    startTime = &startTime1;
    lapTime = &lapTime1;
    bestLap = &bestLap1;
    recentLap = &recentLap1;
    lapCount = &lapCount1;
    lapCounting = &lapCounting1;
  } else {
    startTime = &startTime2;
    lapTime = &lapTime2;
    bestLap = &bestLap2;
    recentLap = &recentLap2;
    lapCount = &lapCount2;
    lapCounting = &lapCounting2;
  }

  // Check if lap counting is active
  if (*lapCounting) {
    *lapTime = currentTime - *startTime;
    Serial.println("Lap time: " + String(*lapTime / 1000.0, 3));

    // Update best lap if the current lap is faster
    if (*lapTime < *bestLap || *bestLap == 0) {
      *bestLap = *lapTime;
      Serial.println("Best lap: " + String(*bestLap / 1000.0, 3));
    }

    // Update recent lap and reset the lap timer
    *recentLap = *lapTime;
    *startTime = currentTime;

    // Increment lap count
    (*lapCount)++;
    Serial.println("Lap count: " + String(*lapCount));
  }
}

// Function to reset lap information
void resetLapInfo() {
  lapCounting1 = false;
  lapCounting2 = false;
  lapCountInitialized1 = false;
  lapCountInitialized2 = false;

  startTime1 = 0;
  startTime2 = 0;
  lapTime1 = 0;
  lapTime2 = 0;
  bestLap1 = 0;
  bestLap2 = 0;
  recentLap1 = 0;
  recentLap2 = 0;
  lapCount1 = 0;
  lapCount2 = 0;

  // Display initial lap information after reset
  displayLapInfo(1, lapCount1, 0, "--", "--");
  displayLapInfo(2, lapCount2, 0, "--", "--");
}

// Function to display lap information on the OLED display
void displayLapInfo(int lane, int lapCount, unsigned long currentLap, String bestLap, String recentLap) {
  Adafruit_SSD1306 *display;

  // Determine which display to use based on the lane
  if (lane == 1) {
    display = &display1;
  } else {
    display = &display2;
  }

  // Clear the display and set text color to white
  display->clearDisplay();
  display->setTextColor(WHITE);

  // Calculate the center position for Lane and Laps
  int displayWidth = display->width();
  String laneText = "Lane: " + String(lane);
  String lapsText = "Laps: " + String(lapCount);

  // Convert Strings to char arrays
  char laneCharArray[laneText.length() + 1];
  laneText.toCharArray(laneCharArray, laneText.length() + 1);

  char lapsCharArray[lapsText.length() + 1];
  lapsText.toCharArray(lapsCharArray, lapsText.length() + 1);

  int textWidthLane = 0;
  int textWidthLaps = 0;

  // Set the larger text size for Lane and Laps
  display->setTextSize(2);

  // Get text bounds for Lane
  int16_t x1, y1;
  uint16_t w, h;
  display->getTextBounds(laneCharArray, 0, 0, &x1, &y1, &w, &h);
  textWidthLane = w;

  // Get text bounds for Laps
  display->getTextBounds(lapsCharArray, 0, 0, &x1, &y1, &w, &h);
  textWidthLaps = w;

  // Set the cursor position for Lane
  int xPosLane = (displayWidth - textWidthLane) / 2;
  display->setCursor(xPosLane, 0);
  display->println(laneText);

  // Add a bit of vertical spacing
  display->setCursor(0, display->getCursorY() + 2);

  // Set the cursor position for Laps
  int xPosLaps = (displayWidth - textWidthLaps) / 2;
  display->setCursor(xPosLaps, display->getCursorY());
  display->println(lapsText);

  // Reset text size for the remaining information
  display->setTextSize(1);

  // Add 1-pixel vertical spacing between Laps and timers
  display->setCursor(0, display->getCursorY() + 1);

  // Continue with the original layout for the time-related information
  display->print("Current Lap: ");
  display->println(String(currentLap / 1000.0, 3));
  display->println("Recent Lap: " + recentLap);
  display->println("Best Lap: " + bestLap);

  // Update the OLED display
  display->display();
}
