/* Edit by Tim Vancauwenbergh, adaptation from https://github.com/jantenhove/P1-Meter-ESP8266/blob/master/P1Meter.ino
   ESP32 / ESP8266 — reads P1 port of Belgian digital meter (Sagecom T211/S211, eMUCS-P1 v2.1)
   Publishes a single JSON payload to ESPD1-P1/data on each changed telegram.
   Also serves a local web page at / with a live data table. */

// eMUCS-P1 v2.1 spec:
// https://partner.fluvius.be/sites/fluvius/files/2025-09/digital-metering-system-emucs-p1-v2-1.pdf

#include "commons.h"
#include "CRC16.h"
#include "html_index.h"

// ─── Setup ────────────────────────────────────────────────────────────────────

void setup() {
  pinMode(REQ_PIN, OUTPUT);

#if defined(ESP8266)
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);  // active low
#endif

  Serial.begin(115200);

#if defined(ESP32)
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
#endif

  setupWifi();
  setupMQTT();
  setupWebServer();

  Serial.println("Setup complete!");
}

// ─── Wi-Fi ────────────────────────────────────────────────────────────────────

void setupWifi() {
#if defined(IP_STATIC)
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS))
    Serial.println("STA Failed to configure");
#endif
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Waiting for Wi-Fi...");
  }
  Serial.print("Wi-Fi connected! IP: ");
  Serial.println(WiFi.localIP());
}

// ─── MQTT ─────────────────────────────────────────────────────────────────────

void setupMQTT() {
  mqttClient.setBufferSize(512);  // default 256 is too small for the JSON payload
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttConnect();
}

void mqttConnect() {
  if (mqttClient.connected()) return;
  unsigned long now = millis();
  if (now - lastMqttRetry < MQTT_RETRY_INTERVAL) return;
  lastMqttRetry = now;
  Serial.print("Connecting to MQTT...");
  // swap for: mqttClient.connect(mqtt_client, mqtt_user, mqtt_pass)  if auth needed
  if (mqttClient.connect(mqtt_client)) {
    Serial.println(" connected!");
  } else {
    Serial.print(" failed, rc=");
    Serial.println(mqttClient.state());
  }
}

// ─── Web server ───────────────────────────────────────────────────────────────

void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/p1_data", data_web);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Web server started!");
}

void handleNotFound() { server.send(404, "text/plain", "Not found"); }

void handleRoot() {
  String s = MAIN_page;
  server.send(200, "text/html", s);
}

void data_web() {
  // Serve the JSON payload directly — the web UI can use this too if needed
  server.send(200, "application/json", jsonPayload);
}

// ─── Publish ──────────────────────────────────────────────────────────────────

void UpdateValues() {
  // Build a compact JSON payload with all meter values
  // floats are stored at their natural precision — no *1000 conversion needed
  snprintf(jsonPayload, sizeof(jsonPayload),
    "{"
    "\"eclt\":%.3f,\"echt\":%.3f,"
    "\"erlt\":%.3f,\"erht\":%.3f,"
    "\"eac\":%.3f,\"ear\":%.3f,"
    "\"el1c\":%.3f,\"el2c\":%.3f,\"el3c\":%.3f,"
    "\"el1r\":%.3f,\"el2r\":%.3f,\"el3r\":%.3f,"
    "\"el1v\":%.1f,\"el2v\":%.1f,\"el3v\":%.1f,"
    "\"el1i\":%.2f,\"el2i\":%.2f,\"el3i\":%.2f,"
    "\"etar\":%d,"
    "\"etpc\":%.3f,\"etac\":%.3f,"
    "\"gast\":%.3f,\"wast\":%.3f,"
    "\"dsmr\":\"%s\""
    "}",
    ECLT, ECHT, ERLT, ERHT,
    EAC,  EAR,
    EL1C, EL2C, EL3C,
    EL1R, EL2R, EL3R,
    EL1V, EL2V, EL3V,
    EL1I, EL2I, EL3I,
    ETAR,
    ETPC, ETAC,
    GAST, WAST,
    DSMR_VER
  );

  // Change detection — skip publish if nothing changed since last telegram
  if (strcmp(jsonPayload, prevJsonPayload) == 0) {
    Serial.println("No change, skipping publish.");
    return;
  }
  memcpy(prevJsonPayload, jsonPayload, sizeof(jsonPayload));

  // Reconnect if needed
  if (WiFi.status() != WL_CONNECTED) setupWifi();
  if (!mqttClient.connected()) mqttConnect();

  if (mqttClient.connected()) {
    bool ok = mqttClient.publish("ESPD1-P1/data", jsonPayload, true);  // retain=true
    Serial.print("MQTT publish ");
    Serial.println(ok ? "OK" : "FAILED");
  } else {
    Serial.println("MQTT not connected, skipping publish.");
  }

#if defined(ESP8266)
#if defined(USE_LED)
  digitalWrite(LED_BUILTIN, HIGH);
#endif
#endif
}

// ─── Parsing helpers ──────────────────────────────────────────────────────────

bool isNumber(char* res, int len) {
  for (int i = 0; i < len; i++) {
    if (((res[i] < '0') || (res[i] > '9')) && res[i] != '.' && res[i] != 0)
      return false;
  }
  return true;
}

int FindCharInArrayRev(char array[], char c, int len) {
  for (int i = len - 1; i >= 0; i--)
    if (array[i] == c) return i;
  return -1;
}

int FindCharInArray(char array[], char c, int len) {
  for (int i = 0; i < len; i++)
    if (array[i] == c) return i;
  return -1;
}

// Returns float directly — no *1000 storage trick needed anymore
float getValue(char* buffer, int maxlen) {
  int s = FindCharInArrayRev(buffer, '(', maxlen - 2);
  if (s < 8) return 0;
  if (s > 32) s = 32;
  int l = FindCharInArrayRev(buffer, '*', maxlen - 2) - s - 1;
  if (l < 4 || l > 12) return 0;
  char res[16];
  memset(res, 0, sizeof(res));
  strncpy(res, buffer + s + 1, l);
  return isNumber(res, l) ? atof(res) : 0;
}

// For 1-0:1.6.0 peak demand: format is (value*kW)(timestamp)
// Must find the FIRST '(' not the last — otherwise lands on the timestamp group
float getFirstValue(char* buffer, int maxlen) {
  int s = FindCharInArray(buffer, '(', maxlen - 2);
  if (s < 8) return 0;
  int l = FindCharInArrayRev(buffer, '*', maxlen - 2) - s - 1;
  if (l < 1 || l > 12) return 0;
  char res[16];
  memset(res, 0, sizeof(res));
  strncpy(res, buffer + s + 1, l);
  return isNumber(res, l) ? atof(res) : 0;
}

// For values without a unit marker (*), e.g. tariff indicator
long getValueWithoutStar(char* buffer, int maxlen) {
  int s = FindCharInArrayRev(buffer, '(', maxlen - 2);
  if (s < 8) return 0;
  int e = FindCharInArrayRev(buffer, ')', maxlen - 2);
  if (e < 0 || e <= s) return 0;
  int l = e - s - 1;
  char res[16];
  memset(res, 0, sizeof(res));
  strncpy(res, buffer + s + 1, l);
  return isNumber(res, l) ? (long)atof(res) : 0;
}

// ─── Telegram decoder ─────────────────────────────────────────────────────────

bool decodeTelegram(int len) {
  int startChar = FindCharInArrayRev(telegram, '/', len);
  int endChar   = FindCharInArrayRev(telegram, '!', len);
  bool validCRCFound = false;

  if (startChar >= 0) {
    currentCRC = CRC16(0x0000, (unsigned char*)telegram + startChar, len - startChar);
    if (outputOnSerial)
      for (int i = startChar; i < len; i++) Serial.print(telegram[i]);

  } else if (endChar >= 0) {
    currentCRC = CRC16(currentCRC, (unsigned char*)telegram + endChar, 1);
    char messageCRC[5];
    strncpy(messageCRC, telegram + endChar + 1, 4);
    messageCRC[4] = 0;
    if (outputOnSerial)
      for (int i = 0; i < len; i++) Serial.print(telegram[i]);
    validCRCFound = (strtol(messageCRC, NULL, 16) == currentCRC);
    Serial.println(validCRCFound ? "\nVALID CRC FOUND!" : "\n===INVALID CRC FOUND!===");
    currentCRC = 0;

  } else {
    currentCRC = CRC16(currentCRC, (unsigned char*)telegram, len);
    if (outputOnSerial)
      for (int i = 0; i < len; i++) Serial.print(telegram[i]);
  }

  // DSMR version — OBIS 0-0:96.1.4, octet-string A5, value e.g. "50221" = DSMR5.0 eMUCS2.1
  if (strncmp(telegram, "0-0:96.1.4", 10) == 0) {
    int s = FindCharInArrayRev(telegram, '(', len - 2);
    int e = FindCharInArrayRev(telegram, ')', len - 2);
    if (s >= 0 && e > s) {
      int l = e - s - 1;
      if (l > 0 && l < 8) {
        memset(DSMR_VER, 0, sizeof(DSMR_VER));
        strncpy(DSMR_VER, telegram + s + 1, l);
      }
    }
  }

  // Cumulative energy
  if (strncmp(telegram, "1-0:1.8.1",  9)  == 0) ECHT = getValue(telegram, len);  // tariff 1 = day in Belgium
  if (strncmp(telegram, "1-0:1.8.2",  9)  == 0) ECLT = getValue(telegram, len);  // tariff 2 = night in Belgium
  if (strncmp(telegram, "1-0:2.8.1",  9)  == 0) ERHT = getValue(telegram, len);  // tariff 1 = day in Belgium
  if (strncmp(telegram, "1-0:2.8.2",  9)  == 0) ERLT = getValue(telegram, len);  // tariff 2 = night in Belgium

  // Actual power
  if (strncmp(telegram, "1-0:1.7.0",  9)  == 0) EAC  = getValue(telegram, len);
  if (strncmp(telegram, "1-0:2.7.0",  9)  == 0) EAR  = getValue(telegram, len);
  if (strncmp(telegram, "1-0:21.7.0", 10) == 0) EL1C = getValue(telegram, len);
  if (strncmp(telegram, "1-0:41.7.0", 10) == 0) EL2C = getValue(telegram, len);
  if (strncmp(telegram, "1-0:61.7.0", 10) == 0) EL3C = getValue(telegram, len);
  if (strncmp(telegram, "1-0:22.7.0", 10) == 0) EL1R = getValue(telegram, len);
  if (strncmp(telegram, "1-0:42.7.0", 10) == 0) EL2R = getValue(telegram, len);
  if (strncmp(telegram, "1-0:62.7.0", 10) == 0) EL3R = getValue(telegram, len);

  // Voltage and current
  if (strncmp(telegram, "1-0:32.7.0", 10) == 0) EL1V = getValue(telegram, len);
  if (strncmp(telegram, "1-0:52.7.0", 10) == 0) EL2V = getValue(telegram, len);
  if (strncmp(telegram, "1-0:72.7.0", 10) == 0) EL3V = getValue(telegram, len);
  if (strncmp(telegram, "1-0:31.7.0", 10) == 0) EL1I = getValue(telegram, len);
  if (strncmp(telegram, "1-0:51.7.0", 10) == 0) EL2I = getValue(telegram, len);
  if (strncmp(telegram, "1-0:71.7.0", 10) == 0) EL3I = getValue(telegram, len);

  // Tariff and demand
  if (strncmp(telegram, "0-0:96.14.0", 11) == 0) ETAR = (int)getValueWithoutStar(telegram, len);
  if (strncmp(telegram, "1-0:1.4.0",   9)  == 0) ETAC = getValue(telegram, len);
  if (strncmp(telegram, "1-0:1.6.0",   9)  == 0) ETPC = getValue(telegram, len);  // (timestamp)(value*kW)

  // Meter identifiers (not published to MQTT but kept for future use)
  if (strncmp(telegram, "0-0:96.1.4", 10) == 0) MEID = getValueWithoutStar(telegram, len);
  if (strncmp(telegram, "0-0:96.1.1", 10) == 0) MESN = getValueWithoutStar(telegram, len);
  if (strncmp(telegram, "0-0:1.0.0",  9)  == 0) METS = getValueWithoutStar(telegram, len);

  // Gas and water
  if (strncmp(telegram, "0-1:24.2.3", 10) == 0) GAST = getValue(telegram, len);
  if (strncmp(telegram, "0-2:24.2.1", 10) == 0) WAST = getValue(telegram, len);

  return validCRCFound;
}

// ─── Serial reader ────────────────────────────────────────────────────────────

void readTelegram() {
#if defined(ESP32)
  if (!Serial2.available()) return;
#else
  if (!Serial.available()) return;
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
    telegram[len]     = '\n';
    telegram[len + 1] = 0;
    yield();
    if (decodeTelegram(len + 1)) UpdateValues();
  }
}

// ─── Main loop ────────────────────────────────────────────────────────────────

void loop() {
  server.handleClient();
  mqttClient.loop();

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

  if (WiFi.status() == WL_CONNECTED && !mqttClient.connected()) {
    mqttConnect();
  }
}
