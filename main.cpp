// Include necessary libraries
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#define FASTLED_INTERNAL
#include <FastLED.h>
#include <FS.h>
#include <SPIFFS.h>
#include <AsyncTCP.h>
#include <ElegantOTA.h>

// Define Wi-Fi credentials
const char* ssid = "SSID";
const char* password = "PASSWORD";

// Create instances of SSD1306 displays and AsyncWebServer
Adafruit_SSD1306 display1(128, 64, &Wire, -1);
Adafruit_SSD1306 display2(128, 64, &Wire, -1);
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

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
#define LED_PIN     13
#define NUM_LEDS    8
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB

// Create instace of CRGB array
CRGB leds[NUM_LEDS];

// Debouncing variables
unsigned long lastButtonPress = 0;
unsigned long lastDebounceTime1 = 0;
unsigned long lastDebounceTime2 = 0;
unsigned long lastDebounceTimeReset = 0;
unsigned long debounceDelay = 100;
volatile bool lapFlag1 = false;
volatile bool lapFlag2 = false;
volatile unsigned long lastInterruptTime1 = 0;
volatile unsigned long lastInterruptTime2 = 0;
const unsigned long debounceTime = 50; // Adjust based on your requirements

// Lap counting and initialization flags
bool lapCounting1 = false;
bool lapCounting2 = false;
bool lapCountInitialized1 = false;
bool lapCountInitialized2 = false;
bool ledTonePlayed[NUM_LEDS] = {false};
bool isFirstLap1 = true;
bool isFirstLap2 = true;

bool forceRefresh = false; // Global variable to control page refresh

// Lap press counters
int lapPressCount1 = 0;
int lapPressCount2 = 0;

// Define the delay duration
const unsigned long lapCountDelay = 250;
const unsigned long debounceDelayReset = 50;
const unsigned long minDelayBeforeGreen = 1000;
const unsigned long maxDelayBeforeGreen = 2000;
unsigned long startTimestamp = 0;
unsigned long randomDelay = 0;

// Function to play tone
void playTone(int frequency, int duration) {
  tone(buzzerPin, frequency);
  delay(duration);
  noTone(buzzerPin);
}
void updateLapInfo(int lane);
//void resetLapInfo();
void displayLapInfo(int lane, int lapCount, unsigned long currentLap, String bestLap, String recentLap);

void calculateAndSetDelayBeforeGreen() {
    delayBeforeGreen = random(minDelayBeforeGreen, maxDelayBeforeGreen + 1);    
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
unsigned long waitStartTime = 0;

void resetStartSequence() {
    lightState = IDLE; // Reset the light sequence state
    for(int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::Black; // Turn off all LEDs
        ledTonePlayed[i] = false; // Reset the tone played flags for each LED
    }
    FastLED.show(); // Update the LED strip to apply the off state
}

void notifyClients() {
  if (forceRefresh) {
        // Send a message to force the client to refresh the page
        ws.textAll("{\"refresh\": true}");
        forceRefresh = false; // Reset after sending
        return;
    }
  JsonDocument doc;
  // Prepare comprehensive lap information for each lane
  // Here, we directly use lapCount1 and lapCount2 as they are initialized to 0 on the first press and incremented thereafter.
  doc["lane1"]["lapCount"] = isFirstLap1 ? "--" : String(lapCount1); // "--" for not started, or the actual count
  doc["lane1"]["recentLap"] = recentLap1 / 1000.0;
  doc["lane1"]["bestLap"] = bestLap1 / 1000.0;
  doc["lane1"]["currentLap"] = lapCounting1 ? (millis() - startTime1) / 1000.0 : static_cast<double>(-1); // -1 or similar to indicate not started

  doc["lane2"]["lapCount"] = isFirstLap2 ? "--" : String(lapCount2);
  doc["lane2"]["recentLap"] = recentLap2 / 1000.0;
  doc["lane2"]["bestLap"] = bestLap2 / 1000.0;
  doc["lane2"]["currentLap"] = lapCounting2 ? (millis() - startTime2) / 1000.0 : static_cast<double>(-1);
   if (startTimestamp > 0 && randomDelay > 0) {
    doc["startSequence"] = true;
    doc["startTimestamp"] = startTimestamp;
    doc["randomDelay"] = randomDelay;
   }

  String message;
  serializeJson(doc, message);
  ws.textAll(message);
}

void startSequence() {
  randomSeed(esp_random());
  unsigned long seed = esp_random();
  randomSeed(seed);
    resetStartSequence();
    calculateAndSetDelayBeforeGreen(); 
    startTimestamp = millis();  // Set the global startTimestamp variable
    randomDelay = delayBeforeGreen;  // Set the global randomDelay variable
    lightState = RED_LIGHTS;
    redLightStartTime = millis();
    notifyClients();
}

void resetLapTimes() {
    // Reset all lap timing and counting variables
    startTime1 = startTime2 = 0;
    lapTime1 = lapTime2 = 0;
    bestLap1 = bestLap2 = 0;
    recentLap1 = recentLap2 = 0;
    lapCount1 = lapCount2 = 0;
    lapCounting1 = lapCounting2 = false;
    isFirstLap1 = isFirstLap2 = true; // If you're using isFirstLap flags

    // Optionally, reset display or additional state variables here
    displayLapInfo(1, -1, 0, "--", "--");
    displayLapInfo(2, -1, 0, "--", "--");
    // Notify all connected clients about the reset state
    notifyClients();
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.println("Client connected");
    notifyClients(); // Optionally send current state upon new connection
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.println("Client disconnected");
  }
}

void IRAM_ATTR handleInterruptLane1() {
  unsigned long currentTime = millis();
  if (currentTime - lastInterruptTime1 > debounceTime) {
    lapFlag1 = true;
    lastInterruptTime1 = currentTime;
  }
}

void IRAM_ATTR handleInterruptLane2() {
  unsigned long currentTime = millis();
  if (currentTime - lastInterruptTime2 > debounceTime) {
    lapFlag2 = true;
    lastInterruptTime2 = currentTime;
  }
}

// Setup function
void setup() {
  Serial.begin(115200);

  // Initialize LEDC for FastLED
  ledcSetup(0, 5000, 8);
    ledcAttachPin(LED_PIN, 13);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.begin();

  // Set up button and reset pin modes
  pinMode(buttonPin1, INPUT_PULLUP);
  pinMode(buttonPin2, INPUT_PULLUP);
  pinMode(resetPin, INPUT_PULLUP);
  pinMode(startButtonPin, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(buttonPin1), handleInterruptLane1, FALLING);
  attachInterrupt(digitalPinToInterrupt(buttonPin2), handleInterruptLane2, FALLING);

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

if(!SPIFFS.begin(true)){
    Serial.println("An error occurred while mounting SPIFFS");
    return;
  }

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });

  ElegantOTA.begin(&server);    // Start ElegantOTA

  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Web reset action triggered");
    resetLapTimes(); // Resets the lap-related variables
    forceRefresh = true; // Set the flag to true to signal a page refresh
    notifyClients(); // Sends out the update, including the refresh command
    request->send(200, "text/plain", "Lap times reset successfully");
});

  server.on("/start", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Received /start web request");
    startSequence();
    request->send(200, "text/plain", "Start sequence initiated");
});

server.on("/api/updateStartSequence", HTTP_POST, [](AsyncWebServerRequest *request){
  // Simply update the startSequence flag here
  startTimestamp = 0; // Assuming 'startSequence' is the flag controlling the sequence start
  
  // Respond to the client to confirm the action
  request->send(200, "application/json", "{\"success\":true, \"message\":\"Start sequence flag updated.\"}");
  
  // You might want to notify all connected clients about this update
  notifyClients();
});
  // Add more routes as needed for other files  

  // Start the web server
  server.begin();
}

void loop() {
  ElegantOTA.loop();
  static int lastButtonState = HIGH;
  static unsigned long lastButtonPress = 0; 
  bool currentButtonState1 = digitalRead(buttonPin1);
  bool currentButtonState2 = digitalRead(buttonPin2);
  bool currentResetButtonState = digitalRead(resetPin);
  unsigned long currentTime = millis();

if (currentResetButtonState == LOW && (currentTime - lastButtonPress) > debounceDelay) {
    Serial.println("Reset button pressed");
    lastButtonPress = currentTime;
    resetLapTimes(); // Assume this resets all lap counters and timers
  }

  // Handle lap completion flags set by ISRs
  if (lapFlag1) {
    lapFlag1 = false; // Clear the flag    
    // Handle lap completion logic for lane 1
    if (!lapCounting1) {
      lapCounting1 = true;
      startTime1 = millis();
      if (isFirstLap1) {
        lapCount1 = 0; // Initialize lap count at the first press
        isFirstLap1 = false;
      }
    } else {
      updateLapInfo(1); // Update lap info for lane 1
    }
    notifyClients(); // Notify clients of the change
  }

  if (lapFlag2) {
    lapFlag2 = false; // Clear the flag
    // Handle lap completion logic for lane 2
    if (!lapCounting2) {
      lapCounting2 = true;
      startTime2 = millis();
      if (isFirstLap2) {
        lapCount2 = 0; // Similar initialization for lane 2
        isFirstLap2 = false;
      }
    } else {
      updateLapInfo(2); // Update lap info for lane 2
    }
    notifyClients(); // Notify clients of the change
  }
    
  int buttonState = digitalRead(startButtonPin);    
  if (buttonState == LOW && lastButtonState == HIGH && (currentTime - lastButtonPress) > debounceDelay) {
      lastButtonPress = currentTime;        
      // Initiate the start sequence
      startSequence();
  }
      
    lastButtonState = buttonState;
        
  switch (lightState) {
  case IDLE:
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
            ledIndex = 0;
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
    playTone(1000, 500); // Play a short beep
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

  if (digitalRead(resetPin) == LOW) {
        // Check if the current press is at least debounceDelayReset milliseconds after the last valid press
        if ((currentTime - lastDebounceTimeReset) > debounceDelayReset) {
            // Here, handle the reset logic
            resetLapTimes(); // This function resets all your lap-related variables
            forceRefresh = true; // Signal to notifyClients to send refresh command
            
            notifyClients(); // Notify all clients, now includes a refresh command

            lastDebounceTimeReset = currentTime; // Update the last debounce time
        }
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

    // Update best lap if the current lap is faster
    if (*lapTime < *bestLap || *bestLap == 0) {
      *bestLap = *lapTime;
    }

    // Update recent lap and reset the lap timer
    *recentLap = *lapTime;
    *startTime = currentTime;

    // Increment lap count
    (*lapCount)++;
    notifyClients();
  }
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
  String lapsText = lapCount >= 0 ? "Laps: " + String(lapCount) : "Laps: --";

  // Convert Strings to char arrays
  char laneCharArray[laneText.length() + 1];
  laneText.toCharArray(laneCharArray, sizeof(laneCharArray));

  char lapsCharArray[lapsText.length() + 1];
  lapsText.toCharArray(lapsCharArray, sizeof(lapsCharArray));

  display->setTextSize(2); // Larger text for "Lane" and "Laps"

  // Center and print "Lane"
  int16_t x1, y1;
  uint16_t w, h;
  display->getTextBounds(laneCharArray, 0, 0, &x1, &y1, &w, &h);
  display->setCursor((displayWidth - w) / 2, 0);
  display->println(laneText);

  // Center and print "Laps"
  display->getTextBounds(lapsCharArray, 0, 0, &x1, &y1, &w, &h);
  display->setCursor((displayWidth - w) / 2, 16); // Adjust y position based on your layout
  display->println(lapsText);

  display->setTextSize(1); // Smaller text for detailed info

  // Print "Current Lap"
  display->setCursor(0, 34); // Adjust y position based on your layout
  display->print("Current Lap: ");
  display->println(currentLap > 0 ? String(currentLap / 1000.0, 3) + " s" : "--");

  // Print "Recent Lap"
  display->print("Recent Lap: ");
  display->println(recentLap != "0" && recentLap != "--" ? recentLap + " s" : "--");

  // Print "Best Lap"
  display->print("Best Lap: ");
  display->println(bestLap != "0" && bestLap != "--" ? bestLap + " s" : "--");

  // Update the OLED display
  display->display();
}
