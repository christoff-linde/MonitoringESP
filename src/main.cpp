/**
 * @file main.cpp
 * @author Christoff Linde
 * @brief A basic program for monitoring a DHT-22 with an ESP8266
 * @version 0.3
 * @date 2021-03/02
 *
 * Some of the code below was inspired by the 
 * [A Beginner's Guide to the ESP8266](https://tttapa.github.io/ESP8266/Chap01%20-%20ESP8266.html) ebook
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
 /**
  * @brief Initialise WiFiMulti and start a WiFi Access Point.
  *
  * @details This method intialises multiple Access Points. The most suitable one is then connected to.
  * On successful connection, the IP Address assigned to the device is displayed
  */
void startWiFi();

/**
 * @brief Start listening for UDP messages
 *
 * @details This method initialises the WiFiUDP UDP object. On initialization, the UDP listens on port 123
 * by default. The local port is also printed to Serial console.
 */
void startUDP();

/**
 * @brief Start the LittleFS file system
 *
 * @details This method starts the file system for the device. After successfully starting, the contents
 * of the root directory is listed to Serial console.
 * 
 * @see LittleFS
 * @see listDirectory
 */
void startLittleFS();

/**
 * @brief Start the DHT sensor
 *
 * @details This method initialises the DHT object with the default global parameters.
 */
void startSensors();

/**
 * @brief Convert sizes in bytes to KB and MB
 *
 * @details This helper method converts the passed in data to a more human readable format i.e. into KB and MB
 *
 * @see startLittleFS
 * 
 * @param bytes size_t size to be formated
 * @returns String - the formated string
 */
String formatBytes(size_t bytes);

/**
 * @brief Get the UNIX time
 *
 * @details This method checks if the time server has responded. If so,
 *  \li get the UNIX time
 *  otherwise
 *  \li return 0
 *
 * @returns unsigned long - UNIX time or 0
 */
unsigned long getTime();

/**
 * @brief Send NTP packet to IPAddress
 * 
 * @details This method sends a NTP request to the given IPAddress. 
 * All bytes in the packetBuffer are set to 0, with the size determined by NTP_PACKET_SIZE.
 * 
 * A packet is then sent to the WiFiUDP object to the specified port.
 * 
 * @param address IPAddress of the UDP server
 */
void sendNTPpacket(IPAddress& address);

/**
 * @brief List contents of directory
 * 
 * @details This method steps recursively through all directories in the specified filepath,
 * and prints each file by name and size.
 * 
 * @param path the relative path to be listed
 */
void listDirectory(const char* path);

/**
 * @brief Read contents of the specified file
 * 
 * @details This method reads all contents of the file at the specified filepath. If the file
 * does not exist,
 *  \li the file will not be created automatically.
 * If the file exists,
 * \li all contents will be printed to the Serial console.
 * 
 * @param path the relative filepath to the requested file
 */
void readFile(const char* path);

/**
 * @brief Delete the specified file
 * 
 * @details This method deletes the file at the given filepath. If the file does not exist, the
 * delete will fail and a error message will accordingly be printed to the console.
 * 
 * @param path the relative filepath to the requested file
 */
void deleteFile(const char* path);

/**
 * @brief Send data readings to API
 * 
 * @details This method initialises a HTTPClient and API endpoint. The data file storing the 
 * data readings will be opened and processed line by line with @see readStringUntil. This occur 
 * three times for each line of data. 
 * 
 * Read data is stored in a String buffer. For each endline delimitted line of the data file, a JsonObject
 * is created with the necessary data. The created object is then serialized and appended to a StaticJsonDocument
 * declared at the start of the loop.
 * 
 * After processing all lines of data, the data file is flushed and cleared.
 * 
 * An HTTP.POST request is made to the API endpoint with the serialized json data in the request body. The HTTP 
 * response code is returned
 * 
 * @return int - the HTTP response code
 */
int sendData();

#define ONE_HOUR 3600000UL

/// DHTTYPE variable specifying the sensor type
#define DHTTYPE DHT22

/// Physical pin on ESP that maps to GPIO-05
uint8_t DHTPIN = D1;

/**
 * @brief Initialise DHT config
 * 
 * @details Instantiate a DHT object with the DHTPIN and DHTTYPE parameters
 * 
 * @return DHT instantiated DHT object
 */
DHT dht(DHTPIN, DHTTYPE);

/// Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'
ESP8266WiFiMulti wifiMulti;

/// Create an instance of the WiFiUDP class to send and receive UDP messages
WiFiUDP UDP;

/// The time.nist.gov NTP server's IP Address
IPAddress timeServerIP;
const char* ntpServerName = "time.nist.gov";

/// NTP timestamp is in the first 48 bytes of the message
const int NTP_PACKET_SIZE = 48;

/// A buffer to hold incoming and outgoing UDP packets
byte packetBuffer[NTP_PACKET_SIZE];

/**
 * @brief Run at every startup
 * 
 * @details The setup method run at every device startup.
 */
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

/// Update the NTP time every hour
const unsigned long intervalNTP = ONE_HOUR;
/// Store timestamp of previous NTP update
unsigned long prevNTP = 0;
/// Timestamp of lastNTP response initialized to current time
unsigned long lastNTPResponse = millis();

/// Send POST requests every 1 Hour
const unsigned long intervalPost = 3600000;
/// Read sensors every 15 min
const unsigned long intervalTemp = 900000;
unsigned long prevReading = 0;
unsigned long prevSend = 0;
bool dataSent = false;
bool dataRequested = false;
/// Delay to cater for slow 2000ms polling rate of DHT22 sensor
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

      File dataLog = LittleFS.open("data.txt", "a");

      dataLog.print(actualTime);
      dataLog.print(',');
      dataLog.print(humidity);
      dataLog.print(',');
      dataLog.println(temperature);

      dataLog.close();
    }

    if (currentMillis - prevSend > intervalPost)
    {
      dataSent = true;
      prevSend = currentMillis;
    }
    if (currentMillis - prevSend && dataSent)
    {
      dataSent = false;
      Serial.println("Sending data to .NET API");
      int responseCode = sendData();
      if (responseCode > 0)
      {
        Serial.printf("HTTP Response code: %i\n", responseCode);
      }
      else
      {
        Serial.printf("HTTP Error: %i\n", responseCode);
      }
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

int sendData()
{
  HTTPClient http;

  const char* URL = "http://192.168.0.108:5000/api/DataEntries/list";

  StaticJsonDocument<2048> doc;

  File dataFile;
  String buf;

  dataFile = LittleFS.open("data.txt", "r");
  while (dataFile.available())
  {
    JsonObject readingObject = doc.createNestedObject();

    buf = dataFile.readStringUntil(',');
    readingObject["timestamp"] = buf.toInt();

    buf = dataFile.readStringUntil(',');
    readingObject["humidity"] = buf.toFloat();

    buf = dataFile.readStringUntil('\n');
    readingObject["temperature"] = buf.toFloat();
  }

  dataFile.flush();

  dataFile.close();

  // serializeJsonPretty(doc, Serial);

  http.begin(URL);
  http.addHeader("Content-Type", "application/json");

  String serializedJSON;
  serializeJson(doc, serializedJSON);

  int responseCode = http.POST(serializedJSON);

  http.end();

  return responseCode;
}
