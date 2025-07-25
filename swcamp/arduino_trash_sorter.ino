#include <Stepper.h>
#include <Servo.h>

// 스텝모터 설정 (28BYJ-48)
const int stepsPerRevolution = 2048; // 한 바퀴당 스텝 수
Stepper myStepper(stepsPerRevolution, 8, 10, 9, 11); // IN1, IN3, IN2, IN4

// 서보모터 설정
Servo myServo;
const int servoPin = 6;

// 현재 위치 저장
int currentAngle = 0; // 현재 스텝모터 각도
String currentTrashType = ""; // 현재 쓰레기 종류
bool isMoving = false; // 모터 동작 중 플래그

// 각 쓰레기 종류별 각도 매핑
struct TrashMapping {
  String type;
  int angle;
};

TrashMapping trashMap[] = {
  {"플라스틱", 0},
  {"종이", 90},
  {"캔", 180},
  {"비닐", 270}
};

void setup() {
  Serial.begin(9600);
  
  // 스텝모터 속도 설정 (RPM)
  myStepper.setSpeed(10);
  
  // 서보모터 초기화
  myServo.attach(servoPin);
  myServo.write(90); // 중간 위치로 초기화
  
  // 시스템 초기화
  Serial.println("🤖 AI 자동 분리수거 시스템 시작");
  Serial.println("지원 종류: 플라스틱(0°), 종이(90°), 캔(180°), 비닐(270°)");
  Serial.println("시리얼로 쓰레기 종류를 전송하세요...");
  
  delay(2000);
  homePosition(); // 홈 포지션으로 이동
}

void loop() {
  // 시리얼 데이터 확인
  if (Serial.available() > 0 && !isMoving) {
    String receivedData = Serial.readStringUntil('\n');
    receivedData.trim(); // 공백 제거
    
    if (receivedData.length() > 0) {
      Serial.println("받은 데이터: " + receivedData);
      processTrashType(receivedData);
    }
  }
}

// 홈 포지션으로 이동 (0도)
void homePosition() {
  Serial.println("🏠 홈 포지션으로 이동 중...");
  rotateToAngle(0);
  currentAngle = 0;
  currentTrashType = "";
  Serial.println("✅ 홈 포지션 완료");
}

// 쓰레기 종류 처리
void processTrashType(String trashType) {
  isMoving = true;
  
  // 쓰레기 종류에 따른 각도 찾기
  int targetAngle = -1;
  for (int i = 0; i < 4; i++) {
    if (trashMap[i].type == trashType) {
      targetAngle = trashMap[i].angle;
      break;
    }
  }
  
  if (targetAngle == -1) {
    Serial.println("❌ 알 수 없는 쓰레기 종류: " + trashType);
    Serial.println("지원 종류: 플라스틱, 종이, 캔, 비닐");
    isMoving = false;
    return;
  }
  
  Serial.println("🎯 " + trashType + " 감지됨 → " + String(targetAngle) + "도로 이동");
  
  // 같은 종류면 가만히 있기
  if (currentTrashType == trashType && currentAngle == targetAngle) {
    Serial.println("💡 같은 종류입니다. 서보모터만 동작합니다.");
    activateServo();
    isMoving = false;
    return;
  }
  
  // 스텝모터 회전
  rotateToAngle(targetAngle);
  currentAngle = targetAngle;
  currentTrashType = trashType;
  
  Serial.println("⏱️ 1초 대기 중...");
  delay(1000); // 1초 대기
  
  // 서보모터 동작
  activateServo();
  
  isMoving = false;
  Serial.println("✅ 분류 완료! 다음 쓰레기를 기다립니다...");
}

// 스텝모터를 특정 각도로 회전
void rotateToAngle(int targetAngle) {
  // 현재 각도에서 목표 각도까지의 차이 계산
  int angleDiff = targetAngle - currentAngle;
  
  // 최단 경로로 회전하도록 조정
  if (angleDiff > 180) {
    angleDiff -= 360;
  } else if (angleDiff < -180) {
    angleDiff += 360;
  }
  
  if (angleDiff == 0) {
    Serial.println("💡 이미 목표 각도에 있습니다.");
    return;
  }
  
  // 각도를 스텝 수로 변환
  int steps = (angleDiff * stepsPerRevolution) / 360;
  
  Serial.println("🔄 " + String(currentAngle) + "° → " + String(targetAngle) + "° 회전 중... (" + String(abs(steps)) + " 스텝)");
  
  // 스텝모터 회전
  myStepper.step(steps);
  
  Serial.println("✅ " + String(targetAngle) + "°에 도달했습니다.");
}

// 서보모터 동작 (쓰레기 떨어뜨리기)
void activateServo() {
  Serial.println("🔧 서보모터 동작 시작 - 쓰레기 떨어뜨리기");
  
  // 시작 위치 (한쪽 끝)
  myServo.write(0);
  delay(500);
  
  // 끝에서 끝까지 천천히 이동
  Serial.println("📏 0° → 180° 이동 중...");
  for (int pos = 0; pos <= 180; pos += 2) {
    myServo.write(pos);
    delay(20); // 부드러운 움직임을 위한 딜레이
  }
  
  delay(200); // 끝에서 잠시 대기
  
  // 다시 원래 위치로 복귀
  Serial.println("🔄 180° → 90° 복귀 중...");
  for (int pos = 180; pos >= 90; pos -= 2) {
    myServo.write(pos);
    delay(20);
  }
  
  Serial.println("✅ 서보모터 동작 완료 - 중간 위치 복귀");
}

// 시스템 상태 출력 (디버깅용)
void printStatus() {
  Serial.println("=== 시스템 상태 ===");
  Serial.println("현재 각도: " + String(currentAngle) + "°");
  Serial.println("현재 쓰레기: " + currentTrashType);
  Serial.println("동작 중: " + String(isMoving ? "예" : "아니오"));
  Serial.println("==================");
}

// 수동 테스트 명령어 처리
void handleTestCommands(String command) {
  if (command == "TEST") {
    Serial.println("🧪 테스트 모드 시작");
    processTrashType("플라스틱");
    delay(3000);
    processTrashType("종이");
    delay(3000);
    processTrashType("캔");
    delay(3000);
    processTrashType("비닐");
    delay(3000);
    homePosition();
    Serial.println("🧪 테스트 완료");
  }
  else if (command == "HOME") {
    homePosition();
  }
  else if (command == "STATUS") {
    printStatus();
  }
  else if (command == "SERVO") {
    activateServo();
  }
}