/**
 * @file main.cpp
 * @author Christoff Linde
 * @brief The main program containing all functions
 * @version 0.2
 * @date 2021-02-17
 *
 * @copyright Copyright (c) 2021
 *
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <LittleFS.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>

 // Forward declarations
void startWiFi();
void startUDP();
void startLittleFS();
void startSensors();
String formatBytes(size_t bytes);
unsigned long getTime();
void sendNTPpacket(IPAddress& address);
void listDirectory(const char* path);
void readFile(const char* path);
void deleteFile(const char* path);

#define ONE_HOUR 3600000UL

#define DHTTYPE DHT22
uint8_t DHTPIN = D1;

DHT dht(DHTPIN, DHTTYPE);

ESP8266WiFiMulti wifiMulti;

WiFiUDP UDP;

IPAddress timeServerIP;
const char* ntpServerName = "time.nist.gov";

const int NTP_PACKET_SIZE = 48;

byte packetBuffer[NTP_PACKET_SIZE];

void setup()
{
  Serial.begin(115200);
  delay(10);
  Serial.println("\r\n");

  startWiFi();

  startLittleFS();

  startUDP();

  startSensors();

  WiFi.hostByName(ntpServerName, timeServerIP);
  Serial.print("Time server IP:\t");
  Serial.println(timeServerIP);

  sendNTPpacket(timeServerIP);
  delay(500);

}

const unsigned long intervalNTP = ONE_HOUR;
unsigned long prevNTP = 0;
unsigned long lastNTPResponse = millis();

const unsigned long intervalTemp = 60000;
unsigned long prevReading = 0;
bool dataRequested = false;
const unsigned long DS_delay = 2000;

uint32_t timeUNIX = 0;

void loop()
{
  unsigned long currentMillis = millis();

  if (currentMillis - prevNTP > intervalNTP)
  {
    prevNTP = currentMillis;
    sendNTPpacket(timeServerIP);
  }

  uint32_t time = getTime();
  if (time)
  {
    timeUNIX = time;
    Serial.print("NTP response:\t");
    Serial.println(timeUNIX);
    lastNTPResponse = millis();
  }
  else if ((millis() - lastNTPResponse) > 24UL * ONE_HOUR)
  {
    Serial.println("More than 24 hours since last NTP response. Rebooting.");
    Serial.flush();
    ESP.reset();
  }

  if (timeUNIX != 0)
  {
    if (currentMillis - prevReading > intervalTemp)
    {
      dataRequested = true;
      prevReading = currentMillis;
    }
    if (currentMillis - prevReading > DS_delay && dataRequested)
    {
      uint32_t actualTime = timeUNIX + (currentMillis - lastNTPResponse) / 1000;
      dataRequested = false;
      float humidity = dht.readHumidity();
      humidity = round(humidity * 100.0) / 100.0;
      float temperature = dht.readTemperature();
      temperature = round(temperature * 100.0) / 100.0;

      const size_t CAPACITY = JSON_OBJECT_SIZE(3);
      StaticJsonDocument<CAPACITY> doc;

      JsonObject reading = doc.to<JsonObject>();
      reading["timestamp"] = actualTime;
      reading["humidity"] = humidity;
      reading["temperature"] = temperature;

      Serial.printf("\nAppending data to file: %i\t", actualTime);
      Serial.printf("Humidity: %f\tTemperature: %f\n", humidity, temperature);

      File dataFile = LittleFS.open("/data.ndjson", "a");
      serializeJson(reading, dataFile);
      dataFile.close();

      readFile("/data.ndjson");

      File dataLog = LittleFS.open("/data.txt", "a");
      dataLog.print(actualTime);
      dataLog.print(',');
      dataLog.print(humidity);
      dataLog.print(',');
      dataLog.print(temperature);
      dataLog.print(';');
      dataLog.close();
    }
  }
  else
  {
    sendNTPpacket(timeServerIP);
    delay(500);
  }

}

void startWiFi()
{
  wifiMulti.addAP("CL001", "Christo)(*");
  wifiMulti.addAP("Jagter", "Altus1912");

  Serial.println("Connecting");
  while (wifiMulti.run() != WL_CONNECTED)
  {
    delay(250);
    Serial.print('.');
  }
  Serial.println("\r\n");
  Serial.print("Connected to ");
  Serial.println(WiFi.SSID());
  Serial.print("IP address:\t");
  Serial.print(WiFi.localIP());
  Serial.println("\r\n");
}

void startUDP()
{
  Serial.println("Starting UDP");
  UDP.begin(123);
  Serial.print("Local port:\t");
  Serial.println(UDP.localPort());
}

void startLittleFS()
{
  if (!LittleFS.begin())
  {
    Serial.printf("LittleFS mount failed\n");
    return;
  }

  Serial.println("LittleFS started. Contents:");
  listDirectory("/");

  deleteFile("/data.json");
  deleteFile("/data.ndjson");
  deleteFile("/data.txt");
  deleteFile("/hello.txt");
}

void startSensors()
{
  Serial.println("Initialising sensors:");
  dht.begin();
  Serial.println("DHT22 initialised");
}

String formatBytes(size_t bytes)
{
  if (bytes < 1024) { return String(bytes) + "B"; }
  else if (bytes < (1024 * 1024)) { return String(bytes / 1024.0) + "KB"; }
  else if (bytes < (1024 * 1024 * 1024)) { return String(bytes / 1024.0 / 1024.0) + "MB"; }
  else { return "null"; }
}

unsigned long getTime()
{
  if (UDP.parsePacket() == 0)
  {
    return 0;
  }
  UDP.read(packetBuffer, NTP_PACKET_SIZE);
  uint32_t NTPTime = (packetBuffer[40] << 24) | (packetBuffer[41] << 16) | (packetBuffer[42] << 8) | packetBuffer[43];

  const uint32_t seventyYears = 2208988800UL;
  uint32_t UNIXTime = NTPTime - seventyYears;
  return UNIXTime;
}

void sendNTPpacket(IPAddress& address)
{
  Serial.println("Sending NTP request");
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011;

  UDP.beginPacket(address, 123);
  UDP.write(packetBuffer, NTP_PACKET_SIZE);
  if (UDP.endPacket() == 0)
    Serial.println("NTP request failed");
}

void listDirectory(const char* path)
{
  Dir dir = LittleFS.openDir(path);
  while (dir.next())
  {
    String fileName = dir.fileName();
    size_t fileSize = dir.fileSize();
    Serial.printf("\tFS File: %s, size: %s\r\n", fileName.c_str(), formatBytes(fileSize).c_str());
  }
  Serial.printf("\n");
}

void readFile(const char* path)
{
  File file = LittleFS.open(path, "r");
  if (!file)
  {
    Serial.printf("Failed to open file %s for reading", path);
    return;
  };

  while (file.available())
  {
    Serial.write(file.read());
  }

  file.close();
}

void deleteFile(const char* path)
{
  if (LittleFS.remove(path))
  {
    Serial.printf("File at %s deleted", path);
    Serial.printf("\n\r");
  }
  else
  {
    Serial.println("File delete failed");
  }
}
