#include <Stepper.h>

// 스텝모터 설정
const int stepsPerRevolution = 2048;
Stepper myStepper(stepsPerRevolution, 8, 9, 10, 11);

// LED
const int redLED = A0;
const int greenLED = A1;
const int blueLED = A2;

// 시스템 변수
int currentAngle = 0;

void setup() {
  Serial.begin(9600);
  
  // LED 핀 설정
  pinMode(redLED, OUTPUT);
  pinMode(greenLED, OUTPUT);
  pinMode(blueLED, OUTPUT);
  
  // 스텝모터 속도 설정
  myStepper.setSpeed(5);
  
  // 시작 신호
  digitalWrite(greenLED, HIGH);
  delay(1000);
  digitalWrite(greenLED, LOW);
  
  Serial.println("Ready");
}

void loop() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    if (input.length() > 0) {
      processInput(input);
    }
  }
  
  delay(100);
}

void processInput(String input) {
  digitalWrite(blueLED, HIGH); // 동작 중
  
  Serial.print("Processing: ");
  Serial.println(input);
  
  if (input == "플라스틱") {
    rotateToAngle(0);
  } else if (input == "종이") {
    rotateToAngle(90);
  } else if (input == "캔") {
    rotateToAngle(180);
  } else if (input == "비닐") {
    rotateToAngle(270);
  } else if (input == "HOME") {
    rotateToAngle(0);
  } else if (input == "TEST") {
    runTest();
  } else {
    Serial.println("Unknown command");
    digitalWrite(redLED, HIGH);
    delay(500);
    digitalWrite(redLED, LOW);
    digitalWrite(blueLED, LOW);
    return;
  }
  
  // 완료 신호
  digitalWrite(blueLED, LOW);
  digitalWrite(greenLED, HIGH);
  delay(300);
  digitalWrite(greenLED, LOW);
  
  Serial.println("Complete");
}

void rotateToAngle(int targetAngle) {
  int angleDiff = targetAngle - currentAngle;
  
  // 최단 경로 계산
  if (angleDiff > 180) {
    angleDiff -= 360;
  } else if (angleDiff < -180) {
    angleDiff += 360;
  }
  
  if (angleDiff == 0) {
    Serial.println("Already at position");
    return;
  }
  
  int steps = (angleDiff * stepsPerRevolution) / 360;
  
  Serial.print("Rotating ");
  Serial.print(angleDiff);
  Serial.print(" degrees (");
  Serial.print(steps);
  Serial.println(" steps)");
  
  myStepper.step(steps);
  currentAngle = targetAngle;
  
  delay(500); // 안정화
}

void runTest() {
  Serial.println("Running test...");
  
  rotateToAngle(0);
  delay(1000);
  
  rotateToAngle(90);
  delay(1000);
  
  rotateToAngle(180);
  delay(1000);
  
  rotateToAngle(270);
  delay(1000);
  
  rotateToAngle(0);
  
  Serial.println("Test complete");
}
