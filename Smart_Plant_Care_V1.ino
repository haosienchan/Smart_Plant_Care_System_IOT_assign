/*
=====================================================
RELAY MODULE NOTES (FL-3FF-S-Z)
=====================================================

Relay terminal labels (left to right):

常开  = Normally Open (NO)
公共端 = Common (COM)
常闭  = Normally Closed (NC)

Terminal order:
NO | COM | NC

Recommended pump wiring:
- External power positive -> COM
- NO -> Pump positive
- Pump negative -> External power negative
- Don't connect anything to NC

Relay control pins:
- IN   -> ESP32 GPIO 18
- GND  -> ESP32 GND
- VCC  -> 5V

IMPORTANT:
This relay module is LOW trigger:
- digitalWrite(RELAY_PIN, LOW)  = Relay ON  = Pump ON
- digitalWrite(RELAY_PIN, HIGH) = Relay OFF = Pump OFF

Use external 5V power for the pump.
Do NOT power the pump directly from ESP32.
=====================================================
*/

#include <DHT.h>
#include <ESP32Servo.h>
#include <Stepper.h>

// =====================================================
// PIN DEFINITIONS
// =====================================================

// Sensors
#define SOIL_PIN          34
#define LDR_PIN           35
#define WATER_LEVEL_PIN   32
#define DHT_PIN           4

// Relay (Water Pump)
#define RELAY_PIN         18

// Fan Driver (L9110)
#define FAN_IN1           19
#define FAN_IN2           21

// Servo Motor (Sunshade)
#define SERVO_PIN         13

// Stepper Motor (Ventilation Flap)
#define IN1               14
#define IN2               27
#define IN3               26
#define IN4               25

// =====================================================
// DHT11 SETUP
// =====================================================

#define DHTTYPE DHT11
DHT dht(DHT_PIN, DHTTYPE);

// =====================================================
// SERVO SETUP
// =====================================================

Servo shadeServo;

// =====================================================
// STEPPER SETUP
// =====================================================

const int stepsPerRevolution = 2048;

Stepper ventilationStepper(
  stepsPerRevolution,
  IN1, IN3, IN2, IN4
);

// =====================================================
// THRESHOLDS
// =====================================================

// Soil moisture
// Higher value = drier soil
int soilThreshold = 2500;

// Light threshold
int lightThreshold = 2000;

// Environmental thresholds
float tempThreshold = 30.0;
float humidityThreshold = 80.0;

// =====================================================
// STATE VARIABLES
// =====================================================

bool flapOpen = false;
bool shadeClosed = false;

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  Serial.println("Smart Plant Care System Starting...");

  // DHT11
  dht.begin();

  // Relay
  pinMode(RELAY_PIN, OUTPUT);

  // Relay OFF initially
  digitalWrite(RELAY_PIN, HIGH);

  // Fan driver
  pinMode(FAN_IN1, OUTPUT);
  pinMode(FAN_IN2, OUTPUT);

  // Fan OFF initially
  digitalWrite(FAN_IN1, LOW);
  digitalWrite(FAN_IN2, LOW);

  // Water level sensor
  pinMode(WATER_LEVEL_PIN, INPUT);

  // Servo
  shadeServo.attach(SERVO_PIN);

  // Initial sunshade position
  shadeServo.write(90);

  // Stepper
  ventilationStepper.setSpeed(10);

  Serial.println("System Ready");
  Serial.println("--------------------------------");
}

// =====================================================
// MAIN LOOP
// =====================================================

void loop() {

  // =====================================================
  // 1. WATER LEVEL SENSOR
  // =====================================================

  int waterState = digitalRead(WATER_LEVEL_PIN);

  bool lowWater = false;

  if (waterState == HIGH) {

    Serial.println("WARNING: Low Water Level!");

    lowWater = true;

    // Force pump OFF
    digitalWrite(RELAY_PIN, HIGH);
  }

  // =====================================================
  // 2. SOIL MOISTURE SENSOR + WATER PUMP
  // =====================================================

  if (!lowWater) {

    int soilValue = analogRead(SOIL_PIN);

    Serial.print("Soil Moisture: ");
    Serial.println(soilValue);

    if (soilValue > soilThreshold) {

      Serial.println("Dry Soil -> Pump ON");

      // Relay ON
      digitalWrite(RELAY_PIN, LOW);

    } else {

      Serial.println("Wet Soil -> Pump OFF");

      // Relay OFF
      digitalWrite(RELAY_PIN, HIGH);
    }

  } else {

    Serial.println("Skipping irrigation due to low water level");
  }

  // =====================================================
  // 3. LIGHT SENSOR + SUNSHADE SERVO
  // =====================================================

  int lightValue = analogRead(LDR_PIN);

  Serial.print("Light Level: ");
  Serial.println(lightValue);

  if (lightValue > lightThreshold) {

    if (!shadeClosed) {

      Serial.println("High Light -> Closing Sunshade");

      // Close sunshade
      shadeServo.write(0);

      shadeClosed = true;
    }

  } else {

    if (shadeClosed) {

      Serial.println("Low Light -> Opening Sunshade");

      // Open sunshade
      shadeServo.write(90);

      shadeClosed = false;
    }
  }

  // =====================================================
  // 4. DHT11 SENSOR
  // =====================================================

  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  // Validate reading
  if (isnan(humidity) || isnan(temperature)) {

    Serial.println("Failed to read DHT11 sensor!");

  } else {

    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" °C");

    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");

    // =====================================================
    // 5. FAN + VENTILATION CONTROL
    // =====================================================

    if (temperature > tempThreshold ||
        humidity > humidityThreshold) {

      Serial.println("High Temp/Humidity -> Ventilation ON");

      // Turn fan ON
      digitalWrite(FAN_IN1, HIGH);
      digitalWrite(FAN_IN2, LOW);

      // Open ventilation flap only once
      if (!flapOpen) {

        Serial.println("Opening Ventilation Flap");

        ventilationStepper.step(512);

        flapOpen = true;
      }

    } else {

      Serial.println("Normal Temp/Humidity -> Ventilation OFF");

      // Turn fan OFF
      digitalWrite(FAN_IN1, LOW);
      digitalWrite(FAN_IN2, LOW);

      // Close ventilation flap only once
      if (flapOpen) {

        Serial.println("Closing Ventilation Flap");

        ventilationStepper.step(-512);

        flapOpen = false;
      }
    }
  }

  // =====================================================
  // LOOP DELAY
  // =====================================================

  Serial.println("--------------------------------");

  delay(3000);
}