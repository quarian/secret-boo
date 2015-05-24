#include <Adafruit_NeoPixel.h>
#include "settings.h"

// Which pin on the Arduino is connected to?
#define SWITCH_PIN         2
#define MOTOR_A            4
#define MOTOR_B            5
#define MOTOR_EN           6
#define LED_PIN            10

#define HALL_SENSOR        A0
#define FORCE_SENSOR_1     A2
#define FORCE_SENSOR_2     A3
#define FORCE_SENSOR_3     A4
#define CURRENT_SENSOR     A5

#define STATE_INITIAL      20
#define STATE_INITIAL_TIGHTENING  25
#define STATE_TIGHTENING   21
#define STATE_CLOSED       22
#define STATE_LOOSENING    23
#define STATE_CONTROL      24
#define STATE_RESETTING    26

#define CLOCKWISE          0
#define COUNTERCLOCKWISE   1

#define HEEL_SENSOR        0
#define TONGUE_SENSOR      1
#define TOE_SENSOR         2

#define MOTOR_MAX          255

// How many NeoPixels are attached to the Arduino?
#define NUMPIXELS      1

// When we setup the NeoPixel library, we tell it how many pixels, and which pin to use to send signals.
// Note that for older NeoPixel strips you might need to change the third parameter--see the strandtest
// example for more information on possible values.
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

// color channel variables

int RED = 0;
int GREEN = 0;
int BLUE = 0;

int delayval = 10; // common delay value for the system

// settings variables
int hallSensorThreshold = HALL_SENSOR_THRESHOLD;
int hallSensorBaseValue = HALL_SENSOR_BASE_VALUE;
int toeForceThreshold = TOE_THRESHOLD;
int heelForceThreshold = HEEL_THRESHOLD;
int tongueForceThreshold = TOP_THRESHOLD;

// variables to store sensor values for the main loop
int forceSensorValues[] = {0, 0, 0};
int hallSensorValue = 0;
int currentSensorValue = 0;
int switchValue = 0; // value == 1 when open, 0 when closed
int currentValue = 0;
unsigned long hallTime = 0;
boolean hallCompare = false;
unsigned long resetInitialTime = 0;

int STATE = STATE_INITIAL;

void setup() {
  pixels.begin(); // This initializes the NeoPixel library.
  pinMode(SWITCH_PIN, INPUT);
  pinMode(MOTOR_A, OUTPUT);
  pinMode(MOTOR_B, OUTPUT);
  pinMode(MOTOR_EN, OUTPUT);
  
  digitalWrite(MOTOR_A, HIGH);
  digitalWrite(MOTOR_B, LOW);
  
  Serial.begin(38400);
}

void loop() {
  updateLight();
  
  readSensors();
  
  debugPrint();
  
  handleStateMachine();
  
  delay(delayval); // Delay for a period of time (in milliseconds).
}

void updateLight() {
  pixels.setPixelColor(0, pixels.Color(RED, GREEN, BLUE));
  pixels.show(); // This sends the updated pixel color to the hardware.
}

void readSensors() {
  readHallSensor();
  // Make sure reading is not 0 (measurement made during PWM low phase
  readCurrentSensor();
  readForceSensors();
}

void handleStateMachine() { 
  checkHeelClosed();
  switch (STATE) {
    case STATE_INITIAL:
      setColor(0, 0, 150);
      manageMotor(COUNTERCLOCKWISE, TONGUE_SENSOR); 
      break;
    case STATE_INITIAL_TIGHTENING:
      setColor(0, 150, 150);
      controlMotor(CLOCKWISE, MOTOR_MAX);
      checkTopTightness();
      break;
    case STATE_TIGHTENING:
      setColor(150, 150, 0);
      manageMotor(CLOCKWISE, TOE_SENSOR);
      break;
    case STATE_CLOSED:
      setColor(0, 150, 0);
      checkHallSensor();
      break;
    case STATE_LOOSENING: 
      setColor(150, 0, 150);
      manageMotor(COUNTERCLOCKWISE, HEEL_SENSOR);
      break;
    case STATE_CONTROL:
      setColor(150, 0, 0);
      manageControlState();
      break;
    case STATE_RESETTING:
      setColor(255, 102, 0);
      manageReset();
    default:

      break;    
  }
}

void checkHeelClosed() {
  readHeelSwitch();
  if ((STATE == STATE_INITIAL || STATE == STATE_RESETTING) && !switchValue)
    switchState(STATE, STATE_INITIAL_TIGHTENING);
  else if (STATE != STATE_INITIAL && STATE != STATE_RESETTING && switchValue) {
    resetInitialTime = millis();
    switchState(STATE, STATE_RESETTING); 
  }
}

void controlMotor(boolean dir, int power)
{
  digitalWrite(MOTOR_A, dir);
  digitalWrite(MOTOR_B, !dir);
  
  if(power > 0 && power <= 255)
    analogWrite(MOTOR_EN, power);
  else
    analogWrite(MOTOR_EN, 0);
}

void stopMotor() {
    controlMotor(CLOCKWISE, 0);  
}

void readForceSensors() {
  forceSensorValues[0] = analogRead(FORCE_SENSOR_1);
  forceSensorValues[1] = analogRead(FORCE_SENSOR_2);
  forceSensorValues[2] = analogRead(FORCE_SENSOR_3);
}

void readHallSensor() {
  hallSensorValue = analogRead(HALL_SENSOR);
}

void checkHallSensor() {
  //Serial.println("Checking hall sensor");
  hallCompare = abs(hallSensorValue - hallSensorBaseValue) > hallSensorThreshold;
  if (hallCompare)
    compareHallValues();
  else
    hallTime = 0; 
}

void compareHallValues() {
  hallTime = hallTime == 0 ? millis() : hallTime;
  if (millis() - hallTime > 1000) {
    if (STATE == STATE_CLOSED)
      switchState(STATE, STATE_CONTROL);
    else
      switchState(STATE, STATE_CLOSED);
    hallTime = 0;
  }
}

void manageControlState() {
  checkHallSensor();
  if (STATE != STATE_CONTROL)
    return;
  
  if  (abs(forceSensorValues[HEEL_SENSOR] - forceSensorValues[TOE_SENSOR]) < 200)
    return;
    
  if (forceSensorValues[HEEL_SENSOR] > toeForceThreshold) {
    switchState(STATE, STATE_LOOSENING);
    return;
  }
  
  if (forceSensorValues[TOE_SENSOR] > heelForceThreshold) {
    switchState(STATE, STATE_TIGHTENING);
    return;
  }
}

void manageMotor(int direction, int location) {
  int targetState = getTargetStateForMotor(location);
  if (targetState) {
    stopMotor();
    switchState(STATE, targetState);
  } else if (currentValue < 90)
    controlMotor(direction, MOTOR_MAX); 
}

int getTargetStateForMotor(int location) {
  if (checkSensorUnderThreshold(location, HEEL_SENSOR, toeForceThreshold) ||
      checkSensorUnderThreshold(location, TOE_SENSOR, heelForceThreshold))
    return STATE_CONTROL;
  if (checkSensorUnderThreshold(location, TONGUE_SENSOR, tongueForceThreshold))
    return STATE_INITIAL;
  return 0;
}

boolean checkSensorUnderThreshold(int location, int sensor, int threshold) {
  return location == sensor && forceSensorValues[sensor] < threshold;
}

void readCurrentSensor() {
  for(int i = 0; i < 10; i++)
  {
    currentValue = analogRead(CURRENT_SENSOR);
    if(currentValue != 0)
      break;
  }
}

void readHeelSwitch() {
  switchValue = digitalRead(SWITCH_PIN);
}

void manageReset() {
  if (millis() - resetInitialTime > 3000) {
    stopMotor();
    switchState(STATE, STATE_INITIAL);
    return;
  }
  manageMotor(COUNTERCLOCKWISE, MOTOR_MAX);
}

void checkTopTightness() {
  if (forceSensorValues[TONGUE_SENSOR] > tongueForceThreshold) {
    switchState(STATE, STATE_CLOSED);
    stopMotor();
   }
}

void setColor(int red, int green, int blue) {
  RED = red;
  GREEN = green;
  BLUE = blue;  
}

void switchState(int oldState, int newState) {
  Serial.print  (" Switching state ");
  Serial.print("Old state ");
  Serial.print(oldState);
  Serial.print(" , new State");
  Serial.println(newState);
  STATE = newState;
}

void debugPrint() {
  Serial.print("State: ");
  Serial.print(getStateByNumber(STATE));
  Serial.print(" Hall: ");
  Serial.print(hallSensorValue);
  Serial.print(" Forces: ");
  Serial.print(forceSensorValues[0]);
  Serial.print(" ");
  Serial.print(forceSensorValues[1]);
  Serial.print(" ");
  Serial.print(forceSensorValues[2]);
  Serial.print(" Current: ");
  Serial.print(currentValue);
  Serial.print(" Switch: ");
  Serial.println(switchValue);
}

String getStateByNumber(int state) {
  switch (state) {
    case STATE_INITIAL:
      return "Initial_state";
    case STATE_INITIAL_TIGHTENING:
      return "Initial_tightening";
    case STATE_CLOSED:
      return "Closed";
    case STATE_LOOSENING:
      return "Loosening";
    case STATE_TIGHTENING:
      return "Tightening";
    case STATE_CONTROL:
      return "Control";
    case STATE_RESETTING:
      return "Resetting";
    default:  
      return "Should not be here"; 
  }  
  
}
