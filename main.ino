#include <Stepper.h>
#include <LiquidCrystal.h>

int stepChunk = 1;
// ===== 하드웨어 설정 =====
// 스텝모터 설정 (28BYJ-48 + ULN2003)
const int stepsPerRevolution = 2048; // 한 바퀴당 스텝 수
Stepper myStepper(stepsPerRevolution, 10, 11, 12, 13); // IN1, IN2, IN3, IN4 -> 디지털 10,11,12,13

// LCD 디스플레이 (16x2) - 핀 재배치
LiquidCrystal lcd(7, 8, 5, 4, 3, 2);

// LED 및 부저
const int redLED = A0;      // 빨간색 LED (에러/대기)
const int greenLED = A1;    // 초록색 LED (동작 완료)
const int blueLED = A2;     // 파란색 LED (동작 중)
const int buzzer = A3;      // 부저

// ===== 시스템 변수 =====
int currentAngle = 0;       // 현재 스텝모터 각도
String currentTrashType = ""; // 현재 쓰레기 종류
bool isMoving = false;      // 모터 동작 중 플래그
unsigned long lastActivity = 0; // 마지막 활동 시간
int totalProcessed = 0;     // 처리된 쓰레기 개수

// 각 쓰레기 종류별 설정
struct TrashConfig {
  String type;
  int angle;
  String icon;
  int ledPin;
};

TrashConfig trashConfigs[] = {
  {"플라스틱", 0,   "PLA", greenLED},
  {"종이",    180,   "PAP", blueLED},
  {"캔",     360,   "CAN", redLED},
  {"비닐",    540,   "VIN", greenLED}
};

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
  
  // 스텝모터 속도 설정 (RPM)
  myStepper.setSpeed(15); // 더 빠른 속도로 조정
  
  // 시스템 시작 시퀀스
  startupSequence();
  
  // 홈 포지션으로 이동
  homePosition();
  
  Serial.println("🤖 시스템 준비 완료!");
  lastActivity = millis();
}

void loop() {
  // 시리얼 데이터 처리
  if (Serial.available() > 0 && !isMoving) {
    String receivedData = Serial.readStringUntil('\n');
    receivedData.trim();
    
    if (receivedData.length() > 0) {
      Serial.println("📨 받은 데이터: " + receivedData);
      
      // 특수 명령어 처리
      if (handleSpecialCommands(receivedData)) {
        return;
      }
      
      // 쓰레기 분류 처리
      processTrashType(receivedData);
      lastActivity = millis();
    }
  }
  
  // 대기 모드 체크 (30초 무활동 시)
  if (millis() - lastActivity > 30000 && !isMoving) {
    idleMode();
  }
  
  delay(100); // CPU 부하 감소
}

// ===== 시스템 초기화 =====
void startupSequence() {
  // LCD 스플래시 화면
  lcd.setCursor(0, 0);
  lcd.print("AI Trash Sorter");
  lcd.setCursor(0, 1);
  lcd.print("Starting...     ");
  
  // LED 시퀀스
  for (int i = 0; i < 3; i++) {
    digitalWrite(redLED, HIGH);
    delay(200);
    digitalWrite(redLED, LOW);
    digitalWrite(greenLED, HIGH);
    delay(200);
    digitalWrite(greenLED, LOW);
    digitalWrite(blueLED, HIGH);
    delay(200);
    digitalWrite(blueLED, LOW);
  }
  
  // 부저 시작음
  playStartupSound();
  
  delay(1000);
}

// ===== 홈 포지션 =====
void homePosition() {
  Serial.println("🏠 홈 포지션으로 이동...");
  updateLCD("Homing...", "Please wait");
  
  rotateToAngle(0);
  currentAngle = 0;
  currentTrashType = "";
  
  updateLCD("Ready to Sort", "Bins: 4 | Count:0");
  digitalWrite(greenLED, HIGH);
  delay(500);
  digitalWrite(greenLED, LOW);
  
  Serial.println("✅ 홈 포지션 완료");
}

// ===== 쓰레기 분류 처리 =====
void processTrashType(String trashType) {
  isMoving = true;
  digitalWrite(blueLED, HIGH); // 동작 중 표시
  
  // 쓰레기 설정 찾기
  TrashConfig* config = findTrashConfig(trashType);
  
  if (config == nullptr) {
    handleUnknownTrash(trashType);
    isMoving = false;
    digitalWrite(blueLED, LOW);
    return;
  }
  
  Serial.println("🎯 " + trashType + " → " + String(config->angle) + "°");
  updateLCD("Sorting: " + config->icon, "Angle: " + String(config->angle) + " deg");
  
  // 효과음
  playBeep(2, 100);
  
  // 스텝모터 동작
  rotateToAngle(config->angle);
  currentAngle = config->angle;
  currentTrashType = trashType;
  
  Serial.println("⏱️ 안정화 대기...");
  updateLCD("Stabilizing...", "1 second wait");
  delay(1000);
  
  // 완료 처리
  totalProcessed++;
  digitalWrite(blueLED, LOW);
  digitalWrite(greenLED, HIGH);
  playSuccessSound();
  delay(300);
  digitalWrite(greenLED, LOW);
  
  updateLCD("Complete! #" + String(totalProcessed), "Ready for next");
  
  isMoving = false;
  Serial.println("✅ 분류 완료! 총 " + String(totalProcessed) + "개 처리됨");
}

// ===== 스텝모터 제어 (가속도 포함) =====
void rotateToAngle(int targetAngle) {
  int angleDiff = calculateShortestPath(targetAngle);
  
  if (angleDiff == 0) {
    Serial.println("💡 이미 목표 위치");
    return;
  }
  
  int totalSteps = abs((angleDiff * stepsPerRevolution) / 360);
  
  Serial.println("🔄 " + String(currentAngle) + "° → " + String(targetAngle) + "°");
  Serial.println("📏 " + String(totalSteps) + " 스텝 이동");
  
  // 가속도 적용 회전
  rotateWithAcceleration(angleDiff > 0 ? totalSteps : -totalSteps);
  
  Serial.println("✅ " + String(targetAngle) + "° 도달");
}

// 대용량 회전 (큰 각도 이동)
void rotateWithAcceleration(int steps) {
  int direction = steps > 0 ? 1 : -1;
  int totalSteps = abs(steps);
  
  Serial.println("🔄 총 " + String(totalSteps) + " 스텝 회전 시작");
  
  // 더 빠른 속도로 설정
  myStepper.setSpeed(15);
  
  // 대용량 블록으로 회전 (200스텝씩)
  int stepSize = 200; // 한번에 200스텝씩 회전
  int remainingSteps = totalSteps;
  int rotationCount = 0;
  
  while (remainingSteps > 0) {
    int currentStepSize = min(stepSize, remainingSteps);
    
    Serial.println("회전 " + String(++rotationCount) + ": " + String(currentStepSize) + " 스텝");
    myStepper.step(direction * currentStepSize);
    remainingSteps -= currentStepSize;
    
    // 진행률 표시
    int progress = ((totalSteps - remainingSteps) * 100) / totalSteps;
    updateLCD("Rotating " + String(progress) + "%", "Steps: " + String(totalSteps - remainingSteps));
    Serial.println("진행률: " + String(progress) + "% (남은 스텝: " + String(remainingSteps) + ")");
    
    // 회전 확인을 위한 짧은 딜레이
    delay(50);
  }
  
  Serial.println("✅ 회전 완료: 총 " + String(totalSteps) + " 스텝 이동됨");
}

// ===== 유틸리티 함수들 =====
TrashConfig* findTrashConfig(String trashType) {
  for (int i = 0; i < 4; i++) {
    if (trashConfigs[i].type == trashType) {
      return &trashConfigs[i];
    }
  }
  return nullptr;
}

int calculateShortestPath(int targetAngle) {
  int angleDiff = targetAngle - currentAngle;
  
  // 최단 경로 계산
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
  lcd.print(line1.substring(0, 16)); // 16글자 제한
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

void playStartupSound() {
  int melody[] = {262, 294, 330, 349}; // 도, 레, 미, 파
  for (int i = 0; i < 4; i++) {
    tone(buzzer, melody[i], 200);
    delay(250);
  }
  noTone(buzzer);
}

void playSuccessSound() {
  tone(buzzer, 523, 100); // 높은 도
  delay(120);
  tone(buzzer, 659, 100); // 높은 미
  delay(120);
  tone(buzzer, 784, 200); // 높은 솔
  delay(220);
  noTone(buzzer);
}

void playErrorSound() {
  for (int i = 0; i < 3; i++) {
    tone(buzzer, 200, 100); // 낮은 부저음
    delay(150);
  }
  noTone(buzzer);
}

// ===== 특수 명령어 처리 =====
bool handleSpecialCommands(String command) {
  if (command == "TEST") {
    runFullTest();
    return true;
  } else if (command == "HOME") {
    homePosition();
    return true;
  } else if (command == "STATUS") {
    printSystemStatus();
    return true;
  } else if (command == "RESET") {
    resetSystem();
    return true;
  } else if (command == "CALIBRATE") {
    calibrateMotors();
    return true;
  }
  return false;
}

void runFullTest() {
  Serial.println("🧪 전체 시스템 테스트 시작");
  updateLCD("Full System", "Test Mode");
  
  String testTypes[] = {"플라스틱", "종이", "캔", "비닐"};
  
  for (int i = 0; i < 4; i++) {
    Serial.println("테스트 " + String(i+1) + "/4: " + testTypes[i]);
    processTrashType(testTypes[i]);
    delay(2000);
  }
  
  homePosition();
  Serial.println("🧪 테스트 완료");
}

void printSystemStatus() {
  Serial.println("=== 시스템 상태 ===");
  Serial.println("현재 각도: " + String(currentAngle) + "°");
  Serial.println("현재 타입: " + currentTrashType);
  Serial.println("처리 개수: " + String(totalProcessed));
  Serial.println("동작 상태: " + String(isMoving ? "동작중" : "대기중"));
  Serial.println("업타임: " + String(millis()/1000) + "초");
  Serial.println("==================");
}

void resetSystem() {
  Serial.println("🔄 시스템 리셋");
  totalProcessed = 0;
  homePosition();
  Serial.println("✅ 리셋 완료");
}

void calibrateMotors() {
  Serial.println("🔧 모터 캘리브레이션 시작");
  updateLCD("Calibrating", "Motors...");
  
  // 스텝모터 대용량 회전 테스트
  int testAngles[] = {0, 180, 360, 540, 0};
  
  for (int i = 0; i < 5; i++) {
    Serial.println("=== 캘리브레이션 " + String(i+1) + "/5 ===");
    Serial.println("목표 각도: " + String(testAngles[i]) + "도");
    rotateToAngle(testAngles[i]);
    Serial.println("도달 완료, 3초 대기...");
    delay(3000); // 더 긴 대기시간으로 회전 확인
  }
  
  homePosition();
  Serial.println("✅ 캘리브레이션 완료");
}

void handleUnknownTrash(String trashType) {
  Serial.println("❌ 알 수 없는 타입: " + trashType);
  updateLCD("Unknown Type", trashType);
  
  digitalWrite(redLED, HIGH);
  playErrorSound();
  delay(1000);
  digitalWrite(redLED, LOW);
  
  updateLCD("Supported Types", "PLA|PAP|CAN|VIN");
  delay(2000);
}

void idleMode() {
  static bool ledState = false;
  ledState = !ledState;
  digitalWrite(greenLED, ledState);
  
  updateLCD("Idle Mode", "Waiting...");
  delay(1000);
}
