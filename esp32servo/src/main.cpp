#include <Arduino.h>
#include "I2Cdev.h"
#include "MPU6050_6Axis_MotionApps20.h"
#include "Wire.h"
#include "ESP32Servo.h"

// 3-axis servo
#define SERVO_YAW_PIN 32
#define SERVO_PITCH_PIN 19
#define SERVO_ROLL_PIN 25
Servo servoYaw;
Servo servoPitch;
Servo servoRoll;
// Published values for SG90 servos; adjust if needed
int minUs = 1000;
int maxUs = 2000;
int pos = 0;      // position in degrees
ESP32PWM pwm;

// MPU6050 Accel/Gyro
// #define OUTPUT_READABLE_QUATERNION
// #define OUTPUT_READABLE_REALACCEL
// #define OUTPUT_READABLE_WORLDACCEL
#define OUTPUT_READABLE_YAWPITCHROLL
#define INTERRUPT_PIN 34
#define SDA_PIN 21
#define SCL_PIN 22
// #define M_PI 3.14159265358979323846

MPU6050 mpu;
bool dmpReady = false;  // set true if DMP init was successful
bool outputAccel = true;
uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU
uint8_t devStatus;      // return status after each device operation (0 = success, !0 = error)
uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
uint16_t fifoCount;     // count of all bytes currently in FIFO
uint8_t fifoBuffer[64]; // FIFO storage buffer
// orientation/motion vars
Quaternion q;           // [w, x, y, z]         quaternion container
VectorInt16 aa;         // [x, y, z]            accel sensor measurements
VectorInt16 aaReal;     // [x, y, z]            gravity-free accel sensor measurements
VectorInt16 aaWorld;    // [x, y, z]            world-frame accel sensor measurements
VectorFloat gravity;    // [x, y, z]            gravity vector
float euler[3];         // [psi, theta, phi]    Euler angle container
float ypr[3];           // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector

// ================================================================
// ===               INTERRUPT DETECTION ROUTINE                ===
// ================================================================

volatile bool mpuInterrupt = false;     // indicates whether MPU interrupt pin has gone high
void dmpDataReady() {
    mpuInterrupt = true;
}


// UART communication with ESP32-CAM
#define RX_PIN 14
#define TX_PIN 13
bool receivedData    = false;
const int numChars = 40;
char data[numChars];
int xVal  = 0;
int yVal  = 0;
int i     = 0;


/**
 * Taken from the ESP32Servo multiple servo example 
 */
void servoSetup() {
	// Allow allocation of all timers
	ESP32PWM::allocateTimer(0);
	ESP32PWM::allocateTimer(1);
	ESP32PWM::allocateTimer(2);
	Serial.begin(115200);
	servoYaw.setPeriodHertz(50);        // Standard 50hz servo
	servoPitch.setPeriodHertz(50);      // Standard 50hz servo
	servoRoll.setPeriodHertz(50);       // Standard 50hz servo

  servoYaw.attach(SERVO_YAW_PIN, minUs, maxUs);
  servoPitch.attach(SERVO_PITCH_PIN, minUs, maxUs);
  servoRoll.attach(SERVO_ROLL_PIN, minUs, maxUs);

  servoYaw.write(0);
  servoPitch.write(0);
  servoRoll.write(0);
}

void servoDestroy() {
  servoYaw.detach();
  servoPitch.detach();
  servoRoll.detach();
}

/**
 * Correct for the roll of the camera, using input from the gyro
 */
void maintainHorizontal() {
  int roll = 180-(ypr[2] * 180/M_PI);
  Serial.print("Roll:");
  Serial.println(roll);
  servoYaw.write(roll);
}

/**
 * Move servos according to received command from user
 */
void setServos() {
  int yaw   = map(xVal,-255,255,0,180);
  int pitch = map(yVal,-255,255,0,180);
  servoPitch.write(yaw);
  servoRoll.write(pitch);
}

/**
 * Taken from the I2CDevLib example for the MPU6050
 */
void gyroSetup() {
  // join I2C bus (I2Cdev library doesn't do this automatically)
  #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
      Wire.begin();
      Wire.setClock(400000); // 400kHz I2C clock. Comment this line if having compilation difficulties
  #elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
      Fastwire::setup(400, true);
  #endif

  // initialize device
  Serial.println(F("Initializing I2C devices..."));
  mpu.initialize();
  pinMode(INTERRUPT_PIN, INPUT);

  // verify connection
  Serial.println(F("Testing device connections..."));
  Serial.println(mpu.testConnection() ? F("MPU6050 connection successful") : F("MPU6050 connection failed"));

  // load and configure the DMP
  Serial.println(F("Initializing DMP..."));
  devStatus = mpu.dmpInitialize();

  // supply your own gyro offsets here, scaled for min sensitivity
  mpu.setXGyroOffset(-45);
  mpu.setYGyroOffset(-96);
  mpu.setZGyroOffset(-4);
  mpu.setXAccelOffset(-2833);
  mpu.setYAccelOffset(2196);
  mpu.setZAccelOffset(1009);


    // make sure it worked (returns 0 if so)
    if (devStatus == 0) {
        // Calibration Time: generate offsets and calibrate our MPU6050
        mpu.CalibrateAccel(6);
        mpu.CalibrateGyro(6);
        mpu.PrintActiveOffsets();
        // turn on the DMP, now that it's ready
        Serial.println(F("Enabling DMP..."));
        mpu.setDMPEnabled(true);

        // enable Arduino interrupt detection
        Serial.print(F("Enabling interrupt detection (Arduino external interrupt "));
        Serial.print(digitalPinToInterrupt(INTERRUPT_PIN));
        Serial.println(F(")..."));
        attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), dmpDataReady, RISING);
        mpuIntStatus = mpu.getIntStatus();

        // set our DMP Ready flag so the main loop() function knows it's okay to use it
        Serial.println(F("DMP ready! Waiting for first interrupt..."));
        dmpReady = true;

        // get expected DMP packet size for later comparison
        packetSize = mpu.dmpGetFIFOPacketSize();
    } else {
        // ERROR!
        // 1 = initial memory load failed
        // 2 = DMP configuration updates failed
        // (if it's going to break, usually the code will be 1)
        Serial.print(F("DMP Initialization failed (code "));
        Serial.print(devStatus);
        Serial.println(F(")"));
    }

}

/**
 * Taken from the I2CDevLib example for the MPU6050
 */
void gyroRead() {
  // if programming failed, don't try to do anything
  if (!dmpReady) return;
  // read a packet from FIFO
  if (mpu.dmpGetCurrentFIFOPacket(fifoBuffer)) { // Get the Latest packet 
      #ifdef OUTPUT_READABLE_QUATERNION
          // display quaternion values in easy matrix form: w x y z
          mpu.dmpGetQuaternion(&q, fifoBuffer);
          Serial.print("quat\t");
          Serial.print(q.w);
          Serial.print("\t");
          Serial.print(q.x);
          Serial.print("\t");
          Serial.print(q.y);
          Serial.print("\t");
          Serial.println(q.z);
      #endif

      #ifdef OUTPUT_READABLE_EULER
          // display Euler angles in degrees
          mpu.dmpGetQuaternion(&q, fifoBuffer);
          mpu.dmpGetEuler(euler, &q);
          Serial.print("euler\t");
          Serial.print(euler[0] * 180/M_PI);
          Serial.print("\t");
          Serial.print(euler[1] * 180/M_PI);
          Serial.print("\t");
          Serial.println(euler[2] * 180/M_PI);
      #endif

      #ifdef OUTPUT_READABLE_YAWPITCHROLL
          // display Euler angles in degrees
          mpu.dmpGetQuaternion(&q, fifoBuffer);
          mpu.dmpGetGravity(&gravity, &q);
          mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
          if (outputAccel) {
            Serial.print("ypr\t");
            Serial.print(ypr[0] * 180/M_PI);
            Serial.print("\t");
            Serial.print(ypr[1] * 180/M_PI);
            Serial.print("\t");
            Serial.println(ypr[2] * 180/M_PI);
          }
      #endif

      #ifdef OUTPUT_READABLE_REALACCEL
          // display real acceleration, adjusted to remove gravity
          mpu.dmpGetQuaternion(&q, fifoBuffer);
          mpu.dmpGetAccel(&aa, fifoBuffer);
          mpu.dmpGetGravity(&gravity, &q);
          mpu.dmpGetLinearAccel(&aaReal, &aa, &gravity);
          Serial.print("areal\t");
          Serial.print(aaReal.x);
          Serial.print("\t");
          Serial.print(aaReal.y);
          Serial.print("\t");
          Serial.println(aaReal.z);
      #endif

      #ifdef OUTPUT_READABLE_WORLDACCEL
          // display initial world-frame acceleration, adjusted to remove gravity
          // and rotated based on known orientation from quaternion
          mpu.dmpGetQuaternion(&q, fifoBuffer);
          mpu.dmpGetAccel(&aa, fifoBuffer);
          mpu.dmpGetGravity(&gravity, &q);
          mpu.dmpGetLinearAccel(&aaReal, &aa, &gravity);
          mpu.dmpGetLinearAccelInWorld(&aaWorld, &aaReal, &q);
          Serial.print("aworld\t");
          Serial.print(aaWorld.x);
          Serial.print("\t");
          Serial.print(aaWorld.y);
          Serial.print("\t");
          Serial.println(aaWorld.z);
      #endif
  
      #ifdef OUTPUT_TEAPOT
          // display quaternion values in InvenSense Teapot demo format:
          teapotPacket[2] = fifoBuffer[0];
          teapotPacket[3] = fifoBuffer[1];
          teapotPacket[4] = fifoBuffer[4];
          teapotPacket[5] = fifoBuffer[5];
          teapotPacket[6] = fifoBuffer[8];
          teapotPacket[7] = fifoBuffer[9];
          teapotPacket[8] = fifoBuffer[12];
          teapotPacket[9] = fifoBuffer[13];
          Serial.write(teapotPacket, 14);
          teapotPacket[11]++; // packetCount, loops at 0xFF on purpose
      #endif
  }
}

void parseData() {
  char *strtokIndx;
  char xValString[10] = {0};
  char yValString[10] = {0};
  strtokIndx = strtok(data,",");
  strcpy(xValString, strtokIndx);

  strtokIndx = strtok(NULL,",");
  strcpy(yValString, strtokIndx);

  xVal = atoi(xValString);
  yVal = atoi(yValString);
  Serial.printf("x:%03d,y%03d\n",xVal,yVal);
}

void readData() {
  byte rc = Serial2.read();
  if (rc != '\n') {
    data[i] = rc;
    i++;
    if (i >= numChars) {
      i = numChars - 1;
    }
  }
  else {
    data[i] = '\0';
    i = 0;
    receivedData = true;
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial2.begin(115200,SERIAL_8N1,RX_PIN,TX_PIN,false,0);
  Serial2.setTimeout(0);

  // Prepare accel/gyro library
  gyroSetup();
  servoSetup();
}

void loop() {
  // put your main code here, to run repeatedly:
  while (Serial2.available()) {
    readData();
  }

  if (receivedData) {
    Serial.print("Data: ");
    Serial.println(data);
    receivedData = false;
    parseData();
    setServos();
  }

  // Read gyro. Note that gyro is only accurate in Pitch and Roll, by using gravity as a reference axis
  // Yaw is calculated bast on rotations from an initial reference, and will drift over time
  // We are primarily interested in ROLL, assuming wall mount, to keep the camera horizontal
  gyroRead();
  maintainHorizontal();
}