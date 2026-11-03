/*
 KESTRAL ESP32 2WD motor + HC-SR04 test
 Reconstructed from confirmed KESTRAL pin mapping/direction notes.
 Serial Monitor: 115200 baud
 Commands: F forward, B backward, L pivot left, R pivot right,
           S stop, 1-9 speed, D distance, H help.
 Raise wheels before testing. HC-SR04 ECHO needs a voltage divider.
*/
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  #define CORE3 1
#else
  #define CORE3 0
#endif

const int AIN1=27, AIN2=26, PWMA=25; // left motor
const int BIN1=33, BIN2=32, PWMB=18; // right motor
const int STBY=14;
const int TRIG=5, ECHO=22;
const int PWM_FREQ=1000, PWM_BITS=8;
int speedValue=180;

void pwmSetup() {
#if CORE3
  ledcAttach(PWMA, PWM_FREQ, PWM_BITS);
  ledcAttach(PWMB, PWM_FREQ, PWM_BITS);
#else
  ledcSetup(0, PWM_FREQ, PWM_BITS); ledcAttachPin(PWMA, 0);
  ledcSetup(1, PWM_FREQ, PWM_BITS); ledcAttachPin(PWMB, 1);
#endif
}
void pwmLeft(int v) {
#if CORE3
  ledcWrite(PWMA, constrain(v,0,255));
#else
  ledcWrite(0, constrain(v,0,255));
#endif
}
void pwmRight(int v) {
#if CORE3
  ledcWrite(PWMB, constrain(v,0,255));
#else
  ledcWrite(1, constrain(v,0,255));
#endif
}
void leftMotor(int v) {
  v=constrain(v,-255,255);
  if(v>0){ digitalWrite(AIN1,LOW); digitalWrite(AIN2,HIGH); pwmLeft(v); }
  else if(v<0){ digitalWrite(AIN1,HIGH); digitalWrite(AIN2,LOW); pwmLeft(-v); }
  else { pwmLeft(0); digitalWrite(AIN1,LOW); digitalWrite(AIN2,LOW); }
}
void rightMotor(int v) {
  v=constrain(v,-255,255);
  if(v>0){ digitalWrite(BIN1,LOW); digitalWrite(BIN2,HIGH); pwmRight(v); }
  else if(v<0){ digitalWrite(BIN1,HIGH); digitalWrite(BIN2,LOW); pwmRight(-v); }
  else { pwmRight(0); digitalWrite(BIN1,LOW); digitalWrite(BIN2,LOW); }
}
void drive(int left, int right){ leftMotor(left); rightMotor(right); }
void stopMotors(){ drive(0,0); }

long distanceCm() {
  digitalWrite(TRIG,LOW); delayMicroseconds(3);
  digitalWrite(TRIG,HIGH); delayMicroseconds(10);
  digitalWrite(TRIG,LOW);
  unsigned long t=pulseIn(ECHO,HIGH,30000UL);
  if(t==0) return -1;
  return (long)(t*0.0343/2.0);
}
void help(){
  Serial.println("F forward | B backward | L left | R right | S stop");
  Serial.println("1-9 speed | D distance | H help");
}
void setup(){
  Serial.begin(115200);
  pinMode(AIN1,OUTPUT); pinMode(AIN2,OUTPUT);
  pinMode(BIN1,OUTPUT); pinMode(BIN2,OUTPUT);
  pinMode(STBY,OUTPUT);
  pinMode(TRIG,OUTPUT); pinMode(ECHO,INPUT);
  digitalWrite(TRIG,LOW);
  pwmSetup();
  digitalWrite(STBY,HIGH);
  stopMotors();
  Serial.println("KESTRAL motor test ready; default speed=180");
  help();
}
void loop(){
  if(!Serial.available()) return;
  char c=Serial.read();
  if(c>='1' && c<='9'){
    speedValue=map(c-'0',1,9,80,230);
    Serial.printf("Speed=%d/255\n",speedValue);
    return;
  }
  switch(c){
    case 'F': case 'f': drive(speedValue,speedValue); Serial.println("FORWARD"); break;
    case 'B': case 'b': drive(-speedValue,-speedValue); Serial.println("BACKWARD"); break;
    case 'L': case 'l': drive(-speedValue,speedValue); Serial.println("PIVOT LEFT"); break;
    case 'R': case 'r': drive(speedValue,-speedValue); Serial.println("PIVOT RIGHT"); break;
    case 'S': case 's': stopMotors(); Serial.println("STOP"); break;
    case 'D': case 'd': { long d=distanceCm(); if(d<0) Serial.println("No echo"); else Serial.printf("Distance: %ld cm\n",d); break; }
    case 'H': case 'h': help(); break;
    case '\r': case '\n': break;
    default: Serial.println("Unknown command"); break;
  }
}
