#include <Stepper.h>
#include <Servo.h>
#include <LiquidCrystal.h>

// ===== 하드웨어 설정 =====
// 스텝모터 설정 (28BYJ-48 + ULN2003)
const int stepsPerRevolution = 2048; // 한 바퀴당 스텝 수
Stepper myStepper(stepsPerRevolution, 8, 9, 10, 11); // 올바른 핀 순서: IN1, IN2, IN3, IN4

// 서보모터 설정
Servo dropperServo;
const int servoPin = 6;

// LCD 디스플레이 (16x2)
LiquidCrystal lcd(12, 13, 5, 4, 3, 2);

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
  {"종이",    90,   "PAP", blueLED},
  {"캔",     180,   "CAN", redLED},
  {"비닐",    270,   "VIN", greenLED}
};

void setup() {
  Serial.begin(9600);
  Serial.println("🤖 AI 쓰레기 분류기 시작!");
  
  // 핀 모드 설정
  pinMode(redLED, OUTPUT);
  pinMode(greenLED, OUTPUT);
  pinMode(blueLED, OUTPUT);
  pinMode(buzzer, OUTPUT);
  
  // 모든 LED 끄기
  digitalWrite(redLED, LOW);
  digitalWrite(greenLED, LOW);
  digitalWrite(blueLED, LOW);
  
  // LCD 초기화
  lcd.begin(16, 2);
  lcd.clear();
  Serial.println("✅ LCD 초기화 완료");
  
  // 스텝모터 속도 설정 (RPM) - 우노에서 안전한 속도
  myStepper.setSpeed(5); // 5 RPM (안전한 속도)
  
  // 서보모터 초기화
  dropperServo.attach(servoPin);
  Serial.println("✅ 서보모터 연결됨");
  dropperServo.write(90); // 안전한 중간 위치로 시작
  delay(1000);
  dropperServo.write(0); // 시작 위치 (0도)
  delay(500);
  Serial.println("✅ 서보모터 초기화 완료");
  
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
  
  // 서보모터도 홈 포지션
  dropperServo.write(0);
  delay(500);
  
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
  
  // 같은 종류 체크
  if (currentTrashType == trashType && currentAngle == config->angle) {
    Serial.println("💡 같은 종류 - 서보만 동작");
    updateLCD("Same Type", "Servo Only");
    activateDropper();
  } else {
    // 스텝모터 + 서보 동작
    rotateToAngle(config->angle);
    currentAngle = config->angle;
    currentTrashType = trashType;
    
    Serial.println("⏱️ 안정화 대기...");
    updateLCD("Stabilizing...", "1 second wait");
    delay(1000);
    
    activateDropper();
  }
  
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

// 가속도 적용 회전
void rotateWithAcceleration(int steps) {
  int absSteps = abs(steps);
  int direction = steps > 0 ? 1 : -1;
  
  // 가속도 구간 (전체의 30%)
  int accelSteps = absSteps / 3;
  int maxSpeed = 8; // 최대 속도 (우노 안전)
  int minSpeed = 3;  // 최소 속도
  
  for (int i = 0; i < absSteps; i++) {
    int currentSpeed;
    
    if (i < accelSteps) {
      // 가속 구간
      currentSpeed = map(i, 0, accelSteps, minSpeed, maxSpeed);
    } else if (i > absSteps - accelSteps) {
      // 감속 구간
      currentSpeed = map(i, absSteps - accelSteps, absSteps, maxSpeed, minSpeed);
    } else {
      // 정속 구간
      currentSpeed = maxSpeed;
    }
    
    myStepper.setSpeed(currentSpeed);
    myStepper.step(direction);
    
    // 진행률 표시 (매 10% 마다)
    if (i % (absSteps / 10) == 0) {
      int progress = (i * 100) / absSteps;
      updateLCD("Rotating " + String(progress) + "%", "Please wait...");
    }
  }
}

// ===== 서보모터 제어 (업그레이드) =====
void activateDropper() {
  Serial.println("🔧 서보 드로퍼 동작 시작");
  updateLCD("Dropping Trash", "Servo Active");
  
  // 시작 위치 확인 (0도)
  dropperServo.write(0);
  delay(300);
  
  // 0도 → 180도 (부드럽게)
  Serial.println("📏 0° → 180° 이동");
  for (int pos = 0; pos <= 180; pos += 3) {
    dropperServo.write(pos);
    delay(15); // 부드러운 움직임
    
    // 중간 지점에서 효과음
    if (pos == 90) {
      playBeep(1, 50);
    }
  }
  
  delay(300); // 180도에서 잠시 대기
  
  // 180도 → 0도 (복귀)
  Serial.println("🔄 180° → 0° 복귀");
  for (int pos = 180; pos >= 0; pos -= 3) {
    dropperServo.write(pos);
    delay(15);
  }
  
  delay(200); // 안정화
  Serial.println("✅ 서보 드로퍼 동작 완료");
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
  } else if (command == "SERVO") {
    activateDropper();
    return true;
  } else if (command == "RESET") {
    resetSystem();
    return true;
  } else if (command == "CALIBRATE") {
    calibrateMotors();
    return true;
  } else if (command == "DEBUG") {
    debugMotors();
    return true;
  } else if (command == "SIMPLE") {
    simpleStepTest();
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
  
  // 스텝모터 한바퀴 회전
  for (int angle = 0; angle <= 360; angle += 90) {
    rotateToAngle(angle);
    delay(1000);
  }
  
  // 서보모터 전체 범위 테스트
  for (int i = 0; i < 3; i++) {
    activateDropper();
    delay(500);
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

// ===== 디버그 함수들 =====
void debugMotors() {
  Serial.println("🔍 모터 디버그 시작");
  updateLCD("Debug Mode", "Motor Test");
  
  // 스텝모터 핀 상태 확인
  Serial.println("스텝모터 핀 상태:");
  Serial.println("핀 8: " + String(digitalRead(8)));
  Serial.println("핀 9: " + String(digitalRead(9)));
  Serial.println("핀 10: " + String(digitalRead(10)));
  Serial.println("핀 11: " + String(digitalRead(11)));
  
  // 서보모터 테스트
  Serial.println("서보모터 테스트:");
  for (int pos = 0; pos <= 180; pos += 45) {
    Serial.println("서보 위치: " + String(pos) + "도");
    dropperServo.write(pos);
    delay(1000);
  }
  dropperServo.write(0);
  
  Serial.println("✅ 디버그 완료");
}

void simpleStepTest() {
  Serial.println("🔄 단순 스텝 테스트");
  updateLCD("Simple Step", "Testing...");
  
  // 매우 느린 속도로 단순 테스트
  myStepper.setSpeed(3); // 3 RPM
  
  Serial.println("10 스텝 전진");
  for (int i = 0; i < 10; i++) {
    myStepper.step(1);
    Serial.print(".");
    delay(100);
  }
  Serial.println();
  
  delay(1000);
  
  Serial.println("10 스텝 후진");
  for (int i = 0; i < 10; i++) {
    myStepper.step(-1);
    Serial.print(".");
    delay(100);
  }
  Serial.println();
  
  Serial.println("✅ 단순 테스트 완료");
}