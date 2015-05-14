#include <Adafruit_NeoPixel.h>

// Which pin on the Arduino is connected to?
#define SWITCH_PIN         2
#define MOTOR_A            4
#define MOTOR_B            5
#define MOTOR_EN           6
#define LED_PIN            10

#define HALL_SENS          A0
#define FORCE_SENSOR_1     A2
#define FORCE_SENSOR_2     A3
#define FORCE_SENSOR_3     A4
#define CURRENT_SENSOR     A5

#define STATE_INITIAL      20
#define STATE_TIGHTENING   21
#define STATE_CLOSED       22
#define STATE_LOOSENING    23
#define STATE_CONTROL      24

#define CLOCKWISE          0
#define COUNTERCLOCKWISE   1

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
  
  // Hall sensor
  int hall = analogRead(HALL_SENS);
  
  // Make sure reading is not 0 (measurement made during PWM low phase
  int current = 0;
  for(int i = 0; i < 10; i++)
  {
    current = analogRead(CURRENT_SENSOR);
    if(current != 0)
      break;
  }
  
  // Force sensors
  int force1 = analogRead(FORCE_SENSOR_1);
  int force2 = analogRead(FORCE_SENSOR_2);
  int force3 = analogRead(FORCE_SENSOR_3);
  
  readForceSensors();
  
  // Switch
  int sw = digitalRead(SWITCH_PIN);
  
  // Chance motor direction
  controlMotor(sw, force2/6);
  
  // DEbug print
  Serial.print("Hall: ");
  Serial.print(hall);
  Serial.print(" Force 1: ");
  Serial.print(force1);
  Serial.print(" Force 2: ");
  Serial.print(force2);
  Serial.print(" Force 3: ");
  Serial.print(force3);
  Serial.print(" Current: ");
  Serial.print(current);
  Serial.print(" Switch: ");
  Serial.println(sw);
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

void readForceSensors() {
  int force1 = analogRead(FORCE_SENSOR_1);
  int force2 = analogRead(FORCE_SENSOR_2);
  int force3 = analogRead(FORCE_SENSOR_3);
  
  RED = force2 / 4;
  BLUE = force3 / 4;
}

void readHallSensor() {
    
}

void readHeelSwitch() {
  
}

void resetShoe() {

}

void tightenLaces() {
  
}

void loosenLaces() {
  
}
