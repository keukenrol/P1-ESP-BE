// ConfigPortal.h - first-boot hotspot + web form (ESP8266 and ESP32)
// Same firmware on every device: no credentials in code.
//
//  - No saved config, or WiFi join fails (15 s)  -> open AP "P1-ESP-XXXXXX" + captive portal
//  - Long-press GPIO0 (5 s) while running        -> reboots into the portal
//  - Form: WiFi SSID/password, DHCP or static IP (ip/gw/mask/dns), MQTT server/port/topic
//  - Settings are stored in EEPROM (flash), then the device reboots and joins the network.
//
// NOTE: GPIO0 must be HIGH at power-up/reset (low = flash mode). That is why the
// button is only evaluated while the sketch is running (loopButton), not at boot.
#pragma once
#include <Arduino.h>
#include <DNSServer.h>
#include <EEPROM.h>
#if defined(ESP32)
  #include <WiFi.h>
  #include <WebServer.h>
  typedef WebServer PortalWebServer;
#else
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  typedef ESP8266WebServer PortalWebServer;
#endif

#define PORTAL_DEFAULT_TOPIC "ESPD1-P1/data"

struct NetConfig {
  uint32_t magic;
  uint8_t  useDhcp;       // 1 = DHCP, 0 = static
  uint8_t  forcePortal;   // set by loopButton(); cleared on next boot
  char     ssid[33];
  char     pass[65];
  char     server[65];    // MQTT broker hostname or IP
  uint16_t port;          // MQTT port
  char     topic[65];     // MQTT topic
  char     ip[16];
  char     gw[16];
  char     mask[16];
  char     dns[16];
};

class ConfigPortal {
 public:
  NetConfig cfg;

  // Returns true once connected to the configured WiFi.
  // If the portal is needed it blocks until the user saved (then the device reboots).
  bool begin(uint32_t staTimeoutMs = 15000, uint8_t buttonPin = 0) {
    btnPin = buttonPin;
    pinMode(btnPin, INPUT_PULLUP);
    EEPROM.begin(sizeof(NetConfig));
    bool valid = load();

    if (valid && cfg.forcePortal) {
      cfg.forcePortal = 0;
      save();
      runPortal();
    }
    if (valid && connectSta(staTimeoutMs)) return true;
    runPortal();
    return false;
  }

  // Call from loop(): holding the button for holdMs reboots into the portal.
  void loopButton(uint32_t holdMs = 5000) {
    if (digitalRead(btnPin) == LOW) {
      if (!pressed) { pressed = true; pressedAt = millis(); }
      else if (millis() - pressedAt >= holdMs) {
        cfg.forcePortal = 1;
        save();
        ESP.restart();
      }
    } else {
      pressed = false;
    }
  }

  // "P1-ESP-XXXXXX" - unique per chip; used for AP name and MQTT client id
  String deviceId() {
    char b[20];
    snprintf(b, sizeof(b), "P1-ESP-%06X", (unsigned)chipId());
    return String(b);
  }

  bool load() {
    EEPROM.get(0, cfg);
    if (cfg.magic != MAGIC) {
      memset(&cfg, 0, sizeof(cfg));
      cfg.useDhcp = 1;
      cfg.port = 1883;
      strcpy(cfg.topic, PORTAL_DEFAULT_TOPIC);
      return false;
    }
    cfg.ssid[32] = cfg.pass[64] = cfg.server[64] = cfg.topic[64] = 0;
    cfg.ip[15] = cfg.gw[15] = cfg.mask[15] = cfg.dns[15] = 0;
    if (!cfg.topic[0]) strcpy(cfg.topic, PORTAL_DEFAULT_TOPIC);
    if (!cfg.port) cfg.port = 1883;
    return true;
  }

  void save() {
    cfg.magic = MAGIC;
    EEPROM.put(0, cfg);
    EEPROM.commit();
  }

 private:
  static const uint32_t MAGIC = 0x50314347;
  PortalWebServer web{80};
  DNSServer dns;
  uint8_t  btnPin = 0;
  bool     pressed = false;
  uint32_t pressedAt = 0;
  uint32_t lastActivity = 0;

  uint32_t chipId() {
#if defined(ESP32)
    return (uint32_t)ESP.getEfuseMac();
#else
    return ESP.getChipId();
#endif
  }

  bool connectSta(uint32_t timeoutMs) {
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    if (!cfg.useDhcp) {
      IPAddress ip, gw, mask, d;
      if (ip.fromString(cfg.ip) && gw.fromString(cfg.gw) && mask.fromString(cfg.mask)) {
        if (!d.fromString(cfg.dns)) d = gw;
        WiFi.config(ip, gw, mask, d);
      }
    }
    WiFi.begin(cfg.ssid, cfg.pass);
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeoutMs) delay(100);
    return WiFi.status() == WL_CONNECTED;
  }

  static String esc(const char* s) {
    String o;
    for (; *s; s++) {
      switch (*s) {
        case '&': o += F("&amp;"); break;
        case '<': o += F("&lt;"); break;
        case '>': o += F("&gt;"); break;
        case '"': o += F("&quot;"); break;
        default: o += *s;
      }
    }
    return o;
  }

  static String field(const char* label, const char* name, const char* val, const char* extra = "") {
    String s = F("<label>");
    s += label;
    s += F("<input name='");
    s += name;
    s += F("' value=\"");
    s += esc(val);
    s += F("\" ");
    s += extra;
    s += F("></label>");
    return s;
  }

  void handleRoot() {
    lastActivity = millis();
    String h = F("<!doctype html><html><head><meta charset='utf-8'>"
                 "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                 "<title>P1 ESP setup</title><style>"
                 "body{font-family:sans-serif;max-width:420px;margin:1em auto;padding:0 1em}"
                 "label{display:block;margin:.7em 0}input,select{width:100%;padding:.5em;box-sizing:border-box}"
                 "small{color:#666}button{padding:.7em 1.2em;margin-top:1em}</style></head><body>"
                 "<h2>P1 ESP setup</h2><form method='POST' action='/save'>"
                 "<h3>WiFi</h3>");
    h += F("<label>Network (SSID)<input name='ssid' list='nets' maxlength='32' required value=\"");
    h += esc(cfg.ssid);
    h += F("\"></label><datalist id='nets'>");
    int n = WiFi.scanComplete();
    for (int i = 0; i < n; i++) {
      h += F("<option value=\"");
      h += esc(WiFi.SSID(i).c_str());
      h += F("\">");
    }
    h += F("</datalist><label>Password<input name='pass' type='password' maxlength='64' autocomplete='off'>"
           "<small>Leave blank to keep the current password</small></label>");
    h += F("<label>IP mode<select name='dhcp' id='dhcp' onchange='t()'><option value='1'");
    if (cfg.useDhcp) h += F(" selected");
    h += F(">Dynamic (DHCP)</option><option value='0'");
    if (!cfg.useDhcp) h += F(" selected");
    h += F(">Static</option></select></label><div id='st'>");
    h += field("IP address", "ip", cfg.ip);
    h += field("Gateway", "gw", cfg.gw);
    h += field("Subnet mask", "mask", cfg.mask[0] ? cfg.mask : "255.255.255.0");
    h += field("DNS", "dns", cfg.dns);
    h += F("</div><h3>MQTT</h3>");
    h += field("Server (host or IP)", "server", cfg.server, "maxlength='64' required");
    h += field("Port", "port", String(cfg.port).c_str(), "type='number' min='1' max='65535'");
    h += field("Topic", "topic", cfg.topic, "maxlength='64'");
    h += F("<button type='submit'>Save &amp; reboot</button></form>"
           "<script>function t(){document.getElementById('st').style.display="
           "document.getElementById('dhcp').value=='1'?'none':'block'}t()</script>"
           "</body></html>");
    web.send(200, "text/html", h);
  }

  static void copy(char* dst, size_t sz, const String& v) {
    strncpy(dst, v.c_str(), sz - 1);
    dst[sz - 1] = 0;
  }

  void handleSave() {
    lastActivity = millis();
    String ssid = web.arg("ssid");
    String server = web.arg("server");
    ssid.trim();
    server.trim();
    if (!ssid.length() || !server.length()) {
      web.send(400, "text/plain", "SSID and server are required");
      return;
    }
    bool dhcp = web.arg("dhcp") != "0";
    if (!dhcp) {
      IPAddress t;
      if (!t.fromString(web.arg("ip")) || !t.fromString(web.arg("gw")) || !t.fromString(web.arg("mask"))) {
        web.send(400, "text/plain", "Invalid static IP, gateway or mask");
        return;
      }
    }
    // keep the old password if the field is left blank and the SSID is unchanged
    String np = web.arg("pass");
    if (np.length() || ssid != String(cfg.ssid)) copy(cfg.pass, sizeof(cfg.pass), np);
    copy(cfg.ssid, sizeof(cfg.ssid), ssid);
    copy(cfg.server, sizeof(cfg.server), server);
    long p = web.arg("port").toInt();
    cfg.port = (p >= 1 && p <= 65535) ? (uint16_t)p : 1883;
    String topic = web.arg("topic");
    topic.trim();
    copy(cfg.topic, sizeof(cfg.topic), topic.length() ? topic : String(PORTAL_DEFAULT_TOPIC));
    cfg.useDhcp = dhcp;
    copy(cfg.ip, sizeof(cfg.ip), web.arg("ip"));
    copy(cfg.gw, sizeof(cfg.gw), web.arg("gw"));
    copy(cfg.mask, sizeof(cfg.mask), web.arg("mask"));
    copy(cfg.dns, sizeof(cfg.dns), web.arg("dns"));
    cfg.forcePortal = 0;
    save();
    web.send(200, "text/html",
             F("<meta name='viewport' content='width=device-width,initial-scale=1'>"
               "<h3>Saved.</h3><p>The device is rebooting and will join your network.</p>"));
    delay(1500);
    ESP.restart();
  }

  void runPortal() {
    WiFi.persistent(false);
    WiFi.mode(WIFI_AP_STA);   // STA side is needed for scanning
    WiFi.scanNetworks(true);  // async
    WiFi.softAP(deviceId().c_str());  // open AP; pass a 2nd arg (min 8 chars) for a password

    dns.start(53, "*", WiFi.softAPIP());  // captive portal: every name resolves to us
    web.on("/", [this]() { handleRoot(); });
    web.on("/save", HTTP_POST, [this]() { handleSave(); });
    web.onNotFound([this]() {  // phones probe random URLs; send them to the form
      web.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
      web.send(302, "text/plain", "");
    });
    web.begin();

    const uint32_t IDLE_TIMEOUT = 5UL * 60UL * 1000UL;
    bool hadConfig = (cfg.ssid[0] != 0);
    lastActivity = millis();
    for (;;) {
      dns.processNextRequest();
      web.handleClient();
      yield();
      if (WiFi.softAPgetStationNum() > 0) lastActivity = millis();  // someone is connected
      // If a config exists (router was just offline) retry it after 5 idle minutes
      if (hadConfig && millis() - lastActivity > IDLE_TIMEOUT) ESP.restart();
    }
  }
};
