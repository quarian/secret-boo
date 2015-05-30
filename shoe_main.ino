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

#define STATE_SETUP        19
#define STATE_INITIAL      20
#define STATE_INITIAL_TIGHTENING  25
#define STATE_TIGHTENING   21
#define STATE_CLOSED       22
#define STATE_LOOSENING    23
#define STATE_CONTROL      24
#define STATE_RESETTING    26

#define TIGHTEN            1
#define LOOSEN             0

#define HEEL_SENSOR        2
#define TONGUE_SENSOR      1
#define TOE_SENSOR         0

#define MOTOR_MAX          255
#define CURRENT_MAX        70  // 90 for 25 RPM motor, 70 for 30 RPM motor
#define FORCE_SENSOR_FILTER  350

#define GESTURE_DURATION   500
#define RESET_TIME         3000
#define CONTROL_STATE_MAX_TIME   15000

// How many NeoPixels are attached to the Arduino?
#define NUMPIXELS      1

#define PRODUCTION     0  // Change to 1 when flashing production shoes.
                          // Changes the light behaviour such that only
                          // shows light in configuration mode

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
int hallSensorBaseValue = 0; // Will be set up for each shoe individually
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
boolean controlStateFlipped = false;
unsigned long controlTime = 0;

int STATE = STATE_SETUP;

void setup() {
  pixels.begin(); // This initializes the NeoPixel library.
  pinMode(SWITCH_PIN, INPUT);
  pinMode(MOTOR_A, OUTPUT);
  pinMode(MOTOR_B, OUTPUT);
  pinMode(MOTOR_EN, OUTPUT);
  
  digitalWrite(MOTOR_A, HIGH);
  digitalWrite(MOTOR_B, LOW);
  
  Serial.begin(38400);
  
  setUpHallSensor();
  switchState(STATE, STATE_INITIAL);
}

void setUpHallSensor() {
  for (int i = 0; i < 500; i++) {
    hallSensorBaseValue += analogRead(HALL_SENSOR);
    if (i > 1)
      hallSensorBaseValue = hallSensorBaseValue / 2;
    delay(delayval);
    blinkLightSetup(i);
  }
  Serial.println("Hall sensor base value");
  Serial.println(hallSensorBaseValue);
}

void blinkLightSetup(int i) {
    if (!(i % 50))
      setColor(150, 0, 0);  
    if (!(i % 101))
      setColor(0, 0, 150);
    updateLight();
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
      manageMotor(LOOSEN, TONGUE_SENSOR); 
      break;
    case STATE_INITIAL_TIGHTENING:
      setColor(0, 150, 150);
      controlMotor(TIGHTEN, MOTOR_MAX);
      checkTopTightness();
      break;
    case STATE_TIGHTENING:
      setColor(0, 150, 150);
      manageMotor(TIGHTEN, HEEL_SENSOR);
      break;
    case STATE_CLOSED:
      setColor(255, 0, 255);
      checkHallSensor();
      break;
    case STATE_LOOSENING: 
      setColor(150, 0, 150);
      manageMotor(LOOSEN, TOE_SENSOR);
      break;
    case STATE_CONTROL:
      setColor(0, 0, 150);
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
  
  if(power > 0 && power <= MOTOR_MAX)
    analogWrite(MOTOR_EN, power);
  else
    analogWrite(MOTOR_EN, 0);
}

void stopMotor() {
    controlMotor(TIGHTEN, 0);  
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
  else {
    hallTime = 0;
    controlStateFlipped = false;  
  }
}

void compareHallValues() {
  hallTime = hallTime == 0 ? millis() : hallTime;
  if (millis() - hallTime > GESTURE_DURATION && !controlStateFlipped) {
    if (STATE == STATE_CLOSED) {
      switchState(STATE, STATE_CONTROL);
      controlTime = millis();
    }
    else
      switchState(STATE, STATE_CLOSED);
    hallTime = 0;
    controlStateFlipped = true;
  }
}

void manageControlState() {
  checkHallSensor();
  if (STATE != STATE_CONTROL)
    return;
    
  if (millis() - controlTime > CONTROL_STATE_MAX_TIME) {
    switchState(STATE, STATE_CLOSED);
    controlTime = 0;
    return;
  }
  
  if  (abs(forceSensorValues[HEEL_SENSOR] - forceSensorValues[TOE_SENSOR]) < FORCE_SENSOR_FILTER)
    return;
    
  if (forceSensorValues[HEEL_SENSOR] > heelForceThreshold) {
    switchState(STATE, STATE_TIGHTENING);
    return;
  }
  
  if (forceSensorValues[TOE_SENSOR] > toeForceThreshold) {
    switchState(STATE, STATE_LOOSENING);
    return;
  }
}

void manageMotor(int direction, int location) {
  int targetState = getTargetStateForMotor(location);
  if (targetState) {
    stopMotor();
    switchState(STATE, targetState);
  } else if (currentValue < CURRENT_MAX)
    controlMotor(direction, MOTOR_MAX); 
}

int getTargetStateForMotor(int location) {
  if ((checkSensorUnderThreshold(location, HEEL_SENSOR, heelForceThreshold) ||
      checkSensorUnderThreshold(location, TOE_SENSOR, toeForceThreshold)) &&
      abs(forceSensorValues[HEEL_SENSOR] - forceSensorValues[TOE_SENSOR]) < FORCE_SENSOR_FILTER)
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
  if (millis() - resetInitialTime > RESET_TIME) {
    stopMotor();
    switchState(STATE, STATE_INITIAL);
    return;
  }
  manageMotor(LOOSEN, MOTOR_MAX);
}

void checkTopTightness() {
  if (forceSensorValues[TONGUE_SENSOR] > tongueForceThreshold || currentValue > CURRENT_MAX) {
    switchState(STATE, STATE_CLOSED);
    stopMotor();
   }
}

void setColor(int red, int green, int blue) {
  if (!PRODUCTION || STATE == STATE_CONTROL || STATE == STATE_SETUP ||
      STATE == STATE_LOOSENING || STATE == STATE_TIGHTENING) {
    RED = red;
    GREEN = green;
    BLUE = blue;
  } else {
    RED = 0;
    GREEN = 0;
    BLUE = 0;  
  }
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
