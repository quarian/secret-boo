#include <Adafruit_NeoPixel.h>

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

#define CLOCKWISE          0
#define COUNTERCLOCKWISE   1

#define TOP_THRESHOLD      1000
#define HALL_THRESHOLD     10
#define HALL_BASE_VALUE    605 

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

// variables to store sensor values for the main loop
int forceSensorValues[] = {0, 0, 0};
int hallSensorValue = 0;
int currentSensorValue = 0;
int switchValue = 0; // value == 1 when open, 0 when closed
int currentValue = 0;
unsigned long hallTime = 0;
unsigned long hallTimeCompare = 0;
boolean hallCompare = false;

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
  pixels.setPixelColor(0, pixels.Color(RED, GREEN, BLUE));
  pixels.show(); // This sends the updated pixel color to the hardware.
  delay(delayval); // Delay for a period of time (in milliseconds).
  
  readHallSensor();
  
  // Make sure reading is not 0 (measurement made during PWM low phase
  readCurrentSensor();
  
  readForceSensors();
  
  debugPrint();
  
  // state machine handling
  
  checkHeelClosed();
  switch (STATE) {
    case STATE_INITIAL:
      break;
    case STATE_INITIAL_TIGHTENING:
      controlMotor(CLOCKWISE, 20);
      checkTopTightness();
      break;
    case STATE_TIGHTENING:
      manageMotor(CLOCKWISE, 2);
      break;
    case STATE_CLOSED:
      checkHallSensor();
      break;
    case STATE_LOOSENING:
      manageMotor(COUNTERCLOCKWISE, 1);
      break;
    case STATE_CONTROL:
      manageControlState();
      break;
    default:

      break;    
  }
}

void checkHeelClosed() {
  readHeelSwitch();
  if (STATE == STATE_INITIAL && !switchValue)
    switchState(STATE, STATE_INITIAL_TIGHTENING);
  else if (switchValue)
    resetShoe();
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
  
  GREEN = forceSensorValues[0] / 4;
  RED = forceSensorValues[1] / 4;
  BLUE = forceSensorValues[2] / 4;
}

void readHallSensor() {
  hallSensorValue = analogRead(HALL_SENSOR);
}

void checkHallSensor() {
  Serial.println("Checking hall sensor");
  hallCompare = abs(hallSensorValue - HALL_BASE_VALUE) > HALL_THRESHOLD;
  if (hallCompare) {
    Serial.println("Hall sensors close");
    hallTimeCompare = millis();
    if (hallTime == 0)
      hallTime = millis();
    Serial.println(hallTimeCompare - hallTime);
    if (hallTimeCompare - hallTime > 1000) {
      if (STATE == STATE_CLOSED)
        switchState(STATE, STATE_CONTROL);
      else
        switchState(STATE, STATE_CLOSED);
      hallTime = 0;
    }
  } else {
    hallTime = 0;  
  }
}

void manageControlState() {
  checkHallSensor();
  if (STATE != STATE_CONTROL)
    return;
  if (forceSensorValues[1] > 1000) {
    switchState(STATE, STATE_LOOSENING);
    return;
  }
  if (forceSensorValues[2] > 1000) {
    switchState(STATE, STATE_TIGHTENING);
    return;
  }
}

void manageMotor(int direction, int location) {
  if (forceSensorValues[location] < 1000) {
    switchState(STATE, STATE_CONTROL);
    stopMotor();
    return;  
  }
  controlMotor(direction, forceSensorValues[location]/50); 
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

void resetShoe() {
  // TODO: loosen the shoe to max loosnes
  if (STATE != STATE_INITIAL)
    switchState(STATE, STATE_INITIAL);
  stopMotor();
}

void tightenLaces() {
  
}

void checkTopTightness() {
  Serial.print("Checking top tightness, ");
  Serial.println(forceSensorValues[0]);
  if (forceSensorValues[0] > TOP_THRESHOLD) {
    switchState(STATE, STATE_CLOSED);
    stopMotor();
   }
}

void loosenLaces() {
  
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
      return "Initial state";
    case STATE_INITIAL_TIGHTENING:
      return "Initial tightening";
    case STATE_CLOSED:
      return "Closed";
    case STATE_LOOSENING:
      return "Loosening";
    case STATE_TIGHTENING:
      return "Tightening";
    case STATE_CONTROL:
      return "Control";
    default:  
      return "Should not be here"; 
  }  
  
}
