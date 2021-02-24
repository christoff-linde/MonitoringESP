#include <Arduino.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <string.h>

const char* _ssid = "CL001";
const char* _password = "Christo)(*";
// const char* _ssid = "Jagter";
// const char* _password = "Altus1912";

// Time between POST requests
static unsigned int delayTime = 3600000; // 30 000 = 30 sec

#define DHTTYPE DHT22
uint8_t DHTPIN = D6;

DHT dht(DHTPIN, DHTTYPE);

int sendData(StaticJsonDocument<64> data) {
  String URL = "http://192.168.0.108:5000/api/DataEntries";
  // String URL = "http://10.0.0.100:5000/api/DataEntries";

  HTTPClient http;

  http.begin(URL);
  http.addHeader("Content-Type", "application/json");

  String serializedJson;
  serializeJson(data, serializedJson);

  int responseCode = http.POST(serializedJson);

  http.end();

  return responseCode;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(DHTPIN, INPUT);
  dht.begin();

  WiFi.begin(_ssid, _password);
  Serial.println("");
  Serial.print("Connecting to ");
  Serial.println(_ssid);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  // put your main code here, to run repeatedly:
  static unsigned long last_time = 0;

  if (millis() - last_time > delayTime) {
    if (WiFi.status() == WL_CONNECTED) {
      StaticJsonDocument<64> doc;

      doc["humidity"] = dht.readHumidity();
      doc["temperatureC"] = dht.readTemperature();

      serializeJsonPretty(doc, Serial);

      int responseCode = sendData(doc);

      if (responseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(responseCode);
      }
      else {
        Serial.print("HTTP Error: ");
        Serial.println(responseCode);
      }
    }
    else {
      Serial.println("WiFi Disconnected");
    }
    last_time = millis();
  }
}