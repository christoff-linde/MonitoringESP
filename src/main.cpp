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
#include <FS.h>
#include <LittleFS.h>
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <SPI.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>

#include <iostream>
#include <fstream>
#include <iomanip>
#include <string.h>

 // NTP Servers:
static const char ntpServerName[] = "us.pool.ntp.org";

const int timeZone = 2;     // Central European Time

void initWifi();
void initDHT();
int sendData();
int sendData(StaticJsonDocument<64> data);

const char* _ssid = "CL001";
const char* _password = "Christo)(*";
// const char* _ssid = "Jagter";
// const char* _password = "Altus1912";

// Time between POST requests
static unsigned int postDelayTime = 60000;   // 86 400 000 = 24h

// Time between DHT Readings
static unsigned int dhtDelayTime = 4000;      // 900 000    = 15 min

#define DHTTYPE DHT22
uint8_t DHTPIN = D1;                            // Physical pin on ESP

DHT dht(DHTPIN, DHTTYPE);

WiFiUDP Udp;
unsigned int localPort = 8888;

DynamicJsonDocument doc(2048);

time_t getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
void sendNTPpacket(IPAddress& address);

void initData();
void listDir(const char* dirname);
void readFile(const char* path);
void writeFile(const char* path);
void appendFile(const char* path);
void deleteFile(const char* path);

/**
 * @brief initialise folder structure if not present
 *
 */
void initData()
{
  if (!LittleFS.begin())
  {
    Serial.println("LittleFS mount failed");
    return;
  }

  Serial.println("initializing data");
  listDir("/data");

  if (LittleFS.exists("/data/data.txt"))
    deleteFile("/data/data.txt");

  // if (LittleFS.exists("/data/data.json"))
  //   deleteFile("/data/data.json");
}

/**
 * @brief list contents of directory
 *
 * @param dirname directory name
 */
void listDir(const char* dirname)
{
  Serial.printf("Listing directory: %s\n", dirname);

  Dir root = LittleFS.openDir(dirname);

  while (root.next())
  {
    File file = root.openFile("r");

    Serial.print("\tFILE:\t");
    Serial.print(root.fileName());
    Serial.print("\tSIZE:\t");
    Serial.println(file.size());

    time_t cr = file.getCreationTime();
    time_t lw = file.getLastWrite();

    file.close();
  }
}

/**
 * @brief Read file and print contents to Serial
 *
 * @param path path to the file
 */
void readFile(const char* path)
{
  Serial.printf("Reading file: %s\n", path);

  File file = LittleFS.open(path, "r");
  if (!file)
  {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  while (file.available())
  {
    Serial.write(file.read());
  }

  file.close();
}

/**
 * @brief Write data to a file. If the file already exists, it wil be overwritten
 *
 * @param path path to the file
 * @param message message to be added to file
 */
void writeFile(const char* path, const char* message)
{
  Serial.printf("Writing file: %s\n", path);

  File file = LittleFS.open(path, "w");
  if (!file)
  {
    Serial.println("Failed to open file for writing");
    return;
  }

  if (file.print(message))
  {
    Serial.println("File written");
  }
  else
  {
    Serial.println("Write failed");
  }

  delay(2000);
  file.close();
}

/**
 * @brief Append to an existing file
 *
 * @param path path to the file
 * @param message nessage to be appended
 */
void appendFile(const char* path, const char* message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = LittleFS.open(path, "a");
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message)) {
    Serial.println("Message appended");
  }
  else {
    Serial.println("Append failed");
  }
  file.close();
}

/**
 * @brief Rename existing file
 *
 * @param path1 path to existing file name
 * @param path2 path to new file name
 */
void renameFile(const char* path1, const char* path2) {
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (LittleFS.rename(path1, path2)) {
    Serial.println("File renamed");
  }
  else {
    Serial.println("Rename failed");
  }
}

/**
 * @brief Delete file at given path
 *
 * @param path path to file to be deleted
 */
void deleteFile(const char* path) {
  Serial.printf("Deleting file: %s\n", path);
  if (LittleFS.remove(path)) {
    Serial.println("File deleted");
  }
  else {
    Serial.println("Delete failed");
  }
}

/**
 * @brief Initial setup code that will only be executed once (on startup)
 *
 */
void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);

  while (!Serial) {}

  initData();
  initWifi();
  initDHT();
}

/**
 * @brief Main code that will be be executed repeatedly
 *
 */
void loop()
{
  // put your main code here, to run repeatedly:
  static unsigned long last_time = 0;
  static unsigned long post_last_time = 0;

  // Read DHT
  if (millis() - last_time > dhtDelayTime)
  {
    if (WiFi.status() == WL_DISCONNECTED)
    {
      initWifi();
    }

    if (WiFi.status() == WL_CONNECTED)
    {

      // read sensor
      time_t timestamp = getNtpTime();
      float humidity = dht.readHumidity();
      float temperature = dht.readTemperature();

      JsonObject dataReading = doc.createNestedObject();
      dataReading["timestamp"] = timestamp;
      dataReading["humidity"] = humidity;
      dataReading["temperatureC"] = temperature;

      // serializeJsonPretty(doc, Serial);

      File dataFile = LittleFS.open("/data/data.json", "w");
      serializeJson(doc, dataFile);
      dataFile.close();

      // readFile("/data/data.json");

      // File dataFile = LittleFS.open("/data/data.json", "a");
      // listDir("/data/");

      // serializeJson(dataReading, dataFile);
      // dataFile.close();

      // readFile("/data/data.json");
    }
    last_time = millis();
  }

  if (millis() - post_last_time > postDelayTime)
  {
    if (WiFi.status() == WL_DISCONNECTED)
    {
      initWifi();
    }

    if (WiFi.status() == WL_CONNECTED)
    {
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
    post_last_time = millis();
  }

  // Send data

  // if (millis() - last_time > postDelayTime) {
  //   static unsigned int dhtDelayTime = 900000;
  //   if (WiFi.status() == WL_CONNECTED) {
  //     StaticJsonDocument<64> doc;

  //     doc["timestamp"] = getNtpTime();
  //     doc["humidity"] = dht.readHumidity();
  //     doc["temperatureC"] = dht.readTemperature();

  //     Serial.println("");
  //     serializeJsonPretty(doc, Serial);
  //     Serial.println("");

  //     int responseCode = sendData(doc);

  //     if (responseCode > 0) {
  //       Serial.print("HTTP Response code: ");
  //       Serial.println(responseCode);
  //     }
  //     else {
  //       Serial.print("HTTP Error: ");
  //       Serial.println(responseCode);
  //     }
  //   }
  //   else {
  //     Serial.println("WiFi Disconnected");
  //   }
  //   last_time = millis();
  // }
}

/**
 * @brief Method to send JSON data to API Endpoint
 * 
 * @return int HTTP response code
 */
int sendData()
{
  String URL = "http://192.168.0.108:5000/api/DataEntries/list";

  HTTPClient http;

  http.begin(URL);
  http.addHeader("Content-Type", "application/json");

  File dataFile = LittleFS.open("/data/data.json", "r");

  // DynamicJsonDocument temp(2048);
  // deserializeJson(temp, dataFile);

  String serializedJson;
  serializeJson(doc, serializedJson);

  int responseCode = http.POST(serializedJson);

  http.end();

  return responseCode;
}

/**
 * @brief Method to send JSON data to API Endpoint
 *
 * @param data serialized JSON data
 * @return int HTTP response code
 */
int sendData(StaticJsonDocument<64> data)
{
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

/**
 * @brief setup WiFi connection
 *
 */
void initWifi()
{
  WiFi.begin(_ssid, _password);
  Serial.println("");
  Serial.print("Connecting to ");
  Serial.println(_ssid);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  // Serial.print("Connected to WiFi network with IP Address: ");
  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(300);
}

/**
 * @brief setup DHT settings for temperature sensor
 *
 */
void initDHT()
{
  Serial.printf("initializing DHT sensor");
  pinMode(DHTPIN, INPUT);
  dht.begin();
}

void digitalClockDisplay()
{
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(".");
  Serial.print(month());
  Serial.print(".");
  Serial.print(year());
  Serial.println();
}

void printDigits(int digits)
{
  // utility for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

/**
 * @brief Get the Ntp Time object
 *
 * @return time_t
 */
time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0); // discard any previously received packets
  // Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      // Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 = (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response");
  return 0; // return 0 if unable to get the time
}

/**
 * @brief send an NTP request to the time server at the given address
 *
 * @param address - destination IPAddress
 */
void sendNTPpacket(IPAddress& address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

