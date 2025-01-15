#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

#include "limb_control.h"
#include "body_control.h"
#include "gait_control.h"
#include "calibration.h"
#include "WebServerControl.h"


Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40, Wire2);

const uint8_t motorChannels[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
const int numMotors = sizeof(motorChannels)/sizeof(motorChannels[0]);
const int numLimbs = 4;

WebServerControl webServer(80);

int   motorSpeed       = 1500;  // PWM stop frq.
const int pulseMin     = 500;   // CW max
const int pulseMax     = 2500;  // CCW max
int   runTime          = 3000;  // Run time
int   pulsesForFullMov = 3;     // Elongate/retract loops

// -----------------------------
// Limb, Body, Gait, Calibration
// -----------------------------
LimbControl limbs[numMotors] = {
  LimbControl(pwm, motorChannels[0]), // M1
  LimbControl(pwm, motorChannels[1]), // M2
  LimbControl(pwm, motorChannels[2]), // M3
  LimbControl(pwm, motorChannels[3]), // M4
  LimbControl(pwm, motorChannels[4]), // M5
  LimbControl(pwm, motorChannels[5]), // M6
  LimbControl(pwm, motorChannels[6]), // M7
  LimbControl(pwm, motorChannels[7]), // M8
  LimbControl(pwm, motorChannels[8]), // M9 (Body1)
  LimbControl(pwm, motorChannels[9])  // M10(Body2) - If using two motors for body
};

BodyControl bodyControl(limbs[8], limbs[9]);
GaitControl gaitControl(limbs, numMotors); 
Calibration calibration(limbs, numLimbs);

struct MotorTask {
  bool active;
  unsigned long endMs;
};
MotorTask motorTasks[numMotors];

// -----------------------------
// Serial Input
// -----------------------------
String inputString    = "";
bool   stringComplete = false;

// -----------------------------
// Finite State Machine (FSM)
// -----------------------------
enum MainState {
  STATE_IDLE,
  STATE_INDIVIDUAL,
  STATE_BODY,
  STATE_GAIT,
  STATE_STAND_UP,
  STATE_SIT_DOWN,
  STATE_ELONGATE,
  STATE_RETRACT
};

MainState mainState = STATE_IDLE;

static bool standUpStarted  = false;
static bool sitDownStarted  = false;
static bool elongateStarted = false;
static bool retractStarted  = false;


void serialEvent();
void displayMessage(const char* msg);
void processCommand(String cmd);
void controlMotor(int motorIndex, uint16_t pulse, int duration);
void stopMotor(int motorIndex);
void stopMotors();
void fullyElongate();
void fullyRetract();
void motorSweepTest(int motorIndex);

// FSM Helpers
void setState(MainState newState);
void doIdleState();
void doIndividualState();
void doGaitState();
void doStandUpState();
void doSitDownState();
void doElongateState();
void doRetractState();

// Web callbacks
void onControlModeChange(const char* mode);
void onGeneralCommand(const char* command);

// --------------------------------------------------------------------
// Arduino setup()
// --------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  // while(!Serial); // Used for better seriel console operation

  Wire2.begin();
  delay(10);

  pwm.begin();
  pwm.setPWMFreq(400);

  // Init limbs
  for (int i=0; i<numMotors; i++){
    limbs[i].init();
    motorTasks[i].active = false;
    motorTasks[i].endMs  = 0;
  }

  // Init Web Server
  const char* ssid     = "PortentaRobot";   // SSID
  const char* password = "portentaconnect"; // Password
  webServer.setControlModeCallback(onControlModeChange);
  webServer.setCommandCallback(onGeneralCommand);
  webServer.begin(ssid, password);
  Serial.println("Web Server initialized. Connect to Wi-Fi AP to control the robot.");

  bodyControl.init();
  gaitControl.init();

  Serial.println("System initialized (Wire2 + PCA9685 @ 0x40).");
  displayMessage("System Initialized");

  mainState = STATE_IDLE;
}

// --------------------------------------------------------------------
// Arduino loop()
// --------------------------------------------------------------------
void loop(){
  // Web requests
  webServer.handleClient();

  // Serial
  serialEvent();
  if (stringComplete) {
    processCommand(inputString);
    inputString    = "";
    stringComplete = false;
  }

  // FSM
  switch(mainState) {
    case STATE_IDLE:        doIdleState();        break;
    case STATE_INDIVIDUAL:  doIndividualState();  break;
    // case STATE_BODY:        /* Prepared for body-specific logic*/ break;
    case STATE_GAIT:        doGaitState();        break;
    case STATE_STAND_UP:    doStandUpState();     break;
    case STATE_SIT_DOWN:    doSitDownState();     break;
    case STATE_ELONGATE:    doElongateState();    break;
    case STATE_RETRACT:     doRetractState();     break;
  }

  unsigned long now = millis();
  for (int i=0; i<numMotors; i++){
    if (motorTasks[i].active && now >= motorTasks[i].endMs) {
      limbs[i].stopMotor();
      motorTasks[i].active = false;
      Serial.print("[Timer] Motor ");
      Serial.print(i);
      Serial.println(" auto-stopped");
    }
  }
}

// --------------------------------------------------------------------
// Callback: onControlModeChange
// --------------------------------------------------------------------
void onControlModeChange(const char* mode) {
  Serial.print("[WebServer] Control mode changed to: ");
  Serial.println(mode);

  if (strcmp(mode, "INDIVIDUAL") == 0) {
    setState(STATE_INDIVIDUAL);
    Serial.println("Changed to individual mode from AP control");
  } 
  else if (strcmp(mode, "GAIT") == 0) {
    setState(STATE_GAIT);
    gaitControl.setState(GaitControl::STOP_STATE);
    Serial.println("Changed to gait mode from AP control");
  }
  else {
    Serial.println("[onControlModeChange] Unknown mode!");
  }
}

// --------------------------------------------------------------------
// Callback: onGeneralCommand
// --------------------------------------------------------------------
void onGeneralCommand(const char* command) {
  Serial.print("[WebServer] Received general command: ");
  Serial.println(command);

  String cmdStr = String(command);
  Serial.println("Before processCommand");
  processCommand(cmdStr);
  Serial.println("Finished in [onGeneralCommand]");
}

// --------------------------------------------------------------------
// setState(newState): Switch FSM
// --------------------------------------------------------------------
void setState(MainState newState){
  switch(mainState){
    case STATE_STAND_UP:   standUpStarted  = false; break;
    case STATE_SIT_DOWN:   sitDownStarted  = false; break;
    case STATE_ELONGATE:   elongateStarted = false; break;
    case STATE_RETRACT:    retractStarted  = false; break;
    default: break;
  }
  mainState = newState;

  switch(mainState){
    case STATE_IDLE:        Serial.println("[FSM] => STATE_IDLE"); break;
    case STATE_INDIVIDUAL:  Serial.println("[FSM] => STATE_INDIVIDUAL"); break;
    case STATE_BODY:        Serial.println("[FSM] => STATE_BODY"); break;
    case STATE_GAIT:        Serial.println("[FSM] => STATE_GAIT"); break;
    case STATE_STAND_UP:
      Serial.println("[FSM] => STATE_STAND_UP");
      standUpStarted = false;
      break;
    case STATE_SIT_DOWN:
      Serial.println("[FSM] => STATE_SIT_DOWN");
      sitDownStarted = false;
      break;
    case STATE_ELONGATE:
      Serial.println("[FSM] => STATE_ELONGATE");
      elongateStarted = false;
      break;
    case STATE_RETRACT:
      Serial.println("[FSM] => STATE_RETRACT");
      retractStarted = false;
      break;
  }
}

// --------------------------------------------------------------------
// Idle
// --------------------------------------------------------------------
void doIdleState() {
  // No logic yet
}

// --------------------------------------------------------------------
// Individual
// --------------------------------------------------------------------
void doIndividualState() {
  // no special repeated logic
}

// --------------------------------------------------------------------
// Gait
// --------------------------------------------------------------------
void doGaitState(){
  gaitControl.update();
}

// --------------------------------------------------------------------
// Stand Up
// --------------------------------------------------------------------
void doStandUpState(){
  if (!standUpStarted) {
    Serial.println("[StandUp] Commanding M2,M4,M6,M8 => CW for example");
    int dur = 4250;
    // Using M2, M4, M6, M8 (all lower motors on limbs)
    controlMotor(1, pulseMin, dur); // M2
    controlMotor(3, pulseMin, dur); // M4
    controlMotor(5, pulseMin, dur); // M6
    controlMotor(7, pulseMin, dur); // M8
    standUpStarted = true;
  }
  bool anyActive = false;
  for (int idx : {1,3,5,7}) {
    if (motorTasks[idx].active) { anyActive = true; break; }
  }
  if (!anyActive) {
    Serial.println("[StandUp] Done => STATE_IDLE");
    setState(STATE_IDLE);
  }
}

// --------------------------------------------------------------------
// Sit Down
// --------------------------------------------------------------------
void doSitDownState(){
  if (!sitDownStarted) {
    Serial.println("[SitDown] Commanding M2,M4,M6,M8 => CCW");
    int dur = 3500;
    controlMotor(1, pulseMax, dur);
    controlMotor(3, pulseMax, dur);
    controlMotor(5, pulseMax, dur);
    controlMotor(7, pulseMax, dur);
    sitDownStarted = true;
  }
  bool anyActive = false;
  for (int idx : {1,3,5,7}) {
    if (motorTasks[idx].active) { anyActive = true; break; }
  }
  if (!anyActive) {
    Serial.println("[SitDown] Done => STATE_IDLE");
    setState(STATE_IDLE);
  }
}

// --------------------------------------------------------------------
// Elongate
// --------------------------------------------------------------------
void doElongateState(){
  if (!elongateStarted){
    Serial.println("[Elongate] partial moves on M1 & M2");
    elongateStarted = true;
  }
  static int stepCount=0;
  static unsigned long lastMs=0;
  unsigned long now=millis();
  if(now - lastMs>500){
    controlMotor(0, pulseMin, 1000);
    controlMotor(1, pulseMin, 1000);
    stepCount++;
    lastMs = now;
    if(stepCount >= pulsesForFullMov){
      Serial.println("[Elongate] Done => STATE_IDLE");
      stepCount=0;
      setState(STATE_IDLE);
    }
  }
}

// --------------------------------------------------------------------
// Retract
// --------------------------------------------------------------------
void doRetractState(){
  if(!retractStarted){
    Serial.println("[Retract] partial moves on M1 & M2");
    retractStarted=true;
  }
  static int stepCount=0;
  static unsigned long lastMs=0;
  unsigned long now=millis();
  if(now - lastMs>500){
    controlMotor(0, pulseMax, 1000);
    controlMotor(1, pulseMax, 1000);
    stepCount++;
    lastMs=now;
    if(stepCount>=pulsesForFullMov){
      Serial.println("[Retract] Done => STATE_IDLE");
      stepCount=0;
      setState(STATE_IDLE);
    }
  }
}

// --------------------------------------------------------------------
// serial Event
// --------------------------------------------------------------------
void serialEvent(){
  while(Serial.available()){
    char inChar=(char)Serial.read();
    if(inChar=='\n'){
      stringComplete=true;
      break;
    }
    inputString+=inChar;
  }
}

// --------------------------------------------------------------------
// process Command
// --------------------------------------------------------------------
void processCommand(String cmd){
  cmd.trim();
  Serial.print("[processCommand] ");
  Serial.println(cmd);

  // optional parse :duration
  int colonIndex=cmd.indexOf(':');
  String baseCmd=(colonIndex!=-1) ? cmd.substring(0,colonIndex) : cmd;
  String param  =(colonIndex!=-1) ? cmd.substring(colonIndex+1) : "";
  int duration=200;
  if(colonIndex!=-1){
    int dur=param.toInt();
    if(dur>0) duration=dur;
  }

  if(cmd.startsWith("SET_MODE:INDIVIDUAL")){
    Serial.println("[Cmd] => STATE_INDIVIDUAL");
    setState(STATE_INDIVIDUAL);
  }
  else if(cmd.startsWith("SET_MODE:BODY")){
    Serial.println("[Cmd] => STATE_BODY");
    setState(STATE_BODY);
  }
  else if(cmd.startsWith("SET_MODE:GAIT")){
    Serial.println("[Cmd] => STATE_GAIT");
    gaitControl.setState(GaitControl::STOP_STATE);
    setState(STATE_GAIT);
  }

  // StandUp / SitDown
  else if(cmd.startsWith("STAND_UP")){
    Serial.println("[Cmd] => STATE_STAND_UP");
    setState(STATE_STAND_UP);
  }
  else if(cmd.startsWith("SIT_DOWN")){
    Serial.println("[Cmd] => STATE_SIT_DOWN");
    setState(STATE_SIT_DOWN);
  }
  else if(cmd.startsWith("ELONGATE")){
    Serial.println("[Cmd] => STATE_ELONGATE");
    setState(STATE_ELONGATE);
  }
  else if(cmd.startsWith("RETRACT")){
    Serial.println("[Cmd] => STATE_RETRACT");
    setState(STATE_RETRACT);
  }

  else if(cmd.startsWith("START_CRAWLING")){
    if(mainState==STATE_GAIT){
      gaitControl.setState(GaitControl::CRAWLING_STATE);
      Serial.println("[Cmd] Gait => CRAWLING");
    } else {
      Serial.println("[ERROR] Must be in STATE_GAIT for crawling");
    }
  }
  else if(cmd.startsWith("START_WALKING")){
    if(mainState==STATE_GAIT){
      gaitControl.setState(GaitControl::WALKING_STATE);
      Serial.println("[Cmd] Gait => WALKING");
    } else {
      Serial.println("[ERROR] Must be in STATE_GAIT for walking");
    }
  }
  else if(cmd.startsWith("STOP_GAIT")){
    if(mainState==STATE_GAIT){
      gaitControl.setState(GaitControl::STOP_STATE);
      Serial.println("[Cmd] Gait => STOPPED");
    } else {
      Serial.println("[ERROR] Not in GAIT state");
    }
  }

  else if(cmd.startsWith("START_FASTCRAWL")){
    if(mainState==STATE_GAIT){
      gaitControl.setState(GaitControl::FASTCRAWL_STATE);
      Serial.println("[Cmd] Gait => FASTCRAWL");
    } else {
      Serial.println("[ERROR] Must be in STATE_GAIT for FASTCRAWL");
    }
  }

  // INDIVIDUAL commands
  else if(cmd.startsWith("ROTATE_") || cmd.startsWith("STOP_")){
    if (cmd.startsWith("ROTATE_M1_CW")) {
      controlMotor(0, pulseMin, duration);
    }
    else if (cmd.startsWith("ROTATE_M1_CCW")) {
      controlMotor(0, pulseMax, duration);
    }
    else if (cmd.startsWith("ROTATE_M2_CW")) {
      controlMotor(1, pulseMin, duration);
    }
    else if (cmd.startsWith("ROTATE_M2_CCW")) {
      controlMotor(1, pulseMax, duration);
    }
    else if (cmd.startsWith("ROTATE_M3_CW")) {
      controlMotor(2, pulseMin, duration);
    }
    else if (cmd.startsWith("ROTATE_M3_CCW")) {
      controlMotor(2, pulseMax, duration);
    }
    else if (cmd.startsWith("ROTATE_M4_CW")) {
      controlMotor(3, pulseMin, duration);
    }
    else if (cmd.startsWith("ROTATE_M4_CCW")) {
      controlMotor(3, pulseMax, duration);
    }
    else if (cmd.startsWith("ROTATE_M5_CW")) {
      controlMotor(4, pulseMin, duration);
    }
    else if (cmd.startsWith("ROTATE_M5_CCW")) {
      controlMotor(4, pulseMax, duration);
    }
    else if (cmd.startsWith("ROTATE_M6_CW")) {
      controlMotor(5, pulseMin, duration);
    }
    else if (cmd.startsWith("ROTATE_M6_CCW")) {
      controlMotor(5, pulseMax, duration);
    }
    else if (cmd.startsWith("ROTATE_M7_CW")) {
      controlMotor(6, pulseMin, duration);
    }
    else if (cmd.startsWith("ROTATE_M7_CCW")) {
      controlMotor(6, pulseMax, duration);
    }
    else if (cmd.startsWith("ROTATE_M8_CW")) {
      controlMotor(7, pulseMin, duration);
    }
    else if (cmd.startsWith("ROTATE_M8_CCW")) {
      controlMotor(7, pulseMax, duration);
    }

    else if (cmd.startsWith("STOP_M1_M2_MOTORS")) {
      stopMotor(0);
      stopMotor(1);
    }
    else if (cmd.startsWith("STOP_M3_M4_MOTORS")) {
      stopMotor(2);
      stopMotor(3);
    }
    else if (cmd.startsWith("STOP_M5_M6_MOTORS")) {
      stopMotor(4);
      stopMotor(5);
    }
    else if (cmd.startsWith("STOP_M7_M8_MOTORS")) {
      stopMotor(6);
      stopMotor(7);
    }
    else if(cmd.startsWith("STOP_MOTORS")){
      stopMotors();
    }
  }

  else if (cmd.startsWith("ROTATE_BODY1_CW")) {
    controlMotor(8, pulseMin, duration); 
  }
  else if (cmd.startsWith("ROTATE_BODY1_CCW")) {
      controlMotor(8, pulseMax, duration); 
  }
  else if (cmd.startsWith("STOP_BODY1")) {
      stopMotor(8);
  }

  else if (cmd.startsWith("ROTATE_BODY2_CW")) {
      controlMotor(9, pulseMin, duration);
  }
  else if (cmd.startsWith("ROTATE_BODY2_CCW")) {
      controlMotor(9, pulseMax, duration);
  }
  else if (cmd.startsWith("STOP_BODY2")) {
      stopMotor(9);
  }


  // Calibration
  else if(baseCmd == "CALIBRATE_LIMB"){
    int limbIndex = param.toInt()-1; 
    calibration.calibrateLimb(limbIndex);
  }
  else if(baseCmd == "CALIBRATE_ALL_LIMBS"){
    calibration.calibrateAllLimbs();
  }

  else {
    Serial.println("[Cmd] Unknown or unhandled command");
  }
}

// --------------------------------------------------------------------
// Motor control
// --------------------------------------------------------------------
void controlMotor(int motorIndex, uint16_t pulse, int duration){
  if(motorIndex<0 || motorIndex>=numMotors){
    Serial.println("[ERROR] Invalid motor index!");
    return;
  }
  limbs[motorIndex].setPulse(pulse);
  motorTasks[motorIndex].active = true;
  motorTasks[motorIndex].endMs  = millis() + duration;
}

void stopMotor(int motorIndex){
  if(motorIndex<0 || motorIndex>=numMotors){
    Serial.println("[ERROR] Invalid motor index!");
    return;
  }
  limbs[motorIndex].stopMotor();
  motorTasks[motorIndex].active = false;
  Serial.print("[stopMotor] Motor ");
  Serial.print(motorIndex);
  Serial.println(" manually stopped");
}

void stopMotors(){
  Serial.println("[stopMotors] Stopping all");
  for(int i=0; i<numMotors; i++){
    limbs[i].stopMotor();
    motorTasks[i].active=false;
  }
}

void fullyElongate(){
  for(int i=0; i<pulsesForFullMov; i++){
    controlMotor(0, pulseMin, runTime);
    controlMotor(1, pulseMin, runTime);
    delay(runTime/10);
  }
  displayMessage("Fully Elongated");
}

void fullyRetract(){
  for(int i=0; i<pulsesForFullMov; i++){
    controlMotor(0, pulseMax, runTime);
    controlMotor(1, pulseMax, runTime);
    delay(runTime/10);
  }
  displayMessage("Fully Retracted");
}

void displayMessage(const char* msg){
  Serial.println(msg);
}

void motorSweepTest(int motorIndex){
  Serial.println("[motorSweepTest] begin...");
  for(int pulse=1450; pulse>=500; pulse-=100){
    limbs[motorIndex].setPulse(pulse);
    delay(1000);
  }
  limbs[motorIndex].stopMotor();
  delay(1000);
  for(int pulse=1550; pulse<=2500; pulse+=100){
    limbs[motorIndex].setPulse(pulse);
    delay(1000);
  }
  limbs[motorIndex].stopMotor();
  Serial.println("[motorSweepTest] done.");
}