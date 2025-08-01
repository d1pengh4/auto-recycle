#include <Stepper.h>
#include <LiquidCrystal.h>

// ===== 하드웨어 설정 =====
// 스텝모터 설정 (28BYJ-48 + ULN2003)
const int stepsPerRevolution = 2048;
Stepper myStepper(stepsPerRevolution, 8, 9, 10, 11); // IN1, IN2, IN3, IN4

// LCD 디스플레이 (16x2)
LiquidCrystal lcd(12, 13, 5, 4, 3, 2);

// LED 및 부저
const int redLED = A0;
const int greenLED = A1;
const int blueLED = A2;
const int buzzer = A3;

// ===== 시스템 변수 =====
int currentAngle = 0;
String currentTrashType = "";
bool isMoving = false;
unsigned long lastActivity = 0;
int totalProcessed = 0;

// 쓰레기 종류별 각도 (메모리 최적화)
const int angles[] = {0, 360, 720, 1080}; // 0도, 1바퀴, 2바퀴, 3바퀴
const char* types[] = {"플라스틱", "종이", "캔", "비닐"};
const char* icons[] = {"PLA", "PAP", "CAN", "VIN"};

void setup() {
  Serial.begin(9600);
  
  // 핀 모드 설정
  pinMode(redLED, OUTPUT);
  pinMode(greenLED, OUTPUT);
  pinMode(blueLED, OUTPUT);
  pinMode(buzzer, OUTPUT);
  
  // LCD 초기화
  lcd.begin(16, 2);
  lcd.clear();
  
  // 스텝모터 속도 설정
  myStepper.setSpeed(6); // 안정적인 저속
  
  // 시작 시퀀스
  startupSequence();
  
  // 홈 포지션
  homePosition();
  
  Serial.println(F("System Ready!"));
  lastActivity = millis();
}

void loop() {
  // 시리얼 데이터 처리
  if (Serial.available() > 0 && !isMoving) {
    String receivedData = Serial.readStringUntil('\n');
    receivedData.trim();
    
    if (receivedData.length() > 0) {
      Serial.print(F("Received: "));
      Serial.println(receivedData);
      
      // 명령어 처리
      if (handleCommands(receivedData)) {
        lastActivity = millis();
      }
    }
  }
  
  // 대기 모드 (30초 무활동)
  if (millis() - lastActivity > 30000 && !isMoving) {
    idleMode();
  }
  
  delay(100);
}

// ===== 시작 시퀀스 =====
void startupSequence() {
  // LCD 화면
  lcd.setCursor(0, 0);
  lcd.print(F("Smart Sorter"));
  lcd.setCursor(0, 1);
  lcd.print(F("Starting..."));
  
  // LED 시퀀스
  for (int i = 0; i < 2; i++) {
    digitalWrite(redLED, HIGH);
    delay(150);
    digitalWrite(redLED, LOW);
    digitalWrite(greenLED, HIGH);
    delay(150);
    digitalWrite(greenLED, LOW);
    digitalWrite(blueLED, HIGH);
    delay(150);
    digitalWrite(blueLED, LOW);
  }
  
  // 시작음
  playTone(262, 100); // 도
  playTone(330, 100); // 미
  playTone(392, 150); // 솔
  
  delay(500);
}

// ===== 홈 포지션 =====
void homePosition() {
  Serial.println(F("Homing..."));
  updateLCD("Homing...", "Please wait");
  
  rotateToAngle(0);
  currentAngle = 0;
  currentTrashType = "";
  
  updateLCD("Ready to Sort", "Count: 0");
  digitalWrite(greenLED, HIGH);
  delay(300);
  digitalWrite(greenLED, LOW);
  
  Serial.println(F("Home complete"));
}

// ===== 명령어 처리 =====
bool handleCommands(String command) {
  // 특수 명령어
  if (command == "TEST") {
    runTest();
    return true;
  } else if (command == "HOME") {
    homePosition();
    return true;
  } else if (command == "STATUS") {
    printStatus();
    return true;
  } else if (command == "CALIBRATE") {
    calibrateMotor();
    return true;
  } else if (command == "RESET") {
    resetSystem();
    return true;
  }
  
  // 쓰레기 분류
  return processTrash(command);
}

// ===== 쓰레기 분류 처리 =====
bool processTrash(String trashType) {
  int typeIndex = findTrashType(trashType);
  
  if (typeIndex == -1) {
    handleUnknownTrash(trashType);
    return false;
  }
  
  isMoving = true;
  digitalWrite(blueLED, HIGH);
  
  int targetAngle = angles[typeIndex];
  
  Serial.print(F("Sorting: "));
  Serial.print(types[typeIndex]);
  Serial.print(F(" -> "));
  Serial.print(targetAngle);
  Serial.println(F(" deg"));
  
  updateLCD("Sorting:", icons[typeIndex]);
  
  // 효과음
  playBeep(2, 80);
  
  // 회전
  rotateToAngle(targetAngle);
  currentAngle = targetAngle;
  currentTrashType = trashType;
  
  // 안정화
  delay(800);
  
  // 완료 처리
  totalProcessed++;
  digitalWrite(blueLED, LOW);
  digitalWrite(greenLED, HIGH);
  playSuccessSound();
  delay(200);
  digitalWrite(greenLED, LOW);
  
  updateLCD("Complete!", "Count: " + String(totalProcessed));
  
  isMoving = false;
  Serial.println(F("Sort complete"));
  
  return true;
}

// ===== 스텝모터 제어 =====
void rotateToAngle(int targetAngle) {
  int angleDiff = calculateShortestPath(targetAngle);
  
  if (angleDiff == 0) {
    Serial.println(F("Already at target"));
    return;
  }
  
  int totalSteps = abs((angleDiff * stepsPerRevolution) / 360);
  
  Serial.print(F("Steps: "));
  Serial.println(totalSteps);
  
  // 단순 회전 (메모리 절약)
  rotateSteps(angleDiff > 0 ? totalSteps : -totalSteps);
}

void rotateSteps(int steps) {
  int direction = steps > 0 ? 1 : -1;
  int totalSteps = abs(steps);
  
  // 고정 속도로 회전
  myStepper.setSpeed(6);
  
  // 블록 단위 회전 (안정성)
  int blockSize = 20;
  int remaining = totalSteps;
  
  while (remaining > 0) {
    int currentBlock = min(blockSize, remaining);
    myStepper.step(direction * currentBlock);
    remaining -= currentBlock;
    
    // 진행률 표시
    if (totalSteps > 100 && remaining % 100 == 0) {
      int progress = ((totalSteps - remaining) * 100) / totalSteps;
      updateLCD("Rotating " + String(progress) + "%", "Please wait");
    }
    
    delay(5); // 안정성
  }
}

// ===== 유틸리티 함수 =====
int findTrashType(String type) {
  for (int i = 0; i < 4; i++) {
    if (type == types[i]) {
      return i;
    }
  }
  return -1;
}

int calculateShortestPath(int targetAngle) {
  int angleDiff = targetAngle - currentAngle;
  
  // 최단 경로 계산 (360도 기준)
  if (angleDiff > 180) {
    angleDiff -= 360;
  } else if (angleDiff < -180) {
    angleDiff += 360;
  }
  
  return angleDiff;
}

void updateLCD(String line1, String line2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1.substring(0, 16));
  lcd.setCursor(0, 1);
  lcd.print(line2.substring(0, 16));
}

// ===== 사운드 효과 =====
void playBeep(int count, int duration) {
  for (int i = 0; i < count; i++) {
    digitalWrite(buzzer, HIGH);
    delay(duration);
    digitalWrite(buzzer, LOW);
    delay(duration);
  }
}

void playTone(int frequency, int duration) {
  tone(buzzer, frequency, duration);
  delay(duration + 20);
  noTone(buzzer);
}

void playSuccessSound() {
  playTone(523, 80);  // 높은 도
  playTone(659, 80);  // 높은 미
  playTone(784, 120); // 높은 솔
}

void playErrorSound() {
  for (int i = 0; i < 2; i++) {
    playTone(200, 100);
    delay(50);
  }
}

// ===== 테스트 및 캘리브레이션 =====
void runTest() {
  Serial.println(F("Running test..."));
  updateLCD("Test Mode", "Starting...");
  
  for (int i = 0; i < 4; i++) {
    Serial.print(F("Test "));
    Serial.print(i + 1);
    Serial.print(F("/4: "));
    Serial.println(types[i]);
    
    processTrash(types[i]);
    delay(1500);
  }
  
  homePosition();
  Serial.println(F("Test complete"));
}

void calibrateMotor() {
  Serial.println(F("Calibrating..."));
  updateLCD("Calibrating", "Motors...");
  
  for (int i = 0; i < 4; i++) {
    Serial.print(F("Position "));
    Serial.print(i + 1);
    Serial.print(F(": "));
    Serial.print(angles[i]);
    Serial.println(F(" deg"));
    
    rotateToAngle(angles[i]);
    delay(2000);
  }
  
  homePosition();
  Serial.println(F("Calibration done"));
}

void printStatus() {
  Serial.println(F("=== STATUS ==="));
  Serial.print(F("Angle: "));
  Serial.println(currentAngle);
  Serial.print(F("Type: "));
  Serial.println(currentTrashType);
  Serial.print(F("Count: "));
  Serial.println(totalProcessed);
  Serial.print(F("Moving: "));
  Serial.println(isMoving ? F("YES") : F("NO"));
  Serial.print(F("Uptime: "));
  Serial.print(millis() / 1000);
  Serial.println(F(" sec"));
  Serial.println(F("============"));
}

void resetSystem() {
  Serial.println(F("Resetting..."));
  totalProcessed = 0;
  homePosition();
  Serial.println(F("Reset complete"));
}

void handleUnknownTrash(String type) {
  Serial.print(F("Unknown type: "));
  Serial.println(type);
  
  updateLCD("Unknown Type", type.substring(0, 16));
  
  digitalWrite(redLED, HIGH);
  playErrorSound();
  delay(800);
  digitalWrite(redLED, LOW);
  
  updateLCD("Supported:", "PLA PAP CAN VIN");
  delay(1500);
}

void idleMode() {
  static bool ledState = false;
  static unsigned long lastBlink = 0;
  
  if (millis() - lastBlink > 1000) {
    ledState = !ledState;
    digitalWrite(greenLED, ledState);
    lastBlink = millis();
    
    if (ledState) {
      updateLCD("Idle Mode", "Waiting...");
    }
  }
}
