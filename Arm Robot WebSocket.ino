#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ESP32Servo.h>

#define BASE_PIN 19
#define SHOULDER_PIN 18

// ====== Wi-Fi Config ======
const char* ssid = "UGMURO-INET";
const char* password = "Gepuk15000";

// ====== Web Server ======
AsyncWebServer server(80);

// ====== Servo Setup ======
Servo baseServo;
Servo shoulderServo;

int basePos = 90;
int shoulderPos = 90;

// ====== Recording System ======
#define MAX_STEPS 300
int recordedBase[MAX_STEPS];
int recordedShoulder[MAX_STEPS];
int stepCount = 0;
bool recording = false;
unsigned long lastRecordTime = 0;
const unsigned long recordInterval = 100; // min. ms between recorded samples

// ====== Move both servos together, smoothly ======
void smoothMoveBoth(int targetBase, int targetShoulder, int stepDelay) {
  while (basePos != targetBase || shoulderPos != targetShoulder) {
    if (basePos < targetBase) basePos++;
    else if (basePos > targetBase) basePos--;

    if (shoulderPos < targetShoulder) shoulderPos++;
    else if (shoulderPos > targetShoulder) shoulderPos--;

    baseServo.write(basePos);
    shoulderServo.write(shoulderPos);
    delay(stepDelay);
  }
}

void setup() {
  Serial.begin(115200);

  // Mount LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed!");
    return;
  }

  // Attach servos
  baseServo.attach(BASE_PIN);
  shoulderServo.attach(SHOULDER_PIN);
  baseServo.write(basePos);
  shoulderServo.write(shoulderPos);

  // Connect WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());

  // Serve HTML page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html");
  });

  // Handle slider requests, and record the position if currently recording
  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("base")) {
      basePos = request->getParam("base")->value().toInt();
      baseServo.write(basePos);
      Serial.printf("Base: %d\n", basePos);
    }
    if (request->hasParam("shoulder")) {
      shoulderPos = request->getParam("shoulder")->value().toInt();
      shoulderServo.write(shoulderPos);
      Serial.printf("Shoulder: %d\n", shoulderPos);
    }

    // Sample this position into the recording buffer, throttled by recordInterval
    if (recording && (millis() - lastRecordTime >= recordInterval)) {
      if (stepCount < MAX_STEPS) {
        recordedBase[stepCount] = basePos;
        recordedShoulder[stepCount] = shoulderPos;
        stepCount++;
        lastRecordTime = millis();
      } else {
        Serial.println("Recording buffer full, movement ignored.");
      }
    }

    request->send(200, "text/plain", "OK");
  });

  // Handle reset request (smooth simultaneous movement back to center)
  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request) {
    smoothMoveBoth(90, 90, 15);
    Serial.println("Arm back to initial position");
    request->send(200, "text/plain", "RESET DONE");
  });

  // Toggle recording on/off
  server.on("/record", HTTP_GET, [](AsyncWebServerRequest *request) {
    recording = !recording;
    if (recording) {
      stepCount = 0;
      lastRecordTime = millis();
      Serial.println("Recording started...");
      request->send(200, "text/plain", "Recording started...");
    } else {
      Serial.print("Recording stopped. Steps saved: ");
      Serial.println(stepCount);
      request->send(200, "text/plain", "Recording stopped: " + String(stepCount) + " steps saved");
    }
  });

  // Play back the recorded movement
  server.on("/playback", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (stepCount == 0) {
      request->send(200, "text/plain", "No steps recorded yet");
      return;
    }

    recording = false; // don't let the playback itself get recorded
    Serial.println("Playing back...");

    for (int i = 0; i < stepCount; i++) {
      smoothMoveBoth(recordedBase[i], recordedShoulder[i], 5);
    }

    Serial.println("Playback finished.");
    request->send(200, "text/plain", "Playback finished");
  });

  server.begin();
}

void loop() {
  // Everything is handled through the async web server callbacks above
}
