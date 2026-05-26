#include <WiFi.h>
#include <Arduino_MQTT_Client.h>
#include <PubSubClient.h>
#include <ThingsBoard.h>
#include <DHT.h>


#define LDR_PIN           35
#define DHT_PIN           4
#define DHTTYPE           DHT22
const char* WIFI_SSID = "jack";
const char* WIFI_PASSWORD = "1912Cx30";
const char* TOKEN = "zdmMMCbwdnGnUAfIQgif";
const char* THINGSBOARD_SERVER = "thingsboard.cloud";
constexpr uint16_t THINGSBOARD_PORT = 1883;

DHT dht(DHT_PIN, DHTTYPE);

int lightThreshold = 2000;
float tempThreshold = 40.0;
float humidityThreshold = 80.0;

WiFiClient wifiClient;
Arduino_MQTT_Client mqttClient(wifiClient);
ThingsBoard tb(mqttClient);

void connectWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
}

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

void setup() {
  Serial.begin(115200);
  connectWiFi();
  Serial.println("System Ready");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (!tb.connected()) connectThingsBoard();

  tb.loop();
  delay(3000);
}
