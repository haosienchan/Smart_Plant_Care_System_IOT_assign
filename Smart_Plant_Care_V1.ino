#include <WiFi.h>
#include <Arduino_MQTT_Client.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ESP32Servo.h>
#include <Stepper.h>
#include <ThingsBoard.h>

// =====================================================
// WIFI + THINGSBOARD
// =====================================================

const char* WIFI_SSID = "jack";
const char* WIFI_PASSWORD = "1912Cx30";

const char* TOKEN = "zdmMMCbwdnGnUAfIQgif";
const char* THINGSBOARD_SERVER = "thingsboard.cloud";

constexpr uint16_t THINGSBOARD_PORT = 1883;

WiFiClient wifiClient;
Arduino_MQTT_Client mqttClient(wifiClient);
ThingsBoard tb(mqttClient);

// =====================================================
// PIN DEFINITIONS
// =====================================================

#define SOIL_PIN          34
#define LDR_PIN           35
#define WATER_LEVEL_PIN   32
#define DHT_PIN           4

#define RELAY_PIN         5

#define FAN_IN1           19
#define FAN_IN2           21

#define SERVO_PIN         13

#define IN1               14
#define IN2               27
#define IN3               26
#define IN4               25

// =====================================================
// DHT
// =====================================================

#define DHTTYPE DHT22
DHT dht(DHT_PIN, DHTTYPE);

// =====================================================
// SERVO
// =====================================================

Servo shadeServo;

// =====================================================
// STEPPER
// =====================================================

const int stepsPerRevolution = 2048;
const int FLAP_STEPS = 3 * stepsPerRevolution;

Stepper ventilationStepper(
  stepsPerRevolution,
  IN1, IN3, IN2, IN4
);

// =====================================================
// THRESHOLDS
// =====================================================

int soilThreshold = 2300;
int lightThreshold = 2000;

float tempThreshold = 40.0;
float humidityThreshold = 80.0;

// =====================================================
// STATE
// =====================================================

bool flapOpen = false;
bool shadeClosed = false;
bool pumpOn = false;
bool fanOn = false;

// =====================================================
// WIFI
// =====================================================

void connectWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");
  Serial.println(WiFi.localIP());
}

// =====================================================
// THINGSBOARD
// =====================================================

void connectThingsBoard() {
  while (!tb.connected()) {
    Serial.println("Connecting to ThingsBoard...");

    if (tb.connect(THINGSBOARD_SERVER, TOKEN, THINGSBOARD_PORT)) {
      Serial.println("ThingsBoard Connected!");
    } else {
      Serial.println("Connection failed");
      delay(2000);
    }
  }
}

// =====================================================
// SETUP
// =====================================================

void setup() {
  Serial.begin(115200);

  connectWiFi();

  dht.begin();

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);

  pinMode(FAN_IN1, OUTPUT);
  pinMode(FAN_IN2, OUTPUT);

  pinMode(WATER_LEVEL_PIN, INPUT);

  shadeServo.attach(SERVO_PIN);
  shadeServo.write(90);

  ventilationStepper.setSpeed(10);

  Serial.println("System Ready");
}

// =====================================================
// LOOP
// =====================================================

void loop() {

  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (!tb.connected()) connectThingsBoard();

  // =====================================================
  // SENSORS
  // =====================================================

  int waterState = digitalRead(WATER_LEVEL_PIN);
  bool lowWater = false;
  int water = 1;
  int pump = 0;

  if (waterState == HIGH) {
    Serial.println("Low Water!");
    lowWater = true;
    water = 0;

    digitalWrite(RELAY_PIN, HIGH);
    pumpOn = false;
  }

  int soilValue = analogRead(SOIL_PIN);

  if (!lowWater) {
    if (soilValue > soilThreshold) {
      digitalWrite(RELAY_PIN, LOW);
      pumpOn = true;
      pump = 1;
    } else {
      digitalWrite(RELAY_PIN, HIGH);
      pumpOn = false;
      pump = 0;
    }
  }

  int lightValue = analogRead(LDR_PIN);
  int shade = 0;

  if (lightValue > lightThreshold) {
    if (!shadeClosed) {
      shadeServo.write(0);
      shadeClosed = true;
      shade = 1;
    }
  } else {
    if (shadeClosed) {
      shadeServo.write(90);
      shadeClosed = false;
      shade = 0;
    }
  }

  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  int fan = 0;
  int flap = 0;
  bool shouldVentilate = false;

  if (!isnan(humidity) && !isnan(temperature)) {

    Serial.print("Temp: "); Serial.println(temperature);
    Serial.print("Humidity: "); Serial.println(humidity);

    shouldVentilate =
      (temperature > tempThreshold || humidity > humidityThreshold);

    if (shouldVentilate) {
      fan = 1;
      fanOn = true;
    } else {
      fan = 0;
      fanOn = false;
    }
  }

  // =====================================================
  // ✅ TELEMETRY SENT FIRST (FIX)
  // =====================================================

  tb.sendTelemetryData("temperature", temperature);
  tb.sendTelemetryData("humidity", humidity);
  tb.sendTelemetryData("soilValue", soilValue);
  tb.sendTelemetryData("lightValue", lightValue);
  tb.sendTelemetryData("lowWater", lowWater);
  tb.sendTelemetryData("water", water);
  tb.sendTelemetryData("pumpOn", pumpOn);
  tb.sendTelemetryData("pump", pump);
  tb.sendTelemetryData("fanOn", fanOn);
  tb.sendTelemetryData("fan", fan);
  tb.sendTelemetryData("shadeClosed", shadeClosed);
  tb.sendTelemetryData("shade", shade);

  Serial.println("Telemetry sent FIRST");

  // =====================================================
  // HARDWARE ACTIONS (AFTER TELEMETRY)
  // =====================================================

  if (shouldVentilate) {

    digitalWrite(FAN_IN1, HIGH);
    digitalWrite(FAN_IN2, LOW);

    if (!flapOpen) {
      Serial.println("Opening flap (CW 3 rotations)");
      flapOpen = true;
      flap = 1;
      tb.sendTelemetryData("flapOpen", flapOpen);
      tb.sendTelemetryData("flap", flap);
      ventilationStepper.step(FLAP_STEPS);
    }

  } else {

    digitalWrite(FAN_IN1, LOW);
    digitalWrite(FAN_IN2, LOW);

    if (flapOpen) {
      Serial.println("Closing flap (CCW 3 rotations)");
      flapOpen = false;
      flap = 0;
      tb.sendTelemetryData("flapOpen", flapOpen);
      tb.sendTelemetryData("flap", flap);
      ventilationStepper.step(-FLAP_STEPS);
    }
  }

  

  tb.loop();

  delay(3000);
}
