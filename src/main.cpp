#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <string.h>

const char* ssid = "CL001";
const char* password = "Christo)(*";

String serverName = "http://192.168.0.108:5005/api/DataEntries";

static unsigned int delayTime = 30000;

int sendData() {
  String baseURL = "http://192.168.0.108";
  String port = ":5005";
  String endPoint = "/api/DataEntries";

  String URL = "http://192.168.0.108:5005/api/DataEntries";

  HTTPClient http;

  http.begin(URL);
  http.addHeader("Content-Type", "application/json");

  double temperature = random(22, 35);

  String body = "{\"TemperatureC\":";
  body.concat((String(temperature)));
  body.concat("}");

  Serial.println("");
  Serial.print("body: ");
  Serial.println(body);

  int responseCode = http.POST(body);

  http.end();

  return responseCode;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.println("");
  Serial.print("Connecting to ");
  Serial.println(ssid);

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
      // HTTPClient http;

      // http.begin(serverName.c_str());

      // int httpResponseCode = http.GET();

      // if (httpResponseCode > 0) {
      //   Serial.print("HTTP Response code: ");
      //   Serial.println(httpResponseCode);

      //   String payload = http.getString();
      //   Serial.println(payload);
      // }
      // else {
      //   Serial.print("Error code: ");
      //   Serial.println(httpResponseCode);
      // }

      // http.end();
      int responseCode = sendData();

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
