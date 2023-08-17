/* EDIT FOLLOWING ITEMS ========================== */
#define TYPE "T211"				// S211 or T211, functionality not yet implemented

const char* ota_name = "ESP-P1";
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASS";
const char* host = "XXX.XXX.XXX.XXX";
const uint16_t port = 1889;

#define REQ_PIN 0              // pin 0 -> ESP01S (GPIO0), ESP8266 (D3), ESP32 firebeetle (GPIO0)
#define TIME_INTERVAL 2        // Read time interval for xx seconds

// Use LED on ESP8266 modules
//#define USE_LED 1 // comment out if you dont want the led to flash when a new message has been sent

// Static IP settings
#define IP_STATIC 1 // comment out if you want to use DHCP

#if defined(IP_STATIC)
IPAddress local_IP(XXX.XXX.XXX.XXX);
IPAddress gateway(XXX.XXX.XXX.XXX);
IPAddress subnet(XXX.XXX.XXX.XXX);
IPAddress primaryDNS(XXX.XXX.XXX.XXX);
IPAddress secondaryDNS(XXX.XXX.XXX.XXX);
#endif
/* END EDIT ITEMS ========================== */