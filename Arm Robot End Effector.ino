#include <Servo.h>

Servo baseServo;
Servo shoulderServo;
Servo gripperServo;   // <-- Servo end effector (gripper)

const int joyX = A0;
const int joyY = A1;
const int joyBtn = 2;
const int playBtn = 3;
const int gripperBtn = 4;   // <-- Tombol toggle buka/tutup gripper

const int baseServoPin = 9;
const int shoulderServoPin = 10;
const int gripperServoPin = 11;   // <-- Pin sinyal servo gripper

// --- Pengaturan Gripper ---
const int GRIPPER_OPEN_POS = 90;
const int GRIPPER_CLOSE_POS = 10;
bool gripperClosed = false;
bool lastGripperBtnState = HIGH;
unsigned long lastDebounceGripper = 0;

const int MAX_STEPS = 200;
int basePos[MAX_STEPS];
int shoulderPos[MAX_STEPS];
int gripperPos[MAX_STEPS];   // <-- Rekam posisi gripper juga
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
int currentGripper = GRIPPER_OPEN_POS;
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
int playTargetGripper = 0;

void setup() {
  baseServo.attach(baseServoPin);
  shoulderServo.attach(shoulderServoPin);
  gripperServo.attach(gripperServoPin);

  pinMode(joyBtn, INPUT_PULLUP);
  pinMode(playBtn, INPUT_PULLUP);
  pinMode(gripperBtn, INPUT_PULLUP);

  baseServo.write(currentBase);
  shoulderServo.write(currentShoulder);
  gripperServo.write(currentGripper);

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

    // --- Toggle gripper (debounce pakai millis) ---
    bool gripperBtnState = digitalRead(gripperBtn);
    if (gripperBtnState == LOW && lastGripperBtnState == HIGH && (now - lastDebounceGripper > DEBOUNCE_DELAY)) {
      lastDebounceGripper = now;
      gripperClosed = !gripperClosed;
      currentGripper = gripperClosed ? GRIPPER_CLOSE_POS : GRIPPER_OPEN_POS;
      gripperServo.write(currentGripper);
      Serial.println(gripperClosed ? "Gripper: MENUTUP" : "Gripper: MEMBUKA");
    }
    lastGripperBtnState = gripperBtnState;

    // --- Rekam titik (non-blocking, tidak pakai delay) ---
    if (recording && now - lastRecordTime >= RECORD_INTERVAL) {
      lastRecordTime = now;
      if (stepCount < MAX_STEPS) {
        basePos[stepCount] = targetBase;
        shoulderPos[stepCount] = targetShoulder;
        gripperPos[stepCount] = currentGripper;
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
      playTargetGripper = gripperPos[playIndex];
      gripperServo.write(playTargetGripper);
      currentGripper = playTargetGripper;
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
          playTargetGripper = gripperPos[playIndex];
          gripperServo.write(playTargetGripper);
          currentGripper = playTargetGripper;
          gripperClosed = (currentGripper == GRIPPER_CLOSE_POS);
        }
      }
    }
  }
}
