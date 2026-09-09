#include <Servo.h>

Servo baseServo;
Servo shoulderServo;

const int joyX = A0;
const int joyY = A1;
const int joyBtn = 2;
const int playBtn = 3;

const int baseServoPin = 9;
const int shoulderServoPin = 10;

const int MAX_STEPS = 200;
int basePos[MAX_STEPS];
int shoulderPos[MAX_STEPS];
int stepCount = 0;

// --- State ---
enum Mode { MANUAL, PLAYING };
Mode mode = MANUAL;

bool recording = false;
bool lastJoyBtnState = HIGH;
bool lastPlayBtnState = HIGH;

// --- Posisi servo saat ini & target ---
int currentBase = 90;
int currentShoulder = 90;
int targetBase = 90;
int targetShoulder = 90;

// --- Timing non-blocking (pengganti delay) ---
unsigned long lastMoveTime = 0;
const int MOVE_INTERVAL = 12;   // ms per langkah derajat saat mode manual (atur untuk kecepatan/kehalusan)

unsigned long lastRecordTime = 0;
const int RECORD_INTERVAL = 100; // jeda antar titik rekaman

unsigned long lastDebounceJoy = 0;
unsigned long lastDebouncePlay = 0;
const int DEBOUNCE_DELAY = 250;

// --- Timing untuk playback ---
unsigned long lastPlayMoveTime = 0;
const int PLAY_MOVE_INTERVAL = 8; // ms per langkah derajat saat playback (lebih cepat dari manual)
int playIndex = 0;
int playTargetBase = 0;
int playTargetShoulder = 0;

void setup() {
  baseServo.attach(baseServoPin);
  shoulderServo.attach(shoulderServoPin);

  pinMode(joyBtn, INPUT_PULLUP);
  pinMode(playBtn, INPUT_PULLUP);

  baseServo.write(currentBase);
  shoulderServo.write(currentShoulder);

  Serial.begin(9600);
  Serial.println("System Start");
}

void loop() {
  unsigned long now = millis();

  if (mode == MANUAL) {
    // --- Baca Joystick (selalu dibaca tiap loop, tidak pernah diblok) ---
    int joyValX = analogRead(joyX);
    int joyValY = analogRead(joyY);
    targetBase = map(joyValX, 0, 1023, 0, 180);
    targetShoulder = map(joyValY, 0, 1023, 0, 180);

    // --- Gerakkan servo satu derajat tiap MOVE_INTERVAL ms (non-blocking) ---
    if (now - lastMoveTime >= MOVE_INTERVAL) {
      lastMoveTime = now;
      bool moved = false;

      if (currentBase != targetBase) {
        currentBase += (currentBase < targetBase) ? 1 : -1;
        baseServo.write(currentBase);
        moved = true;
      }
      if (currentShoulder != targetShoulder) {
        currentShoulder += (currentShoulder < targetShoulder) ? 1 : -1;
        shoulderServo.write(currentShoulder);
        moved = true;
      }
    }

    // --- Rekam titik (non-blocking, tidak pakai delay) ---
    if (recording && now - lastRecordTime >= RECORD_INTERVAL) {
      lastRecordTime = now;
      if (stepCount < MAX_STEPS) {
        basePos[stepCount] = targetBase;
        shoulderPos[stepCount] = targetShoulder;
        stepCount++;
      } else {
        recording = false;
        Serial.println("Memori rekaman penuh, rekam dihentikan otomatis.");
      }
    }

    // --- Toggle rekam (debounce pakai millis, bukan delay) ---
    bool joyBtnState = digitalRead(joyBtn);
    if (joyBtnState == HIGH && lastJoyBtnState == LOW && (now - lastDebounceJoy > DEBOUNCE_DELAY)) {
      lastDebounceJoy = now;
      recording = !recording;
      if (recording) {
        stepCount = 0;
        lastRecordTime = now;
        Serial.println("Recording started...");
      } else {
        Serial.print("Recording stopped. Steps saved: ");
        Serial.println(stepCount);
      }
    }
    lastJoyBtnState = joyBtnState;

    // --- Mulai playback ---
    bool playBtnState = digitalRead(playBtn);
    if (playBtnState == LOW && lastPlayBtnState == HIGH && (now - lastDebouncePlay > DEBOUNCE_DELAY) && stepCount > 0 && !recording) {
      lastDebouncePlay = now;
      Serial.println("Playing back...");
      mode = PLAYING;
      playIndex = 0;
      playTargetBase = basePos[playIndex];
      playTargetShoulder = shoulderPos[playIndex];
    }
    lastPlayBtnState = playBtnState;

  } else if (mode == PLAYING) {
    // --- Playback non-blocking: gerak satu derajat tiap PLAY_MOVE_INTERVAL ms ---
    if (now - lastPlayMoveTime >= PLAY_MOVE_INTERVAL) {
      lastPlayMoveTime = now;

      if (currentBase != playTargetBase) {
        currentBase += (currentBase < playTargetBase) ? 1 : -1;
        baseServo.write(currentBase);
      }
      if (currentShoulder != playTargetShoulder) {
        currentShoulder += (currentShoulder < playTargetShoulder) ? 1 : -1;
        shoulderServo.write(currentShoulder);
      }

      // Sudah sampai di titik ini? lanjut ke titik berikutnya
      if (currentBase == playTargetBase && currentShoulder == playTargetShoulder) {
        playIndex++;
        if (playIndex >= stepCount) {
          // Selesai playback, kembali ke mode manual
          targetBase = currentBase;
          targetShoulder = currentShoulder;
          mode = MANUAL;
          Serial.println("Playback finished.");
        } else {
          playTargetBase = basePos[playIndex];
          playTargetShoulder = shoulderPos[playIndex];
        }
      }
    }
  }
}
