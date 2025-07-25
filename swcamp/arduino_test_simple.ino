#include <Stepper.h>
#include <Servo.h>

// ===== 간단한 테스트 설정 =====
// 스텝모터 설정 (28BYJ-48 + ULN2003)
const int stepsPerRevolution = 2048; // 28BYJ-48의 정확한 스텝 수
Stepper myStepper(stepsPerRevolution, 8, 9, 10, 11); // 올바른 핀 순서

// 서보모터 설정
Servo myServo;
const int servoPin = 6;

// LED 테스트용
const int testLED = 13; // 내장 LED

void setup() {
  Serial.begin(9600);
  Serial.println("🤖 아두이노 우노 모터 테스트 시작!");
  
  // LED 설정
  pinMode(testLED, OUTPUT);
  
  // 스텝모터 속도 설정 (매우 느리게 시작)
  myStepper.setSpeed(5); // 5 RPM (매우 느림)
  
  // 서보모터 초기화
  myServo.attach(servoPin);
  myServo.write(90); // 중간 위치
  delay(1000);
  
  Serial.println("✅ 초기화 완료!");
  Serial.println("명령어:");
  Serial.println("- STEP: 스텝모터 테스트");
  Serial.println("- SERVO: 서보모터 테스트");
  Serial.println("- LED: LED 테스트");
  Serial.println("- STATUS: 상태 확인");
}

void loop() {
  // LED 깜빡임 (시스템이 살아있음을 표시)
  digitalWrite(testLED, HIGH);
  delay(100);
  digitalWrite(testLED, LOW);
  delay(900);
  
  // 시리얼 명령 처리
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toUpperCase();
    
    Serial.println("받은 명령: " + command);
    
    if (command == "STEP") {
      testStepperMotor();
    } else if (command == "SERVO") {
      testServoMotor();
    } else if (command == "LED") {
      testLED_Function();
    } else if (command == "STATUS") {
      printStatus();
    } else {
      Serial.println("❌ 알 수 없는 명령: " + command);
    }
  }
}

// 스텝모터 테스트
void testStepperMotor() {
  Serial.println("🔄 스텝모터 테스트 시작...");
  
  // 전원 확인
  Serial.println("전원 상태 확인 중...");
  
  // 매우 천천히 시계방향 회전
  Serial.println("시계방향 90도 회전 (매우 느림)");
  myStepper.setSpeed(3); // 3 RPM
  myStepper.step(stepsPerRevolution / 4); // 90도
  
  delay(2000);
  
  // 반시계방향 복귀
  Serial.println("반시계방향 90도 복귀");
  myStepper.step(-stepsPerRevolution / 4); // -90도
  
  Serial.println("✅ 스텝모터 테스트 완료");
}

// 서보모터 테스트
void testServoMotor() {
  Serial.println("🔧 서보모터 테스트 시작...");
  
  // 0도 → 180도 → 0도
  Serial.println("0도로 이동");
  myServo.write(0);
  delay(1000);
  
  Serial.println("90도로 이동");
  myServo.write(90);
  delay(1000);
  
  Serial.println("180도로 이동");
  myServo.write(180);
  delay(1000);
  
  Serial.println("90도로 복귀");
  myServo.write(90);
  delay(1000);
  
  Serial.println("✅ 서보모터 테스트 완료");
}

// LED 테스트
void testLED_Function() {
  Serial.println("💡 LED 테스트 시작...");
  
  for (int i = 0; i < 10; i++) {
    digitalWrite(testLED, HIGH);
    delay(100);
    digitalWrite(testLED, LOW);
    delay(100);
  }
  
  Serial.println("✅ LED 테스트 완료");
}

// 상태 출력
void printStatus() {
  Serial.println("=== 시스템 상태 ===");
  Serial.println("아두이노: 우노");
  Serial.println("스텝모터 핀: 8,9,10,11");
  Serial.println("서보모터 핀: 6");
  Serial.println("LED 핀: 13");
  Serial.println("전원: " + String(analogRead(A7)) + " (아날로그 참조)");
  Serial.println("업타임: " + String(millis()/1000) + "초");
  Serial.println("메모리: " + String(freeMemory()) + " bytes");
  Serial.println("==================");
}

// 메모리 확인 함수
int freeMemory() {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}