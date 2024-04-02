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
const char* ssid = "The Promised LAN";
const char* password = "goldenbitch";
Adafruit_SSD1306 display1(128, 64, &Wire, -1);
Adafruit_SSD1306 display2(128, 64, &Wire, -1);
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
unsigned long startTime1, startTime2;
unsigned long lapTime1, lapTime2;
unsigned long bestLap1, bestLap2, recentLap1, recentLap2;
unsigned long redLightStartTime = 0;
unsigned long greenLightStartTime = 0;
unsigned long delayBeforeGreen = 0;
int lapCount1, lapCount2;
const int buttonPin1 = 14;
const int buttonPin2 = 27;
const int resetPin = 26;
const int startButtonPin = 25;
const int buzzerPin = 16;
#define LED_PIN     13
#define NUM_LEDS    8
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];
unsigned long lastButtonPress = 0;
unsigned long lastDebounceTime1 = 0;
unsigned long lastDebounceTime2 = 0;
unsigned long debounceDelay = 100;
bool lapCounting1 = false;
bool lapCounting2 = false;
bool lapCountInitialized1 = false;
bool lapCountInitialized2 = false;
bool ledTonePlayed[NUM_LEDS] = {false};
bool isFirstLap1 = true;
bool isFirstLap2 = true;
int lapPressCount1 = 0;
int lapPressCount2 = 0;
const unsigned long lapCountDelay = 250;
const unsigned long minDelayBeforeGreen = 1000;
const unsigned long maxDelayBeforeGreen = 2000;
void playTone(int frequency, int duration) {
  tone(buzzerPin, frequency);
  delay(duration);
  noTone(buzzerPin);
}
void updateLapInfo(int lane);
void displayLapInfo(int lane, int lapCount, unsigned long currentLap, String bestLap, String recentLap);
void calculateAndSetDelayBeforeGreen() {
    delayBeforeGreen = random(minDelayBeforeGreen, maxDelayBeforeGreen + 1);
    Serial.println("Before random delay generation:");
    Serial.print("Calculated delayBeforeGreen: ");
    Serial.println(delayBeforeGreen);
}
enum LightSequenceState {
  IDLE,
  RED_LIGHTS,
  GREEN_LIGHTS,
  TURN_OFF_LIGHTS,
  WAIT_FOR_GREEN_DELAY
};
LightSequenceState lightState = IDLE;
unsigned long waitStartTime = 0;
void resetStartSequence() {
    lightState = IDLE;
    for(int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::Black;
        ledTonePlayed[i] = false;
    }
    FastLED.show();
}
void startSequence() {
  randomSeed(esp_random());
  unsigned long seed = esp_random();
  randomSeed(seed);
  Serial.print("Random seed: ");
  Serial.println(seed);
    resetStartSequence();
    calculateAndSetDelayBeforeGreen();    
    lightState = RED_LIGHTS;
    redLightStartTime = millis();
}
void notifyClients() {
  JsonDocument doc;  
  doc["lane1"]["lapCount"] = isFirstLap1 ? "--" : String(lapCount1);
  doc["lane1"]["recentLap"] = recentLap1 / 1000.0;
  doc["lane1"]["bestLap"] = bestLap1 / 1000.0;
  doc["lane1"]["currentLap"] = lapCounting1 ? (millis() - startTime1) / 1000.0 : static_cast<double>(-1);
  doc["lane2"]["lapCount"] = isFirstLap2 ? "--" : String(lapCount2);
  doc["lane2"]["recentLap"] = recentLap2 / 1000.0;
  doc["lane2"]["bestLap"] = bestLap2 / 1000.0;
  doc["lane2"]["currentLap"] = lapCounting2 ? (millis() - startTime2) / 1000.0 : static_cast<double>(-1);
  String message;
  serializeJson(doc, message);
  ws.textAll(message);
}
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.println("Client connected");
    notifyClients();
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.println("Client disconnected");
  }
}
void setup() {
  Serial.begin(115200);
  ledcSetup(0, 5000, 8);
    ledcAttachPin(LED_PIN, 13);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.begin();
  pinMode(buttonPin1, INPUT_PULLUP);
  pinMode(buttonPin2, INPUT_PULLUP);
  pinMode(resetPin, INPUT_PULLUP);
  pinMode(startButtonPin, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  Wire.begin();
  display1.begin(SSD1306_SWITCHCAPVCC, 0x3D);
  display2.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display1.clearDisplay();
  display2.clearDisplay();
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
  ElegantOTA.begin(&server);
  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Reset action triggered");
    delay(1000);
    ESP.restart();
    request->send(200, "text/plain", "Reset action triggered successfully");
  });
  server.on("/start", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Received /start web request");
    startSequence();
    request->send(200, "text/plain", "Start sequence initiated");
});
  server.begin();
}
void loop() {
  ElegantOTA.loop();
  static int lastButtonState = HIGH;
  static unsigned long lastButtonPress = 0; 
  bool currentButtonState1 = digitalRead(buttonPin1);
bool currentButtonState2 = digitalRead(buttonPin2);
unsigned long currentTime = millis();
if (currentButtonState1 == LOW) {
  Serial.println("button 1 pressed");
  if ((currentTime - lastDebounceTime1) > debounceDelay) {
    lastDebounceTime1 = currentTime;
    if (!lapCounting1) {
      lapCounting1 = true;
      startTime1 = currentTime;
      if (isFirstLap1) {
        lapCount1 = 0;
        isFirstLap1 = false;
      }
      notifyClients();
    } else {
      updateLapInfo(1);
    }
  }
}
if (currentButtonState2 == LOW) {
  Serial.println("button 2 pressed");
  if ((currentTime - lastDebounceTime2) > debounceDelay) {
    lastDebounceTime2 = currentTime;
    if (!lapCounting2) {
      lapCounting2 = true;
      startTime2 = currentTime;
      if (isFirstLap2) {
        lapCount2 = 0;
        isFirstLap2 = false;
      }
      notifyClients();
    } else {
      updateLapInfo(2);
    }
  }
}    
  int buttonState = digitalRead(startButtonPin);    
  if (buttonState == LOW && lastButtonState == HIGH && (currentTime - lastButtonPress) > debounceDelay) {
      lastButtonPress = currentTime;
      startSequence();
  }      
    lastButtonState = buttonState;        
  switch (lightState) {
  case IDLE:
    break;
case RED_LIGHTS:
    if (millis() - redLightStartTime >= 1000) {
        static int ledIndex = 0;
        static unsigned long previousLEDTime = 0;
        unsigned long currentTime = millis();
        unsigned long elapsedTime = currentTime - previousLEDTime;
        if (elapsedTime >= 1000 && ledIndex < NUM_LEDS / 2) {
            if (!ledTonePlayed[ledIndex]) {
                playTone(400, 50);
                ledTonePlayed[ledIndex] = true;
            }
            leds[ledIndex] = CRGB::Red;
            leds[NUM_LEDS - 1 - ledIndex] = CRGB::Red;
            FastLED.show();
            previousLEDTime = currentTime;
            ledIndex++;
        }
        if (ledIndex >= NUM_LEDS / 2) {
            ledIndex = 0;
            if (delayBeforeGreen == 0) {
                lightState = GREEN_LIGHTS;
            } else {
                lightState = WAIT_FOR_GREEN_DELAY;
                waitStartTime = currentTime;
            }
            return;
        }
    }
    break;
case WAIT_FOR_GREEN_DELAY:
    if (millis() - waitStartTime >= delayBeforeGreen) {
        lightState = GREEN_LIGHTS;
        return;
    }
    break;
case GREEN_LIGHTS:
  if (millis() - redLightStartTime >= 5000) {
    fill_solid(leds, NUM_LEDS, CRGB::Green);
    FastLED.show();
    playTone(1000, 250);
    greenLightStartTime = millis();
    lightState = TURN_OFF_LIGHTS;
  }
  break;
case TURN_OFF_LIGHTS:
  if (millis() - greenLightStartTime >= 3000) {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    lightState = IDLE;
  }
  break;
}
  if (digitalRead(resetPin) == LOW) {
    Serial.println("Restart button pressed");
    delay(1000);
    ESP.restart();
  }
  if (lapCounting1) {
    displayLapInfo(1, lapCount1, millis() - startTime1, bestLap1 == 0 ? "--" : String(bestLap1 / 1000.0, 3), recentLap1 == 0 ? "--" : String(recentLap1 / 1000.0, 3));
  }
  if (lapCounting2) {
    displayLapInfo(2, lapCount2, millis() - startTime2, bestLap2 == 0 ? "--" : String(bestLap2 / 1000.0, 3), recentLap2 == 0 ? "--" : String(recentLap2 / 1000.0, 3));
  }
}
void updateLapInfo(int lane) {
  Serial.println("Updating lap info for lane " + String(lane));
  unsigned long currentTime = millis();
  unsigned long *startTime, *lapTime, *bestLap, *recentLap;
  int *lapCount;
  bool *lapCounting;
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
  if (*lapCounting) {
    *lapTime = currentTime - *startTime;
    Serial.println("Lap time: " + String(*lapTime / 1000.0, 3));
    if (*lapTime < *bestLap || *bestLap == 0) {
      *bestLap = *lapTime;
      Serial.println("Best lap: " + String(*bestLap / 1000.0, 3));
    }
    *recentLap = *lapTime;
    *startTime = currentTime;
    (*lapCount)++;
    Serial.println("Lap count: " + String(*lapCount));
    notifyClients();
  }
}
void displayLapInfo(int lane, int lapCount, unsigned long currentLap, String bestLap, String recentLap) {
  Adafruit_SSD1306 *display;
  if (lane == 1) {
    display = &display1;
  } else {
    display = &display2;
  }
  display->clearDisplay();
  display->setTextColor(WHITE);
  int displayWidth = display->width();
  String laneText = "Lane: " + String(lane);
  String lapsText = "Laps: " + String(lapCount);
  char laneCharArray[laneText.length() + 1];
  laneText.toCharArray(laneCharArray, laneText.length() + 1);
  char lapsCharArray[lapsText.length() + 1];
  lapsText.toCharArray(lapsCharArray, lapsText.length() + 1);
  int textWidthLane = 0;
  int textWidthLaps = 0;
  display->setTextSize(2);
  int16_t x1, y1;
  uint16_t w, h;
  display->getTextBounds(laneCharArray, 0, 0, &x1, &y1, &w, &h);
  textWidthLane = w;
  display->getTextBounds(lapsCharArray, 0, 0, &x1, &y1, &w, &h);
  textWidthLaps = w;
  int xPosLane = (displayWidth - textWidthLane) / 2;
  display->setCursor(xPosLane, 0);
  display->println(laneText);
  display->setCursor(0, display->getCursorY() + 2);
  int xPosLaps = (displayWidth - textWidthLaps) / 2;
  display->setCursor(xPosLaps, display->getCursorY());
  display->println(lapsText);
  display->setTextSize(1);
  display->setCursor(0, display->getCursorY() + 1);
  display->print("Current Lap: ");
  display->println(String(currentLap / 1000.0, 3));
  display->println("Recent Lap: " + recentLap);
  display->println("Best Lap: " + bestLap);
  display->display();
}
