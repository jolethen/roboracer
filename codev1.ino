/*
 * OLL RoboRacing Ultra-Low-Latency Race Firmware
 * Target Event: IIT Kanpur - October 24, 2026
 * Optimized for: Zero blocking code, maximum 5m sprint speed, instant disconnect check.
 */

#include "BluetoothSerial.h"
#include "esp_arduino_version.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled!
#endif

BluetoothSerial SerialBT;

// Safe GPIO Pin Assignments
const int PWMA_PIN = 25; 
const int AIN1_PIN = 26; 
const int AIN2_PIN = 27; 

const int PWMB_PIN = 18; 
const int BIN1_PIN = 32; 
const int BIN2_PIN = 33; 

const int STBY_PIN = 19; 

// PWM Settings
const int PWM_FREQ = 20000; 
const int PWM_RES = 8;      
int motorSpeed = 255;       // Full raw power for 5m sprint

// System Modes
enum OperatingMode { MODE_SUB_AUTO, MODE_MANUAL };
OperatingMode currentMode = MODE_MANUAL;

enum PersistentDir { DIR_STOP, DIR_FORWARD, DIR_BACKWARD };
PersistentDir autoBaseDir = DIR_STOP;

enum SteeringState { STEER_NONE, STEER_LEFT, STEER_RIGHT };
SteeringState tempSteer = STEER_NONE;

bool bluetoothSafetyOn = false; 
bool wasConnected = false;

void setMotorSpeeds(int leftSpeed, int rightSpeed);
void stopMotors();
void processCommand(char cmd);
void updateSubAutoMotors();
void clearAllStates();

void setup() {
  // NOTE: Serial.begin removed to eliminate UART blocking overhead during the race.

  pinMode(AIN1_PIN, OUTPUT);
  pinMode(AIN2_PIN, OUTPUT);
  pinMode(BIN1_PIN, OUTPUT);
  pinMode(BIN2_PIN, OUTPUT);
  pinMode(STBY_PIN, OUTPUT);

  digitalWrite(STBY_PIN, LOW);

  #if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(PWMA_PIN, PWM_FREQ, PWM_RES);
    ledcAttach(PWMB_PIN, PWM_FREQ, PWM_RES);
  #else
    ledcSetup(0, PWM_FREQ, PWM_RES);
    ledcAttachPin(PWMA_PIN, 0);
    ledcSetup(1, PWM_FREQ, PWM_RES);
    ledcAttachPin(PWMB_PIN, 1);
  #endif

  stopMotors();
  digitalWrite(STBY_PIN, HIGH);

  SerialBT.begin("OLL_RoboRacer");
  
  // FIX: Properly sync connection state immediately after initialization
  wasConnected = SerialBT.hasClient();
}

void loop() {
  // Instant, zero-lag Bluetooth connection monitoring every cycle
  bool currentlyConnected = SerialBT.hasClient();

  if (wasConnected && !currentlyConnected) {
    if (bluetoothSafetyOn) {
      autoBaseDir = DIR_FORWARD;
      tempSteer = STEER_NONE;
      setMotorSpeeds(motorSpeed, motorSpeed);
    } else {
      clearAllStates();
    }
  }
  wasConnected = currentlyConnected;

  // Instant command reader (non-blocking)
  while (SerialBT.available()) {
    char cmd = (char)SerialBT.read();
    if (cmd != '\n' && cmd != '\r') {
      processCommand(cmd);
    }
  }

  if (currentMode == MODE_SUB_AUTO) {
    updateSubAutoMotors();
  }
}

void processCommand(char cmd) {
  if (cmd == 'X') { bluetoothSafetyOn = true; return; }
  if (cmd == 'Y') { bluetoothSafetyOn = false; return; }

  if (cmd == 'M') { clearAllStates(); currentMode = MODE_MANUAL; return; }
  if (cmd == 'A') { clearAllStates(); currentMode = MODE_SUB_AUTO; return; }
  if (cmd == 'S') { clearAllStates(); return; }

  if (currentMode == MODE_MANUAL) {
    switch (cmd) {
      case 'F': setMotorSpeeds(motorSpeed, motorSpeed); break;
      case 'B': setMotorSpeeds(-motorSpeed, -motorSpeed); break;
      case 'L': setMotorSpeeds(-motorSpeed / 2, motorSpeed); break;
      case 'R': setMotorSpeeds(motorSpeed, -motorSpeed / 2); break;
      case 'C': stopMotors(); break;
      default: break;
    }
  } else if (currentMode == MODE_SUB_AUTO) {
    switch (cmd) {
      case 'F': autoBaseDir = DIR_FORWARD; tempSteer = STEER_NONE; break;
      case 'B': autoBaseDir = DIR_BACKWARD; tempSteer = STEER_NONE; break;
      case 'L': tempSteer = STEER_LEFT; break;
      case 'R': tempSteer = STEER_RIGHT; break;
      case 'C': tempSteer = STEER_NONE; break;
      default: break;
    }
  }
}

void updateSubAutoMotors() {
  if (autoBaseDir == DIR_STOP) {
    stopMotors();
    return;
  }

  int baseSpeed = (autoBaseDir == DIR_FORWARD) ? motorSpeed : -motorSpeed;

  if (tempSteer == STEER_NONE) {
    setMotorSpeeds(baseSpeed, baseSpeed);
  } else if (tempSteer == STEER_LEFT) {
    setMotorSpeeds(baseSpeed / 2, baseSpeed);
  } else if (tempSteer == STEER_RIGHT) {
    setMotorSpeeds(baseSpeed, baseSpeed / 2);
  }
}

void clearAllStates() {
  autoBaseDir = DIR_STOP;
  tempSteer = STEER_NONE;
  stopMotors();
}

void setMotorSpeeds(int leftSpeed, int rightSpeed) {
  digitalWrite(AIN1_PIN, leftSpeed > 0 ? HIGH : LOW);
  digitalWrite(AIN2_PIN, leftSpeed < 0 ? HIGH : LOW);

  digitalWrite(BIN1_PIN, rightSpeed > 0 ? HIGH : LOW);
  digitalWrite(BIN2_PIN, rightSpeed < 0 ? HIGH : LOW);

  #if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(PWMA_PIN, abs(leftSpeed));
    ledcWrite(PWMB_PIN, abs(rightSpeed));
  #else
    ledcWrite(0, abs(leftSpeed));
    ledcWrite(1, abs(rightSpeed));
  #endif
}

void stopMotors() {
  setMotorSpeeds(0, 0);
}
