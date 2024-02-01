#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <ESPAsyncWebSrv.h>

#include <WiFi.h>

const char* ssid = "Wokwi-GUEST";
const char* password = "";

Adafruit_SSD1306 display1(128, 64, &Wire, -1);
Adafruit_SSD1306 display2(128, 64, &Wire, -1);
AsyncWebServer server(80);

unsigned long startTime1, startTime2;
unsigned long lapTime1, lapTime2;
unsigned long bestLap1, bestLap2, recentLap1, recentLap2;
int lapCount1, lapCount2;

const int buttonPin1 = 14;
const int buttonPin2 = 27;
const int resetPin = 26;

unsigned long lastDebounceTime1 = 0;
unsigned long lastDebounceTime2 = 0;
unsigned long debounceDelay = 50;

bool lapCounting1 = false;
bool lapCounting2 = false;
bool lapCountInitialized1 = false;
bool lapCountInitialized2 = false;

int lapPressCount1 = 0;
int lapPressCount2 = 0;

void setup() {
  Serial.begin(9600);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  pinMode(buttonPin1, INPUT_PULLUP);
  pinMode(buttonPin2, INPUT_PULLUP);
  pinMode(resetPin, INPUT_PULLUP);

  Wire.begin();
  display1.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display2.begin(SSD1306_SWITCHCAPVCC, 0x3D);

  display1.clearDisplay();
  display2.clearDisplay();

  lapCount1 = 0;
  lapCount2 = 0;

  displayLapInfo(1, lapCount1, 0, "--", "--");
  displayLapInfo(2, lapCount2, 0, "--", "--");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = "<html><body>";
    html += "<h1>Lane 1</h1>";
    html += "<p>Laps: " + String(lapCount1) + "</p>";
    html += "<p>Current Lap: " + String(millis() - startTime1) + " ms</p>";
    html += "<p>Recent Lap: " + String(recentLap1 / 1000.0, 3) + " s</p>";
    html += "<p>Best Lap: " + String(bestLap1 / 1000.0, 3) + " s</p>";
    html += "<h1>Lane 2</h1>";
    html += "<p>Laps: " + String(lapCount2) + "</p>";
    html += "<p>Current Lap: " + String(millis() - startTime2) + " ms</p>";
    html += "<p>Recent Lap: " + String(recentLap2 / 1000.0, 3) + " s</p>";
    html += "<p>Best Lap: " + String(bestLap2 / 1000.0, 3) + " s</p>";
    html += "</body></html>";
    request->send(200, "text/html", html);
  });

  server.begin();
}

void loop() {
  if ((millis() - lastDebounceTime1) > debounceDelay) {
    if (digitalRead(buttonPin1) == LOW) {
      Serial.println("Button 1 pressed");
      if (!lapCounting1) {
        lapCounting1 = true;
        startTime1 = millis();
      } else {
        updateLapInfo(1);
      }
      lastDebounceTime1 = millis();
    }
  }
  if ((millis() - lastDebounceTime2) > debounceDelay) {
    if (digitalRead(buttonPin2) == LOW) {
      Serial.println("Button 2 pressed");
      if (!lapCounting2) {
        lapCounting2 = true;
        startTime2 = millis();
      } else {
        updateLapInfo(2);
      }
      lastDebounceTime2 = millis();
    }
  }
  if (digitalRead(resetPin) == LOW) {
    resetLapInfo();
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
  }
}

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

  displayLapInfo(1, lapCount1, 0, "--", "--");
  displayLapInfo(2, lapCount2, 0, "--", "--");
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

  display->display();
}
