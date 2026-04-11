// Additional board URL's:
// http://arduino.esp8266.com/stable/package_esp8266com_index.json
// https://espressif.github.io/arduino-esp32/package_esp32_index.json

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

#include <PubSubClient.h>

#define MAXLINELENGTH 512
#define JSONLENGTH    768  // enough for the full JSON payload

/* EDIT FOLLOWING ITEMS ========================== */

//#define USE_LED 1  // comment out if you don't want the LED to flash on publish

#define REQ_PIN 0        // GPIO0 -> ESP01S / ESP8266 D3 / ESP32 firebeetle
#define TIME_INTERVAL 1  // read interval in seconds

#define IP_STATIC 1  // comment out to use DHCP

const char*    ssid        = "YOUR_SSID";
const char*    password    = "YOUR_PASS";
const char*    mqtt_server = "192.168.0.20";
const uint16_t mqtt_port   = 1883;
const char*    mqtt_client = "ESP-P1";
#define MQTT_RETRY_INTERVAL 5000UL
// const char* mqtt_user   = "user";  // uncomment if broker requires auth
// const char* mqtt_pass   = "pass";

#if defined(IP_STATIC)
IPAddress local_IP(192, 168, 0, 21);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(195, 130, 130, 1);
IPAddress secondaryDNS(195, 130, 131, 1);
#endif

/* END EDIT ITEMS ========================== */

// Meter readings stored as floats — no more *1000 integer tricks
float ECHT = 0;  // electricity consumption high tariff  (kWh)
float ECLT = 0;  // electricity consumption low tariff   (kWh)
float ERHT = 0;  // electricity return high tariff       (kWh)
float ERLT = 0;  // electricity return low tariff        (kWh)
float EAC  = 0;  // electricity actual consumption       (kW)
float EAR  = 0;  // electricity actual return            (kW)

float EL1C = 0;  // L1 actual consumption  (kW)
float EL2C = 0;  // L2 actual consumption  (kW)
float EL3C = 0;  // L3 actual consumption  (kW)
float EL1R = 0;  // L1 actual return       (kW)
float EL2R = 0;  // L2 actual return       (kW)
float EL3R = 0;  // L3 actual return       (kW)

float EL1V = 0;  // L1 actual voltage  (V)
float EL2V = 0;  // L2 actual voltage  (V)
float EL3V = 0;  // L3 actual voltage  (V)
float EL1I = 0;  // L1 actual current  (A)
float EL2I = 0;  // L2 actual current  (A)
float EL3I = 0;  // L3 actual current  (A)

int   ETAR = 0;  // tariff indicator (1=day, 2=night)
float ETAC = 0;  // actual avg 15' consumption   (kW)
float ETPC = 0;  // peak avg 15' consumption     (kW)
long  MEID = 0;  // meter equipment identifier
long  MESN = 0;  // meter serial number
long  METS = 0;  // meter telegram timestamp

// NOTE: VERS removed — 0-0:96.1.4 is the equipment identifier (MEID), not the P1 version.
// The eMUCS P1 version is in the telegram header line (e.g. /FLU5\...) not a COSEM object.

float GAST = 0;  // gas total   (m3)
float WAST = 0;  // water total (m3)
char  DSMR_VER[8] = "unknown";  // P1 version e.g. "50221" = DSMR5.0 eMUCS2.1

char telegram[MAXLINELENGTH];
char jsonPayload[JSONLENGTH]     = "{\"eclt\":0.000,\"echt\":0.000,\"erlt\":0.000,\"erht\":0.000,\"eac\":0.000,\"ear\":0.000,\"el1c\":0.000,\"el2c\":0.000,\"el3c\":0.000,\"el1r\":0.000,\"el2r\":0.000,\"el3r\":0.000,\"el1v\":0.0,\"el2v\":0.0,\"el3v\":0.0,\"el1i\":0.00,\"el2i\":0.00,\"el3i\":0.00,\"etar\":0,\"etpc\":0.000,\"etac\":0.000,\"gast\":0.000,\"wast\":0.000,\"dsmr\":\"unknown\"}";
char prevJsonPayload[JSONLENGTH] = "";  // empty so first real telegram always publishes

const bool outputOnSerial = false;
unsigned int  currentCRC = 0;
unsigned long currentTime = 0;
unsigned long lastTime    = 0;
const unsigned long period = TIME_INTERVAL * 1000;
unsigned long lastMqttRetry  = 0;

WiFiClient   wifiClient;
PubSubClient mqttClient(wifiClient);
