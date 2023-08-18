/* EDIT FOLLOWING ITEMS ========================== */
#define TYPE "T211"				// S211 or T211, functionality not yet implemented
#define GAS 0
#define WATER 0

const char* ota_name = "ESP-P1";
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASS";
const char* host = "192.168.0.20";
const uint16_t port = 1889;

#define REQ_PIN 0              // pin 0 -> ESP01S (GPIO0), ESP8266 (D3), ESP32 firebeetle (GPIO0)
#define TIME_INTERVAL 2        // Read time interval for xx seconds

// Use LED on ESP8266 modules
//#define USE_LED 1 // comment out if you dont want the led to flash when a new message has been sent

// Static IP settings
#define IP_STATIC 1 // comment out if you want to use DHCP

#if defined(IP_STATIC)
IPAddress local_IP(192,168,0,21);
IPAddress gateway(192,168,0,1);
IPAddress subnet(255,255,255,0);
IPAddress primaryDNS(195,130,130,1);
IPAddress secondaryDNS(195,130,131,1);
#endif
/* END EDIT ITEMS ========================== */