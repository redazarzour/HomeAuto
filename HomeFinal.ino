#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <driver/ledc.h>  

#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <RCSwitch.h>
#include <ESPmDNS.h>

SemaphoreHandle_t wifiSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// WiFi Configuration (Multi-AP)
WiFiMulti wifiMulti;
const char* ssid_4G = "Idoom4G_AT_B311_BB4B";
const char* password_4G = "G411DEM8A8M";
const char* ssid_portable = "Riad";
const char* password_portable = "77777777";

// Firebase Configuration
const char* firebaseHost = "dbzarzour-default-rtdb.firebaseio.com";
const char* firebaseAuth = "QSRJGBpaSq5F9hK5PpNhPfCPWx2QSihnwFYjtEzU";

// Sensor Configuration
const int flowSensorPin = 4;         // GPIO4 (Flow meter)
volatile long pulseCount = 0;
const float pulsesPerLiter = 450;
const int pressurePin = 34;     
const int BUZZER_PIN = 25;
const int BUZZER_CHANNEL = 0;     // GPIO34 (Pressure)

// Digital Inputs
const int inputPins[5] = {19, 21, 22, 23, 18};
int inputStates[5] = {0};
int previousInputStates[5] = {0};

// Digital Outputs (6 channels 12V control)
const int outputPins[6] = {13, 14, 27, 26, 25, 33};
const int ledPins[6] = {13, 14, 27, 26, 25, 33};    // Shared with outputs
int outputStates[6] = {0};

// Button states
const int buttonPins[6] = {13, 14, 27, 26, 25, 33}; // Same as outputPins
int buttonStates[6] = {0};
int lastButtonStates[6] = {0};
unsigned long lastDebounceTimes[6] = {0};
const unsigned long debounceDelay = 50;

// Alert System
const int alertLedPin = 12;
float CONSUMPTION_THRESHOLD = 200.0; // 200 liters

// Version Control System
int lastKnownVersion = 0;
unsigned long lastVersionCheck = 0;
const unsigned long VERSION_CHECK_INTERVAL = 300000; // 5 minutes

RCSwitch mySwitch = RCSwitch();

// LED Effect System
enum LedState { IDLE, PRESSED, PROCESSING, SUCCESS, ERROR, ALERT, HEARTBEAT };
LedState ledStates[6] = {IDLE};
unsigned long effectTimers[6] = {0};
int effectCounters[6] = {0};

// RGB LED Pins (modify if using single-color LEDs)
const int redPins[6] = {13, 14, 27, 26, 25, 33};
const int greenPins[6] = {12, 15, 32, 21, 22, 23};
const int bluePins[6] = {19, 18, 5, 17, 16, 4};

// System status
bool alertActive = false;
unsigned long lastHeartbeat = 0;
const unsigned long HEARTBEAT_INTERVAL = 60000; // 1 minute

const int DOOR_SENSOR_PIN = 36;  // VP pin (input only)
bool doorStatus = false;
bool lastDoorStatus = false;
unsigned long lastDoorAlert = 0;
const unsigned long DOOR_ALERT_INTERVAL = 30000; // 30 seconds
//const int BUZZER_CHANNEL = 0; // PWM channel (0-15)
unsigned long doorOpenTime = 0;

const char* SERVER_USERNAME = "user";
const char* SERVER_PASSWORD = "user";
const unsigned long NOTIFICATION_TIMEOUT = 5000; // 5 seconds

RCSwitch rfReceiver = RCSwitch();
const int rfDataPin = 15;


WebServer server(80);

// ===== FUNCTION DECLARATIONS =====
void IRAM_ATTR pulseCounter();
float readFlowSensor();
float readPressureSensor();
void readInputs();
bool checkInputChanges();
//void checkControlVersion();
void sendToFirebase(const String& path, const char* key, float value);
void sendToFirebase(const String& path, int values[], int size);
void sendToFirebase(const String& path, bool value);
void sendJsonToFirebase(const String& path, JsonDocument& doc);
void updateLedEffects();
void checkPhysicalButtons();
void toggleOutput(int channel);
void updateControlVersion();
void setRgbLed(int channel, int r, int g, int b);
void  ledcAttachPin(int  BUZZER_PIN,int BUZZER_CHANNEL);
void triggerDoorAlert();
void checkDoorStatus();
void playAlertPattern(int frequencyHz, int durationMs);
void sendAlertToPhone(const char* title, const char* message);
void playAlertPattern(int frequencyHz, int durationMs);
void triggerDoorAlert();
void printFirebaseStatus();
void setupWebServer(); 
void checkRFReote();
void fetchControlData();


void setup() {
  Serial.begin(115200);
  delay(1000); // Crucial stabilization delay
  
  // Create semaphore FIRST
  wifiSemaphore = xSemaphoreCreateMutex();
  if (wifiSemaphore == NULL) {
    Serial.println("Fatal: Failed to create WiFi semaphore");
    ESP.restart();
  }
  
  // Initialize WiFi
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(ssid_4G, password_4G);
  wifiMulti.addAP(ssid_portable, password_portable);

  

// In setup(), after WiFi connects:
if (WiFi.status() == WL_CONNECTED) {
  if (!MDNS.begin("esp32-control")) {
    Serial.println("Error setting up MDNS responder!");
  } else {
    Serial.println("mDNS responder started: esp32-control.local");
  }
}
  
  // Flow sensor interrupt (choose ONE of these)
  pinMode(flowSensorPin, INPUT);
// attachInterrupt(digitalPinToInterrupt(flowSensorPin), pulseCounter, RISING);

  

  // Sensor Initialization
  analogReadResolution(12);

  // Digital Inputs
  for (int i = 0; i < 5; i++) {
    pinMode(inputPins[i], INPUT_PULLUP);
    previousInputStates[i] = digitalRead(inputPins[i]);
  }

  // Digital Outputs
  for (int i = 0; i < 6; i++) {
    pinMode(outputPins[i], OUTPUT);
    digitalWrite(outputPins[i], LOW);
  }

  // Alert LED
  pinMode(alertLedPin, OUTPUT);
  digitalWrite(alertLedPin, LOW);

  // Web Server


  server.on("/status", HTTP_GET, []() {
    String status = "WiFi: " + String(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected") + "\n";
    status += "IP: " + WiFi.localIP().toString() + "\n";
    status += "Firebase: " + String(firebaseHost) + "\n";
    status += "Last Version: " + String(lastKnownVersion) + "\n";
    status += "Uptime: " + String(millis() / 1000) + "s\n";
    server.send(200, "text/plain", status);
  });

  server.begin();
}

  // Firebase Initial Check
  checkControlVersion();

  // Door Sensor
  pinMode(DOOR_SENSOR_PIN, INPUT);

  // Buzzer PWM Setup
  ledc_timer_config_t timer_conf = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_8_BIT,
    .timer_num = LEDC_TIMER_0,
    .freq_hz = 5000,
    .clk_cfg = LEDC_AUTO_CLK
  };
  ledc_timer_config(&timer_conf);

  ledc_channel_config_t channel_conf = {
    .gpio_num = BUZZER_PIN,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = (ledc_channel_t)BUZZER_CHANNEL,
    .timer_sel = LEDC_TIMER_0,
    .duty = 0,
    .hpoint = 0
  };
  ledc_channel_config(&channel_conf);

  // RF Receiver
  rfReceiver.enableReceive(rfDataPin);  // GPIO15 for RF data
} // <-- THIS IS THE MISSING CLOSING BRACE
void setupWebServer() {
  server.on("/notify", HTTP_GET, []() {

    
    if(server.authenticate(SERVER_USERNAME, SERVER_PASSWORD)) {
      return server.requestAuthentication();
    }
    
    Serial.println("Notification received from MIT App");
    unsigned long startTime = millis();
    bool success = false;
    
    while(millis() - startTime < NOTIFICATION_TIMEOUT && !success) {
      if(checkControlVersion()) {
        success = true;
        break;
      }
      delay(500);
    }
    
    if(success) {
      server.send(200, "application/json", "{\"status\":\"OK\",\"version\":" + String(lastKnownVersion) + "}");
    } else { 
      server.send(500, "text/plain", "Failed to update from Firebase");
    }
  });
}
// ===== MAIN LOOP =====

void loop() {
  static unsigned long lastWiFiCheck = 0;
  
  // Safe periodic WiFi check
  if (millis() - lastWiFiCheck > 30000) {
    connectToWiFi();
    lastWiFiCheck = millis();
  }

  server.handleClient();
  


  // Sensor Readings
  float flowRate = readFlowSensor();
  float pressure = readPressureSensor();

  // Check for Consumption Alert
  if (flowRate > CONSUMPTION_THRESHOLD) {
    digitalWrite(alertLedPin, HIGH);
  } else {
    digitalWrite(alertLedPin, LOW);
  }

  // Digital Input Handling
  readInputs();
  if (checkInputChanges()) {
    sendToFirebase("/inputs", inputStates, 5);
  }

  // Version-based Control Check
  if (millis() - lastVersionCheck >= VERSION_CHECK_INTERVAL) {
    checkControlVersion();
    lastVersionCheck = millis();
  }

  // LED Effects
  updateLedEffects();
  
  // Button Handling
  checkPhysicalButtons();

  // Send Sensor Data to Firebase
  sendToFirebase("/sensors", "flow", flowRate);
  sendToFirebase("/sensors", "pressure", pressure);
  checkDoorStatus();

  checkRFRemote();  // New function to handle RF signals

  // Anti-bounce Delay
  delay(1000);
}

void printFirebaseStatus() {
  static unsigned long lastPrint = 0;
  if(millis() - lastPrint > 30000) { // Every 30 seconds
    Serial.println("\nSystem Status:");
    Serial.print("WiFi: ");
    Serial.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
    Serial.print("RSSI: ");
    Serial.println(WiFi.RSSI());
    Serial.print("Firebase: ");
    Serial.println("Operational");
    Serial.print("Heap: ");
    Serial.println(ESP.getFreeHeap());
    lastPrint = millis();
  }
}

void checkRFRemote() {
  if (rfReceiver.available()) {
    unsigned long receivedCode = rfReceiver.getReceivedValue();
    int bitLength = rfReceiver.getReceivedBitlength();
    int protocol = rfReceiver.getReceivedProtocol();

    Serial.print("RF Code: ");
    Serial.print(receivedCode);
    Serial.print(" | Bits: ");
    Serial.print(bitLength);
    Serial.print(" | Protocol: ");
    Serial.println(protocol);

    // Example: Toggle output 0 if code matches curtain remote
    if (receivedCode == 11060348) {  // Replace with your actual remote code
      toggleOutput(0);  // Toggle first output channel
    }

    rfReceiver.resetAvailable();
  }
}

void checkMemory() {
  Serial.printf("Heap: Free=%d, MinEver=%d\n", 
               ESP.getFreeHeap(), 
               ESP.getMinFreeHeap());
  Serial.printf("Stack Watermark: %d\n", 
               uxTaskGetStackHighWaterMark(NULL));
}



void sendAlertToPhone(const char* title, const char* message) {
  HTTPClient http;
  String url = "https://" + String(firebaseHost) + "/notifications.json?auth=" + String(firebaseAuth);
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  
  StaticJsonDocument<200> doc;
  doc["title"] = title;
  doc["message"] = message;
  doc["timestamp"] = millis();
  
  String json;
  serializeJson(doc, json);
  http.POST(json);
  http.end();
  
  Serial.print("Alert sent: ");
  Serial.print(title);
  Serial.print(" - ");
  Serial.println(message);
}
// [Rest of your function implementations remain the same, just ensure they're after loop()]
// ===== LED EFFECT FUNCTIONS =====
void updateLedEffects() {
  for(int i = 0; i < 6; i++) {
    // Declare all variables at the start of the switch scope
    int brightness = 0;
    int intensity = 0;

    
    switch(ledStates[i]) {
      case PRESSED:
        if(millis() - effectTimers[i] < 100) {
          setRgbLed(i, 255, 255, 255);
        } else {
          ledStates[i] = PROCESSING;
          effectTimers[i] = millis();
        }
        break;
        
      case PROCESSING:
        // Breathing blue effect during processing
        effectCounters[i] = (millis() - effectTimers[i]) / 10;
        brightness = 128 + 127 * sin(effectCounters[i] * 0.1); // Now using pre-declared variable
        setRgbLed(i, 0, 0, brightness);
        if(millis() - effectTimers[i] > 3000) { // 3 second timeout
          ledStates[i] = ERROR;
          effectTimers[i] = millis();
        }
        break;
        
      case SUCCESS:
        // Green pulse
        if(millis() - effectTimers[i] < 500) {
          intensity = 255 * (1 - (millis() - effectTimers[i]) / 500.0); // Using pre-declared variable
          setRgbLed(i, 0, intensity, 0);
        } else {
          ledStates[i] = IDLE;
        }
        break;
        
      case ALERT:
        // Fast red blinking
        if((millis() / 200) % 2 == 0) {
          setRgbLed(i, 255, 0, 0);
        } else {
          setRgbLed(i, 0, 0, 0);
        }
        break;
        
      case HEARTBEAT:
        // Single white pulse for system heartbeat
        if(millis() - effectTimers[i] < 100) {
          setRgbLed(i, 255, 255, 255);
        } else {
          ledStates[i] = IDLE;
        }
        break;
        
      case IDLE:
      default:
        // Normal operation - show output state
        setRgbLed(i, outputStates[i] ? 0 : 255, outputStates[i] ? 255 : 0, 0);
    }
  }

 
  
  if(millis() - lastHeartbeat > HEARTBEAT_INTERVAL) {
    for(int i = 0; i < 6; i++) {
      ledStates[i] = HEARTBEAT;
      effectTimers[i] = millis();
    }
    lastHeartbeat = millis();
  }
}

// ===== BUTTON HANDLING =====
void checkPhysicalButtons() {
  for (int i = 0; i < 6; i++) {
    int reading = digitalRead(buttonPins[i]);
    
    if (reading != lastButtonStates[i]) {
      lastDebounceTimes[i] = millis();
    }
    
    if ((millis() - lastDebounceTimes[i]) > debounceDelay) {
      if (reading != buttonStates[i]) {
        buttonStates[i] = reading;
        if (buttonStates[i] == LOW) {
          toggleOutput(i);
        }
      }
    }
    lastButtonStates[i] = reading;
  }
}
void toggleOutput(int channel) {
  outputStates[channel] = !outputStates[channel];
  digitalWrite(ledPins[channel], outputStates[channel]);
  
  // Update Firebase
  String path = "/controls/output_" + String(channel+1);
  sendToFirebase(path, outputStates[channel]);
  
  // Also update the version to notify other devices
  updateControlVersion();
}

void updateControlVersion() {
  HTTPClient http;
  String url = "https://" + String(firebaseHost) + "/controls/control_version.json?auth=" + String(firebaseAuth);
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  String json = "{\".sv\": \"timestamp\"}"; // Use Firebase server timestamp
  http.PATCH(json);
  http.end();
}
void setRgbLed(int channel, int r, int g, int b) {
  analogWrite(redPins[channel], r);
  analogWrite(greenPins[channel], g);
  analogWrite(bluePins[channel], b);
}
// ===== SENSOR FUNCTIONS =====
float readFlowSensor() {
  static unsigned long lastTime = 0;
  if (millis() - lastTime > 1000) {
    detachInterrupt(digitalPinToInterrupt(flowSensorPin));
    float flow = (pulseCount / pulsesPerLiter) * 60; // L/min
    pulseCount = 0;
    lastTime = millis();
    //attachInterrupt(digitalPinToInterrupt(flowSensorPin), pulseCounter, RISING);
    return flow;
  }
  return 0;
}
float readPressureSensor() {
  int raw = analogRead(pressurePin);
  return (raw * (3.3 / 4095.0) - 0.5) * (5.0 / 4.0); // 0.5-4.5V → 0-5 PSI
}

// ===== INPUT FUNCTIONS =====
void readInputs() {
  for (int i = 0; i < 5; i++) {
    inputStates[i] = digitalRead(inputPins[i]);
  }
}

bool checkInputChanges() {
  for (int i = 0; i < 5; i++) {
    if (inputStates[i] != previousInputStates[i]) {
      previousInputStates[i] = inputStates[i];
      return true;
    }
  }
  return false;
}


void triggerDoorAlert() {
  // Visual alert
  for(int i=0; i<6; i++) setRgbLed(i, 255, 0, 0);
  
  // Buzzer patterns
  if(millis() - lastDoorAlert < 300000) { // First 5 minutes
    playAlertPattern(2000, 500); // High pitch intermittent
  } else {
    playAlertPattern(1000, 1000); // Continuous lower pitch
  }
  
  // Firebase notification
  sendAlertToPhone("Door Alert", "Front door left open!");
}
void connectToWiFi() {
  if (xSemaphoreTake(wifiSemaphore, pdMS_TO_TICKS(1000))) {
    if (wifiMulti.run() != WL_CONNECTED) {
      Serial.println("WiFi connection in progress...");
      
      // Add more robust connection handling
      WiFi.disconnect();
      delay(100);
      WiFi.mode(WIFI_STA);
      
      int attempts = 0;
      while (wifiMulti.run() != WL_CONNECTED && attempts < 5) {
        Serial.print(".");
        delay(500);
        attempts++;
      }
      
      if (wifiMulti.run() != WL_CONNECTED) {
        Serial.println("\nFailed to connect to WiFi");
      } else {
        Serial.println("\nConnected to WiFi");
      }
    }
    xSemaphoreGive(wifiSemaphore);
  } else {
    Serial.println("Failed to acquire WiFi semaphore");
  }
}

void playAlertPattern(int frequencyHz, int durationMs) {
  for(int i=0; i<3; i++) {
    ledcWriteTone(BUZZER_CHANNEL, frequencyHz);
    delay(durationMs/2);
    ledcWriteTone(BUZZER_CHANNEL, 0); // Off
    delay(durationMs/2);
  }
}
void checkDoorStatus() {
  doorStatus = digitalRead(DOOR_SENSOR_PIN); // HIGH when closed, LOW when open
  
  if(doorStatus != lastDoorStatus) {
    sendToFirebase("/door/status", doorStatus);
    
    if(!doorStatus && millis() - lastDoorAlert > DOOR_ALERT_INTERVAL) {
      triggerDoorAlert();
      lastDoorAlert = millis();
    }
    
    lastDoorStatus = doorStatus;

    if(!doorStatus && doorOpenTime == 0) {
      doorOpenTime = millis();
    } 
    else if(doorStatus) {
      doorOpenTime = 0;
    }
  
    if(doorOpenTime > 0 && millis() - doorOpenTime > 300000) { // 5 minutes
      sendAlertToPhone("Door Warning", "Door open for 5 minutes!");
      doorOpenTime = millis(); // Reset timer
    }
  }
}
// ===== FIREBASE CONTROL FUNCTIONS =====
// Modify your checkControlVersion to return bool
bool checkControlVersion() {
  if (!xSemaphoreTake(wifiSemaphore, pdMS_TO_TICKS(1000))) {
    Serial.println("Failed to acquire WiFi semaphore for version check");
    return false;
  }

  HTTPClient http;
  String versionUrl = "https://" + String(firebaseHost) + "/controls/control_version.json?auth=" + String(firebaseAuth);
  
  http.begin(versionUrl);
  http.setTimeout(5000); // 5 second timeout
  int httpCode = http.GET();
  
  bool success = false;
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    int currentVersion = payload.toInt();
    
    if (currentVersion != lastKnownVersion) {
      if(fetchControlData()) {
        lastKnownVersion = currentVersion;
        success = true;
      }
    } else {
      success = true; // Already up to date
    }
  } else {
    Serial.printf("Error checking version: %d\n", httpCode);
  }

  http.end();
  xSemaphoreGive(wifiSemaphore);
  return success;
}

// Enhanced fetchControlData with return value
bool fetchControlData() {
  if (!xSemaphoreTake(wifiSemaphore, pdMS_TO_TICKS(1000))) {
    Serial.println("Failed to acquire WiFi semaphore for data fetch");
    return false;
  }

  HTTPClient http;
  String url = "https://" + String(firebaseHost) + "/controls.json?auth=" + String(firebaseAuth);
  
  http.begin(url);
  http.setTimeout(5000); // 5 second timeout
  int httpCode = http.GET();
  
  bool success = false;
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    StaticJsonDocument<300> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      for (int i = 0; i < 6; i++) {
        String key = "output_" + String(i+1);
        if (doc.containsKey(key)) {
          int newState = doc[key];
          if (newState != outputStates[i]) {
            digitalWrite(outputPins[i], newState ? HIGH : LOW);
            outputStates[i] = newState;
            Serial.println("Output " + String(i+1) + " → " + (newState ? "ON" : "OFF"));
          }
        }
      }
      success = true;
    } else {
      Serial.print("JSON deserialize error: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.printf("Error fetching controls: %d\n", httpCode);
  }
  
  http.end();
  xSemaphoreGive(wifiSemaphore);
  return success;
}
   
bool ensureWiFiConnection() {
  if (xSemaphoreTake(wifiSemaphore, pdMS_TO_TICKS(1000))) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Reconnecting to WiFi...");
      WiFi.disconnect();
      delay(100);
      WiFi.mode(WIFI_STA);
      
      int attempts = 0;
      while (wifiMulti.run() != WL_CONNECTED && attempts < 5) {
        Serial.print(".");
        delay(500);
        attempts++;
      }
    }
    bool connected = (WiFi.status() == WL_CONNECTED);
    xSemaphoreGive(wifiSemaphore);
    return connected;
  }
  return false;
}



 void networkMonitor() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 60000) { // Every minute
    Serial.print("WiFi status: ");
    Serial.println(WiFi.status());
    Serial.print("RSSI: ");
    Serial.println(WiFi.RSSI());
    lastCheck = millis();
    
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Network connection lost!");
    }
  }
}
// ===== FIREBASE COMMUNICATION FUNCTIONS =====
void sendToFirebaseWithRetry(const String& path, JsonDocument& doc, int maxRetries = 3) {
  for (int attempt = 0; attempt < maxRetries; attempt++) {
    if (!ensureWiFiConnection()) {
      Serial.println("No WiFi connection for Firebase");
      continue;
    }

    HTTPClient http;
    String url = "https://" + String(firebaseHost) + path + ".json?auth=" + String(firebaseAuth);
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(10000);  // 10 second timeout
    
    String json;
    serializeJson(doc, json);
    
    int httpCode = http.PATCH(json);
    
    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK) {
        Serial.println("Firebase update successful");
      } else {
        Serial.printf("Firebase returned code: %d\n", httpCode);
      }
      http.end();
      return;
    } else {
      Serial.printf("Firebase attempt %d failed: %s\n", attempt+1, http.errorToString(httpCode).c_str());
    }
    
    http.end();
    delay(1000 * (attempt + 1)); // Exponential backoff
  }
  Serial.println("Failed to update Firebase after retries");
}


void verifyFirebaseConnection() {
  if (ensureWiFiConnection()) {
    HTTPClient http;
    String testUrl = "https://" + String(firebaseHost) + "/.json?auth=" + String(firebaseAuth) + "&print=silent";
    
    http.begin(testUrl);
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {
      Serial.println("Firebase connection verified");
    } else {
      Serial.printf("Firebase verification failed: %d\n", httpCode);
      if (httpCode == HTTP_CODE_UNAUTHORIZED) {
        Serial.println("Check your Firebase auth token!");
      }
    }
    http.end();
  }
}
void sendJsonToFirebase(const String& path, JsonDocument& doc) {
  sendToFirebaseWithRetry(path, doc);
}

//void sendToFirebase(const String& path, const char* key, float value) {
  //StaticJsonDocument<200> doc;
 // doc[key] = value;
 // sendToFirebaseWithRetry(path, doc);
//}

void sendToFirebase(const String& path, int values[], int size) {
  StaticJsonDocument<200> doc;
  for (int i = 0; i < size; i++) {
    doc["input_" + String(i+1)] = values[i];
  }
  sendToFirebaseWithRetry(path, doc);
}

void sendToFirebase(const String& path, bool value) {
  StaticJsonDocument<64> doc;  // Small document for single value
  doc["value"] = value;
  sendToFirebaseWithRetry(path, doc);
}

// Improved sendToFirebase with better error reporting
void sendToFirebase(const String& path, const char* key, float value) {
  if(WiFi.status() != WL_CONNECTED) {
    Serial.println("Skipping Firebase update - WiFi disconnected");
    return;
  }

  StaticJsonDocument<200> doc;
  doc[key] = value;
  
  HTTPClient http;
  String url = "https://" + String(firebaseHost) + path + ".json?auth=" + String(firebaseAuth);
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  
  String json;
  serializeJson(doc, json);
  int httpCode = http.PATCH(json);
  
  if(httpCode == HTTP_CODE_OK) {
    Serial.printf("Firebase update to %s successful\n", path.c_str());
  } else {
    Serial.printf("Firebase error %d on path %s\n", httpCode, path.c_str());
  }
  
  http.end();
}