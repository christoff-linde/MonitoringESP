#include <Arduino.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <string.h>

// NTP Servers:
static const char ntpServerName[] = "us.pool.ntp.org";
//static const char ntpServerName[] = "time.nist.gov";
//static const char ntpServerName[] = "time-a.timefreq.bldrdoc.gov";
//static const char ntpServerName[] = "time-b.timefreq.bldrdoc.gov";
//static const char ntpServerName[] = "time-c.timefreq.bldrdoc.gov";

const int timeZone = 2;     // Central European Time
//const int timeZone = -5;  // Eastern Standard Time (USA)
//const int timeZone = -4;  // Eastern Daylight Time (USA)
//const int timeZone = -8;  // Pacific Standard Time (USA)
//const int timeZone = -7;  // Pacific Daylight Time (USA)

void initWifi();
void initDHT();
int sendData(StaticJsonDocument<64> data);

const char* _ssid = "CL001";
const char* _password = "Christo)(*";
// const char* _ssid = "Jagter";
// const char* _password = "Altus1912";

// Time between POST requests
static unsigned int delayTime = 900000; // 900 000 = 15min

#define DHTTYPE DHT22
uint8_t DHTPIN = D6;

DHT dht(DHTPIN, DHTTYPE);

WiFiUDP Udp;
unsigned int localPort = 8888;

time_t getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
void sendNTPpacket(IPAddress& address);

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  initWifi();
  initDHT();
}

void loop()
{
  // put your main code here, to run repeatedly:
  static unsigned long last_time = 0;

  if (millis() - last_time > delayTime) {
    if (WiFi.status() == WL_CONNECTED) {
      StaticJsonDocument<64> doc;

      // time_t currentTime = getNtpTime();

      doc["timestamp"] = getNtpTime();
      doc["humidity"] = dht.readHumidity();
      doc["temperatureC"] = dht.readTemperature();

      Serial.println("");
      serializeJsonPretty(doc, Serial);
      Serial.println("");

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

void initDHT()
{
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

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0); // discard any previously received packets
  Serial.println("Transmit NTP Request");
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
      Serial.println("Receive NTP Response");
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

// send an NTP request to the time server at the given address
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

