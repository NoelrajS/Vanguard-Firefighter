#include <Wire.h>
#include <MPU6050.h>
#include <Adafruit_BMP280.h>
#include <DHT.h>

// Pins
#define TRIG_LEFT D2
#define ECHO_LEFT D3
#define TRIG_RIGHT D4
#define ECHO_RIGHT D5
#define DHT_PIN D6
#define MIC_PIN A0
#define DHTTYPE DHT11

#define STEP_STRIDE_M 0.70

MPU6050 mpu;
Adafruit_BMP280 bmp;
DHT dht(DHT_PIN, DHTTYPE);

// Orientation & Mapping Variables
float currentHeading = 0.0;
unsigned long lastGyroTime = 0;
float gyroBiasZ = 0.0;
float posX = 0.0, posY = 0.0, posZ = 0.0;
float baselineAltitude = 0.0;
int stepCount = 0;
unsigned long lastStepTime = 0;

// Acoustic Thresholds
const int ROCK_CRACK_THRESHOLD = 2800;
const int CIVILIAN_SHOUT_MIN = 1200;
int shoutCount = 0;

float getDistanceCM(int trigPin, int echoPin) {
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    long duration = pulseIn(echoPin, HIGH, 25000);
    if (duration == 0) return 400.0;
    return (duration * 0.0343) / 2.0;
}

void setup() {
    Serial.begin(115200);
    Wire.begin();
    
    pinMode(TRIG_LEFT, OUTPUT);
    pinMode(ECHO_LEFT, INPUT);
    pinMode(TRIG_RIGHT, OUTPUT);
    pinMode(ECHO_RIGHT, INPUT);
    pinMode(MIC_PIN, INPUT);
    
    dht.begin();
    mpu.initialize();
    
    if (!bmp.begin(0x76) && !bmp.begin(0x77)) {
        Serial.println("BMP280 init failed");
    }
    
    Wire.beginTransmission(0x68);
    Wire.write(0x6B);
    Wire.write(0);
    Wire.endTransmission(true);
    
    long gyroAccumulatorZ = 0;
    for (int i = 0; i < 500; i++) {
        gyroAccumulatorZ += mpu.getRotationZ();
        delay(2);
    }
    gyroBiasZ = (float)gyroAccumulatorZ / 500.0;
    baselineAltitude = bmp.readAltitude(1013.25);
    lastGyroTime = micros();
}

void loop() {
    unsigned long currentMicros = micros();

    // 1. Gyro integration for Heading
    int16_t rawGyroZ = mpu.getRotationZ();
    float correctedGyroZ = (float)rawGyroZ - gyroBiasZ;
    float gyroZ_dps = correctedGyroZ / 131.0;
    float dt = (currentMicros - lastGyroTime) / 1000000.0;
    lastGyroTime = currentMicros;

    if (abs(gyroZ_dps) > 1.2) {
        currentHeading += gyroZ_dps * dt;
        if (currentHeading >= 360.0) currentHeading -= 360.0;
        if (currentHeading < 0.0) currentHeading += 360.0;
    }

    // 2. Step Detection
    int16_t ax, ay, az;
    mpu.getAcceleration(&ax, &ay, &az);
    float accG = sqrt((long)ax * ax + (long)ay * ay + (long)az * az) / 16384.0;

    if (accG > 1.35 && (millis() - lastStepTime) > 350) {
        stepCount++;
        lastStepTime = millis();
        float rad = currentHeading * (3.14159 / 180.0);
        posX += STEP_STRIDE_M * cos(rad);
        posY += STEP_STRIDE_M * sin(rad);
    }

    // 3. Environmental & Distance Sensors
    posZ = bmp.readAltitude(1013.25) - baselineAltitude;
    float distLeft = getDistanceCM(TRIG_LEFT, ECHO_LEFT);
    float distRight = getDistanceCM(TRIG_RIGHT, ECHO_RIGHT);
    float temp = dht.readTemperature();

    // 4. HIGH-SPEED ACOUSTIC SAMPLING
    unsigned long startAcoustic = millis();
    unsigned int signalMax = 0;
    unsigned int signalMin = 4095;

    while (millis() - startAcoustic < 50) {
        int sample = analogRead(MIC_PIN);
        if (sample < 4095) {
            if (sample > signalMax) signalMax = sample;
            if (sample < signalMin) signalMin = sample;
        }
    }

    unsigned int peakToPeak = signalMax - signalMin;

    String acoustic_tag = "NOMINAL";
    if (peakToPeak >= ROCK_CRACK_THRESHOLD) {
        acoustic_tag = "SHOCK_IMPACT";
    } else if (peakToPeak >= CIVILIAN_SHOUT_MIN) {
        acoustic_tag = "DISTRESS_VOICE";
    }

    // 5. Stream JSON packet to Python
    Serial.print("{\"x\":");
    Serial.print(posX, 2);
    Serial.print(",\"y\":");
    Serial.print(posY, 2);
    Serial.print(",\"z\":");
    Serial.print(posZ, 2);
    Serial.print(",\"heading\":");
    Serial.print(currentHeading, 1);
    Serial.print(",\"steps\":");
    Serial.print(stepCount);
    Serial.print(",\"dist_l\":");
    Serial.print(distLeft, 1);
    Serial.print(",\"dist_r\":");
    Serial.print(distRight, 1);
    Serial.print(",\"temp\":");
    Serial.print(isnan(temp) ? 0 : temp, 1);
    Serial.print(",\"mic_vol\":");
    Serial.print(peakToPeak);
    Serial.print(",\"acoustic_state\":\"");
    Serial.print(acoustic_tag);
    Serial.println("\"}");
}