/* Edit by Tim Vancauwenbergh, adaptation from https://github.com/jantenhove/P1-Meter-ESP8266/blob/master/P1Meter.ino
ESP32 / ESP8266 node to pull data every 5 seconds from P1 port of digital meter (Sagecom T211 / S211) which sends a string of variables over TCP.
Can be processed in node-red for example on server */

#include <ArduinoOTA.h>

#if defined(ESP32)
#include <ESPmDNS.h>
#include <WebServer.h>a
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

#include "CRC16.h"
#include "html_index.h"  //Our HTML webpage contents

// DSMR codes: https://maakjemeterslim.be/rails/active_storage/blobs/eyJfcmFpbHMiOnsibWVzc2FnZSI6IkJBaHBBZ0lEIiwiZXhwIjpudWxsLCJwdXIiOiJibG9iX2lkIn19--cdd9b48fd0838e89b177f03b745b23450fd8f53e/e-MUCS_P1_Ed_1_7_1.pdf?disposition=attachment

#include "commons.h"

#define MAXLINELENGTH 512
#define uS_TO_S_FACTOR 1000000

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

long GAST = 0;  // gas total consumption in cubic metres

char sValue[MAXLINELENGTH];
char telegram[MAXLINELENGTH];
const bool outputOnSerial = false;
unsigned int currentCRC = 0;
unsigned long currentTime = 0;
unsigned long lastTime = 0;
const unsigned long period = TIME_INTERVAL * 1000;

WiFiClient wifiClient;

void setup() {
  pinMode(REQ_PIN, OUTPUT);

#if defined(ESP8266)  // for ESP01S this is linked to GPIO2
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);  //active low
#endif

  Serial.begin(115200);

#if defined(ESP32)
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
#endif
  setupWifi();
  setupOTA();
  setupWebServer();

  Serial.println("Setup complete!");
}

void setupWifi() {
#if defined(IP_STATIC)
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }
#endif

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Waiting for Wi-Fi...");
  }
  Serial.print("Wi-Fi connected! IP: ");
  Serial.println(WiFi.localIP());
  setupTCP();
}

void setupTCP() {
  if (!wifiClient.connect(host, port)) {
    Serial.println("Connection to host failed");
    return;
  }
  Serial.println("Connection to host successful!");
}

void setupOTA() {
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_FS
      type = "filesystem";
    }
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.setHostname(ota_name);
  ArduinoOTA.begin();
}

void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/p1_data", data_web);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Web server started!");
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

void handleRoot() {
  String s = MAIN_page;              //Read HTML contents
  server.send(200, "text/html", s);  //Send web page
}

void data_web() {
  server.send(200, "text/plane", sValue);
}

void UpdateValues() {
  sprintf(sValue, "%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d", ECHT, ECLT, ERHT, ERLT, EAC, EAR, EL1C, EL2C, EL3C, EL1R, EL2R, EL3R, EL1V, EL2V, EL3V, EL1I, EL2I, EL3I, ETAR, ETPC, ETAC);

  if (WiFi.status() != WL_CONNECTED)
    setupWifi();

  if (wifiClient.connected()) {
    Serial.println("Sending parsed data:");
    Serial.println(sValue);
    wifiClient.print(sValue);
    wifiClient.flush();  //make sure all data is sent
  } else {
    Serial.println("Connection to server lost, data not sent");
    Serial.println("Trying to setup TCP connection...");
    setupTCP();
  }

#if defined(ESP8266)
#if defined(USE_LED)
  digitalWrite(LED_BUILTIN, HIGH);
#endif
#endif
}

bool isNumber(char* res, int len) {
  for (int i = 0; i < len; i++) {
    if (((res[i] < '0') || (res[i] > '9')) && (res[i] != '.' && res[i] != 0)) {
      return false;
    }
  }
  return true;
}

int FindCharInArrayRev(char array[], char c, int len) {
  for (int i = len - 1; i >= 0; i--) {
    if (array[i] == c) {
      return i;
    }
  }
  return -1;
}

long getValidVal(long valNew, long valOld, long maxDiffer) {
  //check if the incoming value is valid
  if (valOld > 0 && ((valNew - valOld > maxDiffer) && (valOld - valNew > maxDiffer)))
    return valOld;
  return valNew;
}

long getValue(char* buffer, int maxlen) {
  int s = FindCharInArrayRev(buffer, '(', maxlen - 2);
  if (s < 8) return 0;
  if (s > 32) s = 32;
  int l = FindCharInArrayRev(buffer, '*', maxlen - 2) - s - 1;
  if (l < 4) return 0;
  if (l > 12) return 0;
  char res[16];
  memset(res, 0, sizeof(res));

  if (strncpy(res, buffer + s + 1, l)) {
    if (isNumber(res, l)) {
      return (1000 * atof(res));
    }
  }
  return 0;
}

long getValueWithoutStar(char* buffer, int maxlen) {
  int s = FindCharInArrayRev(buffer, '(', maxlen - 2);
  if (s < 8) return 0;
  int e = FindCharInArrayRev(buffer, ')', maxlen - 2);
  if (e < 0 || e <= s) return 0;
  int l = e - s - 1;
  char res[16];
  memset(res, 0, sizeof(res));

  if (strncpy(res, buffer + s + 1, l)) {
    if (isNumber(res, l)) {
      return atof(res);
    }
  }
  return 0;
}

bool decodeTelegram(int len) {
  //need to check for start
  int startChar = FindCharInArrayRev(telegram, '/', len);
  int endChar = FindCharInArrayRev(telegram, '!', len);
  bool validCRCFound = false;
  if (startChar >= 0) {
    //start found. Reset CRC calculation
    currentCRC = CRC16(0x0000, (unsigned char*)telegram + startChar, len - startChar);
    if (outputOnSerial) {
      for (int cnt = startChar; cnt < len - startChar; cnt++)
        Serial.print(telegram[cnt]);
    }

  } else if (endChar >= 0) {
    //add to crc calc
    currentCRC = CRC16(currentCRC, (unsigned char*)telegram + endChar, 1);
    char messageCRC[5];
    strncpy(messageCRC, telegram + endChar + 1, 4);
    messageCRC[4] = 0;  //thanks to HarmOtten (issue 5)
    if (outputOnSerial) {
      for (int cnt = 0; cnt < len; cnt++)
        Serial.print(telegram[cnt]);
    }
    validCRCFound = (strtol(messageCRC, NULL, 16) == currentCRC);
    if (validCRCFound)
      Serial.println("\nVALID CRC FOUND!");
    else
      Serial.println("\n===INVALID CRC FOUND!===");
    currentCRC = 0;
  } else {
    currentCRC = CRC16(currentCRC, (unsigned char*)telegram, len);
    if (outputOnSerial) {
      for (int cnt = 0; cnt < len; cnt++)
        Serial.print(telegram[cnt]);
    }
  }

  if (strncmp(telegram, "1-0:1.8.1", strlen("1-0:1.8.1")) == 0)
    ECHT = getValue(telegram, len);

  if (strncmp(telegram, "1-0:1.8.2", strlen("1-0:1.8.2")) == 0)
    ECLT = getValue(telegram, len);

  if (strncmp(telegram, "1-0:2.8.1", strlen("1-0:2.8.1")) == 0)
    ERHT = getValue(telegram, len);

  if (strncmp(telegram, "1-0:2.8.2", strlen("1-0:2.8.2")) == 0)
    ERLT = getValue(telegram, len);

  if (strncmp(telegram, "1-0:1.7.0", strlen("1-0:1.7.0")) == 0)
    EAC = getValue(telegram, len);

  if (strncmp(telegram, "1-0:2.7.0", strlen("1-0:2.7.0")) == 0)
    EAR = getValue(telegram, len);


  if (strncmp(telegram, "1-0:21.7.0", strlen("1-0:21.7.0")) == 0)
    EL1C = getValue(telegram, len);

  if (strncmp(telegram, "1-0:41.7.0", strlen("1-0:41.7.0")) == 0)
    EL2C = getValue(telegram, len);

  if (strncmp(telegram, "1-0:61.7.0", strlen("1-0:61.7.0")) == 0)
    EL3C = getValue(telegram, len);

  if (strncmp(telegram, "1-0:22.7.0", strlen("1-0:22.7.0")) == 0)
    EL1R = getValue(telegram, len);

  if (strncmp(telegram, "1-0:42.7.0", strlen("1-0:42.7.0")) == 0)
    EL2R = getValue(telegram, len);

  if (strncmp(telegram, "1-0:62.7.0", strlen("1-0:62.7.0")) == 0)
    EL3R = getValue(telegram, len);


  if (strncmp(telegram, "1-0:32.7.0", strlen("1-0:32.7.0")) == 0)
    EL1V = getValue(telegram, len);

  if (strncmp(telegram, "1-0:52.7.0", strlen("1-0:52.7.0")) == 0)
    EL2V = getValue(telegram, len);

  if (strncmp(telegram, "1-0:72.7.0", strlen("1-0:72.7.0")) == 0)
    EL3V = getValue(telegram, len);

  if (strncmp(telegram, "1-0:31.7.0", strlen("1-0:31.7.0")) == 0)
    EL1I = getValue(telegram, len);

  if (strncmp(telegram, "1-0:51.7.0", strlen("1-0:51.7.0")) == 0)
    EL2I = getValue(telegram, len);

  if (strncmp(telegram, "1-0:71.7.0", strlen("1-0:71.7.0")) == 0)
    EL3I = getValue(telegram, len);


  if (strncmp(telegram, "0-0:96.14.0", strlen("0-0:96.14.0")) == 0)
    ETAR = getValueWithoutStar(telegram, len);

  if (strncmp(telegram, "0-0:96.1.4", strlen("0-0:96.1.4")) == 0)
    MEID = getValueWithoutStar(telegram, len);

  if (strncmp(telegram, "0-0:96.1.1", strlen("0-0:96.1.1")) == 0)
    MESN = getValueWithoutStar(telegram, len);

  if (strncmp(telegram, "0-0:1.0.0", strlen("0-0:1.0.0")) == 0)
    METS = getValueWithoutStar(telegram, len);

  if (strncmp(telegram, "1-0:1.4.0", strlen("1-0:1.4.0")) == 0)
    ETAC = getValue(telegram, len);

  if (strncmp(telegram, "1-0:1.6.0", strlen("1-0:1.6.0")) == 0)
    ETPC = getValue(telegram, len);


  /*if (strncmp(telegram, "0-1:24.2.3", strlen("0-1:24.2.3")) == 0)
    GAST = getValue(telegram, len);
  */
  return validCRCFound;
}

void readTelegram() {
#if defined(ESP32)
  if (Serial2.available()) {
#else
  if (Serial.available()) {
#endif
    digitalWrite(REQ_PIN, LOW);
    memset(telegram, 0, sizeof(telegram));
#if defined(ESP32)
    while (Serial2.available()) {
      int len = Serial2.readBytesUntil('\n', telegram, MAXLINELENGTH);
#else
    while (Serial.available()) {
      int len = Serial.readBytesUntil('\n', telegram, MAXLINELENGTH);
#endif
      telegram[len] = '\n';
      telegram[len + 1] = 0;
      yield();
      if (decodeTelegram(len + 1)) {
        UpdateValues();
      }
    }
  }
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();

  currentTime = millis();

  if ((currentTime - lastTime) >= period) {
    lastTime = currentTime;
    digitalWrite(REQ_PIN, HIGH);
#if defined(ESP8266)
#if defined(USE_LED)
    digitalWrite(LED_BUILTIN, LOW);
#endif
#endif
  }

  readTelegram();
}