// Additional board URL's:
// http://arduino.esp8266.com/stable/package_esp8266com_index.json
// https://espressif.github.io/arduino-esp32/package_esp32_index.json

#include <ArduinoOTA.h>

#if defined(ESP32)
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
WebServer server(80);
#define RXD2 16  //ESP32 firebeetle
#define TXD2 17  //ESP32 firebeetle

#elif defined(ESP8266)
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
ESP8266WebServer server(80);

#else
#error "Unsupported board selected"
#endif

#include <WiFiUdp.h>

#define MAXLINELENGTH 512
#define uS_TO_S_FACTOR 1000000

/* EDIT FOLLOWING ITEMS ========================== */

// Use LED on ESP8266 modules
//#define USE_LED 1 // comment out if you dont want the led to flash when a new message has been sent

// Pin 0 can generate an OTA upload timeout on some devices, thus making OTA unavailable
#define REQ_PIN 0              // pin 0 -> ESP01S (GPIO0), ESP8266 (D3), ESP32 firebeetle (GPIO0)
#define TIME_INTERVAL 1        // Read time interval in n seconds

// Static IP settings
#define IP_STATIC 1 // comment out if you want to use DHCP

const char* ota_name = "ESP-P1";
const char* ssid = "YOUR_SSID"; // YOUR_SSID
const char* password = "YOUR_PASS"; // YOUR_PASS
const char* host = "192.168.0.20";
const uint16_t port = 1889;

#if defined(IP_STATIC)
IPAddress local_IP(192,168,0,21);
IPAddress gateway(192,168,0,1);
IPAddress subnet(255,255,255,0);
IPAddress primaryDNS(195,130,130,1);
IPAddress secondaryDNS(195,130,131,1);
#endif
/* END EDIT ITEMS ========================== */

// Vars to store meter readings
long ECHT = 0;  // electricity consumption high tariff
long ECLT = 0;  // electricity consumption low tariff
long ERHT = 0;  // electricity return high tariff
long ERLT = 0;  // electricity return low tariff
long EAC = 0;   // electricity actual consumption
long EAR = 0;   // electricity actual return

long EL1C = 0;  // electricity L1 actual consumption
long EL2C = 0;  // electricity L2 actual consumption
long EL3C = 0;  // electricity L3 actual consumption
long EL1R = 0;  // electricity L1 actual return
long EL2R = 0;  // electricity L2 actual return
long EL3R = 0;  // electricity L3 actual return

long EL1V = 0;  // electricity L1 actual voltage
long EL2V = 0;  // electricity L2 actual voltage
long EL3V = 0;  // electricity L3 actual voltage
long EL1I = 0;  // electricity L1 actual current
long EL2I = 0;  // electricity L2 actual current
long EL3I = 0;  // electricity L3 actual current

long ETAR = 0;  // electricity tariff (1 = day, 2 = night)
long ETAC = 0;  // electricity actual average consumption
long ETPC = 0;  // electricity peak average consumption
long MEID = 0;  // meter DSMR version id
long MESN = 0;  // meter serial number
long METS = 0;  // meter telegram timestamp

long VERS = 0;  // DSMR version
long GAST = 0;  // gas total consumption in cubic metres
long WAST = 0;  // water total consumption in cubic metres

char sValue[MAXLINELENGTH];
char telegram[MAXLINELENGTH];
const bool outputOnSerial = false;
unsigned int currentCRC = 0;
unsigned long currentTime = 0;
unsigned long lastTime = 0;
const unsigned long period = TIME_INTERVAL * 1000;

WiFiClient wifiClient;