#include <FS.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <WebSocketsServer.h>

#include "powerstep01.h"
#include "wifistepper.h"

#define RESET_PIN         (5)
#define RESET_TIMEOUT     (3000)
#define MOTOR_ADCCOEFF    (2.65625)

#define FNAME_WIFICFG     "/wificfg.json"
#define FNAME_BROWSERCFG  "/browsercfg.json"
#define FNAME_MOTORCFG    "/motorcfg.json"

#define LEN_HOSTNAME  24
#define LEN_USERNAME  64
#define LEN_SSID      32
#define LEN_PASSWORD  64
#define LEN_IP        16

ESP8266WebServer server(80);
WebSocketsServer websocket(81);
StaticJsonBuffer<2048> jsonbuf;
StaticJsonBuffer<1024> configbuf;

volatile bool flag_reboot = false;

typedef enum {
  M_ACCESSPOINT = 0x0,
  M_STATION = 0x1
} wifi_mode;

struct {
  wifi_mode mode;
  char ap_ssid[LEN_SSID];
  char ap_password[LEN_PASSWORD];
  bool ap_encryption;
  int ap_channel;
  bool ap_hidden;
  char stn_ssid[LEN_SSID];
  char stn_password[LEN_PASSWORD];
  bool stn_encryption;
  char stn_forceip[LEN_IP];
  bool stn_revertap;
  char ip[LEN_IP];
} wificfg = {
  .mode = M_ACCESSPOINT,
  .ap_ssid = {'w','s','x','1','0','0','-','a','p',0},
  .ap_password = {0},
  .ap_encryption = false,
  .ap_channel = 1,
  .ap_hidden = false,
  .stn_ssid = {0},
  .stn_password = {0},
  .stn_encryption = false,
  .stn_forceip = {0},
  .stn_revertap = true,
  .ip = {0}
};

struct {
  bool http_enabled;
  bool https_enabled;
  bool mdns_enabled;
  char mdns_hostname[LEN_HOSTNAME];
  bool auth_enabled;
  char auth_username[LEN_USERNAME];
  char auth_password[LEN_PASSWORD];
  bool ota_enabled;
  char ota_hostname[LEN_HOSTNAME];
  char ota_password[LEN_PASSWORD];
} browsercfg = {
  .http_enabled = true,
  .https_enabled = false,
  .mdns_enabled = true,
  .mdns_hostname = {'w','s','x','1','0','0',0},
  .auth_enabled = false,
  .auth_username = {0},
  .auth_password = {0},
  .ota_enabled = true,
  .ota_hostname = {'w','s','x','1','0','0','-','o','t','a',0},
  .ota_password = {0}
};

struct {
  ps_mode mode;
  ps_stepsize stepsize;
  float maxspeed, minspeed;
  float accel, decel;
  float kthold, ktrun, ktaccel, ktdecel;
  float fsspeed;
  bool fsboost;
  bool reverse;
} motorcfg = {
  .mode = MODE_VOLTAGE,
  .stepsize = STEP_128,
  .maxspeed = 10000.0,
  .minspeed = 0.0,
  .accel = 50.0,
  .decel = 50.0,
  .kthold = 0.25,
  .ktrun = 0.5,
  .ktaccel = 0.8,
  .ktdecel = 0.8,
  .fsspeed = 1000.0,
  .fsboost = true,
  .reverse = false
};



struct {
  int pos, mark;
  float speed;
  bool busy;
} statecache = { 0 };

void wificfg_save() {
  JsonObject& root = jsonbuf.createObject();
  root["mode"] = json_serialize(wificfg.mode);
  root["ap_ssid"] = wificfg.ap_ssid;
  root["ap_password"] = wificfg.ap_password;
  root["ap_encryption"] = wificfg.ap_encryption;
  root["ap_channel"] = wificfg.ap_channel;
  root["ap_hidden"] = wificfg.ap_hidden;
  root["stn_ssid"] = wificfg.stn_ssid;
  root["stn_password"] = wificfg.stn_password;
  root["stn_encryption"] = wificfg.stn_encryption;
  root["stn_forceip"] = wificfg.stn_forceip;
  root["stn_revertap"] = wificfg.stn_revertap;
  JsonVariant v = root;
  File cfg = SPIFFS.open(FNAME_WIFICFG, "w");
  root.printTo(cfg);
  jsonbuf.clear();
}

void browsercfg_save() {
  JsonObject& root = jsonbuf.createObject();
  root["http_enabled"] = browsercfg.http_enabled;
  root["https_enabled"] = browsercfg.https_enabled;
  root["mdns_enabled"] = browsercfg.mdns_enabled;
  root["mdns_hostname"] = browsercfg.mdns_hostname;
  root["auth_enabled"] = browsercfg.auth_enabled;
  root["auth_username"] = browsercfg.auth_username;
  root["auth_password"] = browsercfg.auth_password;
  root["ota_enabled"] = browsercfg.ota_enabled;
  root["ota_hostname"] = browsercfg.ota_hostname;
  root["ota_password"] = browsercfg.ota_password;
  JsonVariant v = root;
  File cfg = SPIFFS.open(FNAME_BROWSERCFG, "w");
  root.printTo(cfg);
  jsonbuf.clear();
}

void motorcfg_update() {
  ps_setsync(SYNC_BUSY);
  ps_setmode(motorcfg.mode);
  ps_setstepsize(motorcfg.stepsize);
  ps_setmaxspeed(motorcfg.maxspeed);
  ps_setminspeed(motorcfg.minspeed, true);
  ps_setaccel(motorcfg.accel);
  ps_setdecel(motorcfg.decel);
  ps_setfullstepspeed(motorcfg.fsspeed, motorcfg.fsboost);
  
  ps_setslewrate(SR_520);

  ps_setocd(500, true);
  ps_vm_setpwmfreq(0, 1);                   // V
  //ps_cm_setpredict(true);                   // C
  ps_setvoltcomp(false);
  ps_setswmode(SW_USER);
  ps_setclocksel(CLK_INT16);

  ps_setktvals(motorcfg.kthold, motorcfg.ktrun, motorcfg.ktaccel, motorcfg.ktdecel);
  ps_setalarmconfig(true, true, true, true);
}

void motorcfg_save() {
  JsonObject& root = jsonbuf.createObject();
  root["mode"] = json_serialize(motorcfg.mode);
  root["stepsize"] = json_serialize(motorcfg.stepsize);
  root["maxspeed"] = motorcfg.maxspeed;
  root["minspeed"] = motorcfg.minspeed;
  root["accel"] = motorcfg.accel;
  root["decel"] = motorcfg.decel;
  root["kthold"] = motorcfg.kthold;
  root["ktrun"] = motorcfg.ktrun;
  root["ktaccel"] = motorcfg.ktaccel;
  root["ktdecel"] = motorcfg.ktdecel;
  root["fsspeed"] = motorcfg.fsspeed;
  root["fsboost"] = motorcfg.fsboost;
  root["reverse"] = motorcfg.reverse;
  JsonVariant v = root;
  File cfg = SPIFFS.open(FNAME_MOTORCFG, "w");
  root.printTo(cfg);
  jsonbuf.clear();
}

ps_direction motorcfg_dir(ps_direction d) {
  if (!motorcfg.reverse)  return d;
  switch (d) {
    case FWD: return REV;
    case REV: return FWD;
  }
}

wifi_mode parse_wifimode(const char * m) {
  if (strcmp(m, "station") == 0)  return M_STATION;
  else                            return M_ACCESSPOINT;
}

int parse_channel(const String& c, int d) {
  int i = c.toInt();
  return i > 0 && i <= 13? i : d;
}

ps_mode parse_motormode(const String& m, ps_mode d) {
  if (m == "voltage")       return MODE_VOLTAGE;
  else if (m == "current")  return MODE_CURRENT;
  else                      return d;
}

ps_stepsize parse_stepsize(int s, ps_stepsize d) {
  switch (s) {
    case 1:   return STEP_1;
    case 2:   return STEP_2;
    case 4:   return STEP_4;
    case 8:   return STEP_8;
    case 16:  return STEP_16;
    case 32:  return STEP_32;
    case 64:  return STEP_64;
    case 128: return STEP_128;
    default:  return d;
  }
}

ps_direction parse_direction(const String& s, ps_direction d) {
  if (s == "forward")       return FWD;
  else if (s == "reverse")  return REV;
  else                      return d;
}

#define json_ok()         "{\"status\":\"ok\"}"
#define json_error(msg)   "{\"status\":\"error\",\"message\":\"" msg "\"}"

void json_addheaders() {
  server.sendHeader("Access-Control-Allow-Credentials", "true");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST");
  server.sendHeader("Access-Control-Allow-Headers", "Authorization, application/json");
}

const char * json_serialize(wifi_mode m) {
  switch (m) {
    case M_ACCESSPOINT: return "accesspoint";
    case M_STATION:     return "station";
    default:            return "";
  }
}

const char * json_serialize(ps_mode m) {
  switch (m) {
    case MODE_VOLTAGE:  return "voltage";
    case MODE_CURRENT:  return "current";
    default:            return "";
  }
}

int json_serialize(ps_stepsize s) {
  switch (s) {
    case STEP_1:    return 1;
    case STEP_2:    return 2;
    case STEP_4:    return 4;
    case STEP_8:    return 8;
    case STEP_16:   return 16;
    case STEP_32:   return 32;
    case STEP_64:   return 64;
    case STEP_128:  return 128;
    default:        return 0;
  }
}

const char * json_serialize(ps_direction d) {
  switch (d) {
    case FWD:   return "forward";
    case REV:   return "reverse";
    default:    return "";
  }
}

const char * json_serialize(ps_movement m) {
  switch (m) {
    case M_STOPPED:     return "idle";
    case M_ACCEL:       return "accelerating";
    case M_DECEL:       return "decelerating";
    case M_CONSTSPEED:  return "spinning";
    default:            return "";
  }
}


void jsonwifi_init() {
  server.on("/api/wifi/scan", [](){
    json_addheaders();
    int n = WiFi.scanNetworks();
    JsonObject& root = jsonbuf.createObject();
    JsonArray& networks = root.createNestedArray("networks");
    for (int i = 0; i < n; ++i) {
      JsonObject& network = jsonbuf.createObject();
      network["ssid"] = WiFi.SSID(i);
      network["rssi"] = WiFi.RSSI(i);
      network["encryption"] = WiFi.encryptionType(i) != ENC_TYPE_NONE;
      networks.add(network);
    }
    root["status"] = "ok";
    JsonVariant v = root;
    server.send(200, "application/json", v.as<String>());
    jsonbuf.clear();
  });
  server.on("/api/wifi/get", [](){
    json_addheaders();
    JsonObject& root = jsonbuf.createObject();
    root["ip"] = wificfg.ip;
    root["mode"] = json_serialize(wificfg.mode);
    if (wificfg.mode == M_ACCESSPOINT) {
      root["ssid"] = wificfg.ap_ssid;
      root["password"] = wificfg.ap_encryption? wificfg.ap_password : "";
      root["encryption"] = wificfg.ap_encryption;
      root["channel"] = wificfg.ap_channel;
      root["hidden"] = wificfg.ap_hidden;
      
    } else if (wificfg.mode == M_STATION) {
      root["ssid"] = wificfg.stn_ssid;
      root["password"] = wificfg.stn_encryption? wificfg.stn_password : "";
      root["encryption"] = wificfg.stn_encryption;
      root["forceip"] = wificfg.stn_forceip;
      root["revertap"] = wificfg.stn_revertap;
    }
    root["status"] = "ok";
    JsonVariant v = root;
    server.send(200, "application/json", v.as<String>());
    jsonbuf.clear();
  });
  server.on("/api/wifi/set/accesspoint", [](){
    json_addheaders();
    wificfg.mode = M_ACCESSPOINT;
    if (server.hasArg("ssid"))      strlcpy(wificfg.ap_ssid, server.arg("ssid").c_str(), LEN_SSID);
    if (server.hasArg("password"))  strlcpy(wificfg.ap_password, server.arg("password").c_str(), LEN_PASSWORD);
    if (server.hasArg("ssid"))      wificfg.ap_encryption = server.hasArg("password") && server.arg("password").length() > 0;
    if (server.hasArg("channel"))   wificfg.ap_channel = parse_channel(server.arg("channel"), wificfg.ap_channel);
    if (server.hasArg("hidden"))    wificfg.ap_hidden = server.arg("hidden") == "true";
    wificfg_save();
    flag_reboot = server.hasArg("restart") && server.arg("restart") == "true";
    server.send(200, "application/json", json_ok());
  });
  server.on("/api/wifi/set/station", [](){
    json_addheaders();
    wificfg.mode = M_STATION;
    if (server.hasArg("ssid"))      strlcpy(wificfg.stn_ssid, server.arg("ssid").c_str(), LEN_SSID);
    if (server.hasArg("password"))  strlcpy(wificfg.stn_password, server.arg("password").c_str(), LEN_PASSWORD);
    if (server.hasArg("ssid"))      wificfg.stn_encryption = server.hasArg("password") && server.arg("password").length() > 0;
    if (server.hasArg("forceip"))   strlcpy(wificfg.stn_forceip, server.arg("forceip").c_str(), LEN_IP);
    if (server.hasArg("revertap"))  wificfg.stn_revertap = server.arg("revertap") == "true";
    wificfg_save();
    flag_reboot = server.hasArg("restart") && server.arg("restart") == "true";
    server.send(200, "application/json", json_ok());
  });
}

void jsonbrowser_init() {
  server.on("/api/browser/get", [](){
    json_addheaders();
    JsonObject& root = jsonbuf.createObject();
    root["http_enabled"] = browsercfg.http_enabled;
    root["https_enabled"] = browsercfg.https_enabled;
    root["mdns_enabled"] = browsercfg.mdns_enabled;
    root["mdns_hostname"] = browsercfg.mdns_hostname;
    root["auth_enabled"] = browsercfg.auth_enabled;
    root["auth_username"] = browsercfg.auth_username;
    root["auth_password"] = browsercfg.auth_password;
    root["ota_enabled"] = browsercfg.ota_enabled;
    root["ota_hostname"] = browsercfg.ota_hostname;
    root["ota_password"] = browsercfg.ota_password;
    root["status"] = "ok";
    JsonVariant v = root;
    server.send(200, "application/json", v.as<String>());
    jsonbuf.clear();
  });
  server.on("/api/browser/set", [](){
    json_addheaders();
    if (server.hasArg("http_enabled"))  browsercfg.http_enabled = server.arg("http_enabled") == "true";
    if (server.hasArg("https_enabled")) browsercfg.https_enabled = server.arg("https_enabled") == "true";
    if (server.hasArg("mdns_enabled"))  browsercfg.mdns_enabled = server.arg("mdns_enabled") == "true";
    if (server.hasArg("mdns_hostname")) strlcpy(browsercfg.mdns_hostname, server.arg("mdns_hostname").c_str(), LEN_HOSTNAME);
    if (server.hasArg("auth_enabled"))  browsercfg.auth_enabled = server.arg("auth_enabled") == "true";
    if (server.hasArg("auth_username")) strlcpy(browsercfg.auth_username, server.arg("auth_username").c_str(), LEN_USERNAME);
    if (server.hasArg("auth_password")) strlcpy(browsercfg.auth_password, server.arg("auth_password").c_str(), LEN_PASSWORD);
    if (server.hasArg("ota_enabled"))   browsercfg.ota_enabled = server.arg("ota_enabled") == "true";
    if (server.hasArg("ota_hostname"))  strlcpy(browsercfg.ota_hostname, server.arg("ota_hostname").c_str(), LEN_HOSTNAME);
    if (server.hasArg("ota_password"))  strlcpy(browsercfg.ota_password, server.arg("ota_password").c_str(), LEN_PASSWORD);
    browsercfg_save();
    flag_reboot = server.hasArg("restart") && server.arg("restart") == "true";
    server.send(200, "application/json", json_ok());
  });
}

void jsonmotor_init() {
  server.on("/api/motor/get", [](){
    json_addheaders();
    JsonObject& root = jsonbuf.createObject();
    root["mode"] = json_serialize(motorcfg.mode);
    root["stepsize"] = json_serialize(motorcfg.stepsize);
    root["maxspeed"] = motorcfg.maxspeed;
    root["minspeed"] = motorcfg.minspeed;
    root["accel"] = motorcfg.accel;
    root["decel"] = motorcfg.decel;
    root["kthold"] = motorcfg.kthold;
    root["ktrun"] = motorcfg.ktrun;
    root["ktaccel"] = motorcfg.ktaccel;
    root["ktdecel"] = motorcfg.ktdecel;
    root["fsspeed"] = motorcfg.fsspeed;
    root["fsboost"] = motorcfg.fsboost;
    root["reverse"] = motorcfg.reverse;
    root["status"] = "ok";
    JsonVariant v = root;
    server.send(200, "application/json", v.as<String>());
    jsonbuf.clear();
  });
  server.on("/api/motor/set", [](){
    json_addheaders();
    if (server.hasArg("mode"))      motorcfg.mode = parse_motormode(server.arg("mode"), motorcfg.mode);
    if (server.hasArg("stepsize"))  motorcfg.stepsize = parse_stepsize(server.arg("stepsize").toInt(), motorcfg.stepsize);
    if (server.hasArg("maxspeed"))  motorcfg.maxspeed = server.arg("maxspeed").toFloat();
    if (server.hasArg("minspeed"))  motorcfg.minspeed = server.arg("minspeed").toFloat();
    if (server.hasArg("accel"))     motorcfg.accel = server.arg("accel").toFloat();
    if (server.hasArg("decel"))     motorcfg.decel = server.arg("decel").toFloat();
    if (server.hasArg("kthold"))    motorcfg.kthold = server.arg("kthold").toFloat();
    if (server.hasArg("ktrun"))     motorcfg.ktrun = server.arg("ktrun").toFloat();
    if (server.hasArg("ktaccel"))   motorcfg.ktaccel = server.arg("ktaccel").toFloat();
    if (server.hasArg("ktdecel"))   motorcfg.ktdecel = server.arg("ktdecel").toFloat();
    if (server.hasArg("fsspeed"))   motorcfg.fsspeed = server.arg("fsspeed").toFloat();
    if (server.hasArg("fsboost"))   motorcfg.fsboost = server.arg("fsboost") == "true";
    if (server.hasArg("reverse"))   motorcfg.reverse = server.arg("reverse") == "true";
    motorcfg_update();
    if (server.hasArg("save") && server.arg("save") == "true") {
      motorcfg_save();
    }
    server.send(200, "application/json", json_ok());
  });
  server.on("/api/motor/status", [](){
    json_addheaders();
    ps_status status = ps_getstatus(server.arg("clearerrors") == "true");
    JsonObject& root = jsonbuf.createObject();
    JsonObject& alarms = root.createNestedObject("alarms");
    alarms["commanderror"] = status.alarms.command_error;
    alarms["overcurrent"] = status.alarms.overcurrent;
    alarms["undervoltage"] = status.alarms.undervoltage;
    alarms["thermalshutdown"] = status.alarms.thermal_shutdown;
    alarms["thermalwarning"] = status.alarms.thermal_warning;
    alarms["stalldetect"] = status.alarms.stall_detect;
    alarms["switch"] = status.alarms.user_switch;
    root["direction"] = json_serialize(motorcfg_dir(status.direction));
    root["movement"] = json_serialize(status.movement);
    root["hiz"] = status.hiz;
    root["busy"] = status.busy;
    root["switch"] = status.user_switch;
    root["status"] = "ok";
    JsonVariant v = root;
    server.send(200, "application/json", v.as<String>());
    jsonbuf.clear();
  });
  server.on("/api/motor/state", [](){
    json_addheaders();
    JsonObject& root = jsonbuf.createObject();
    root["position"] = statecache.pos;
    root["mark"] = statecache.mark;
    root["speed"] = statecache.speed;
    root["busy"] = statecache.busy;
    root["status"] = "ok";
    JsonVariant v = root;
    server.send(200, "application/json", v.as<String>());
    jsonbuf.clear();
  });
  server.on("/api/motor/adc", [](){
    json_addheaders();
    int adc = ps_readadc();
    JsonObject& root = jsonbuf.createObject();
    if (server.hasArg("raw") && server.arg("raw") == "true") {
      root["value"] = adc;
    } else {
      root["value"] = (float)adc * MOTOR_ADCCOEFF;
    }
    root["status"] = "ok";
    JsonVariant v = root;
    server.send(200, "application/json", v.as<String>());
    jsonbuf.clear();
  });
  server.on("/api/motor/abspos/reset", [](){
    json_addheaders();
    ps_resetpos();
    server.send(200, "application/json", json_ok());
  });
  server.on("/api/motor/abspos/set", [](){
    json_addheaders();
    if (!server.hasArg("position")) {
      server.send(200, "application/json", json_error("position arg must be specified"));
      return;
    }
    ps_setpos(server.arg("position").toInt());
    server.send(200, "application/json", json_ok());
  });
  server.on("/api/motor/mark/set", [](){
    json_addheaders();
    if (!server.hasArg("position")) {
      server.send(200, "application/json", json_error("position arg must be specified"));
      return;
    }
    ps_setmark(server.arg("position").toInt());
    server.send(200, "application/json", json_ok());
  });
  server.on("/api/motor/command/run", [](){
    json_addheaders();
    if (!server.hasArg("speed") || !server.hasArg("direction")) {
      server.send(200, "application/json", json_error("speed, direction args must be specified"));
      return;
    }
    ps_run(motorcfg_dir(parse_direction(server.arg("direction"), FWD)), server.arg("speed").toFloat());
    server.send(200, "application/json", json_ok());
  });
  server.on("/api/motor/command/goto", [](){
    json_addheaders();
    if (!server.hasArg("position")) {
      server.send(200, "application/json", json_error("position arg must be specified"));
      return;
    }
    if (server.hasArg("direction")) {
      ps_goto(server.arg("position").toInt(), motorcfg_dir(parse_direction(server.arg("direction"), FWD)));
    } else {
      ps_goto(server.arg("position").toInt());
    }
    server.send(200, "application/json", json_ok());
  });
  server.on("/api/motor/command/stop", [](){
    json_addheaders();
    if (server.hasArg("soft") && server.arg("soft") == "true") {
      ps_softstop();
    } else {
      ps_hardstop();
    }
    server.send(200, "application/json", json_ok());
  });
  server.on("/api/motor/command/hiz", [](){
    json_addheaders();
    if (server.hasArg("soft") && server.arg("soft") == "true") {
      ps_softhiz();
    } else {
      ps_hardhiz();
    }
    server.send(200, "application/json", json_ok());
  });
}

void json_init() {
  jsonwifi_init();
  jsonbrowser_init();
  jsonmotor_init();
  
  server.on("/api/ping", []() {
    json_addheaders();
    
    JsonObject& obj = jsonbuf.createObject();
    obj["status"] = "ok";
    obj["data"] = "pong";

    JsonVariant v = obj;
    server.send(200, "application/json", v.as<String>());
    jsonbuf.clear();
  });
}

void ws_event(uint8_t num, WStype_t type, uint8_t * payload, size_t len) {
  
}

void setup() {
  Serial.begin(115200);

  // Initialize FS
  {
    SPIFFS.begin();
    
    // Check reset
    pinMode(RESET_PIN, INPUT_PULLUP);
    while (!digitalRead(RESET_PIN) && millis() < RESET_TIMEOUT) {
      delay(1);
    }
    if (millis() >= RESET_TIMEOUT) {
      // Reset chosen
      Serial.println("Configuration Reset");
      SPIFFS.remove(FNAME_WIFICFG);
      SPIFFS.remove(FNAME_BROWSERCFG);
      SPIFFS.remove(FNAME_MOTORCFG);

      // Wait for reset to depress
      while (!digitalRead(RESET_PIN)) {
        delay(100);
      }
    }
  }

  // Read wifi configuration
  {
    File fp = SPIFFS.open(FNAME_WIFICFG, "r");
    if (fp) {
      size_t size = fp.size();
      std::unique_ptr<char[]> buf(new char[size]);
      fp.readBytes(buf.get(), size);
      JsonObject& root = jsonbuf.parseObject(buf.get());
      if (root.containsKey("mode"))           wificfg.mode = parse_wifimode(root["mode"].as<char *>());
      if (root.containsKey("ap_ssid"))        strlcpy(wificfg.ap_ssid, root["ap_ssid"].as<char *>(), LEN_SSID);
      if (root.containsKey("ap_password"))    strlcpy(wificfg.ap_password, root["ap_password"].as<char *>(), LEN_PASSWORD);
      if (root.containsKey("ap_encryption"))  wificfg.ap_encryption = root["ap_encryption"].as<bool>();
      if (root.containsKey("ap_channel"))     wificfg.ap_channel = root["ap_channel"].as<int>();
      if (root.containsKey("ap_hidden"))      wificfg.ap_hidden = root["ap_hidden"].as<bool>();
      if (root.containsKey("stn_ssid"))       strlcpy(wificfg.stn_ssid, root["stn_ssid"].as<char *>(), LEN_SSID);
      if (root.containsKey("stn_password"))   strlcpy(wificfg.stn_password, root["stn_password"].as<char *>(), LEN_PASSWORD);
      if (root.containsKey("stn_encryption")) wificfg.stn_encryption = root["stn_encryption"].as<bool>();
      if (root.containsKey("stn_forceip"))    strlcpy(wificfg.stn_forceip, root["stn_forceip"].as<char *>(), LEN_IP);
      if (root.containsKey("stn_revertap"))   wificfg.stn_revertap = root["stn_revertap"].as<bool>();
      jsonbuf.clear();
    }
  }

  // Read browser configuration
  {
    File fp = SPIFFS.open(FNAME_BROWSERCFG, "r");
    if (fp) {
      size_t size = fp.size();
      std::unique_ptr<char[]> buf(new char[size]);
      fp.readBytes(buf.get(), size);
      JsonObject& root = jsonbuf.parseObject(buf.get());
      if (root.containsKey("http_enabled"))   browsercfg.http_enabled = root["http_enabled"].as<bool>();
      if (root.containsKey("https_enabled"))  browsercfg.https_enabled = root["https_enabled"].as<bool>();
      if (root.containsKey("mdns_enabled"))   browsercfg.mdns_enabled = root["mdns_enabled"].as<bool>();
      if (root.containsKey("mdns_hostname"))  strlcpy(browsercfg.mdns_hostname, root["mdns_hostname"].as<char *>(), LEN_HOSTNAME);
      if (root.containsKey("auth_enabled"))   browsercfg.auth_enabled = root["auth_enabled"].as<bool>();
      if (root.containsKey("auth_username"))  strlcpy(browsercfg.auth_username, root["auth_username"].as<char *>(), LEN_USERNAME);
      if (root.containsKey("auth_password"))  strlcpy(browsercfg.auth_password, root["auth_password"].as<char *>(), LEN_PASSWORD);
      if (root.containsKey("ota_enabled"))    browsercfg.ota_enabled = root["ota_enabled"].as<bool>();
      if (root.containsKey("ota_hostname"))   strlcpy(browsercfg.ota_hostname, root["ota_hostname"].as<char *>(), LEN_HOSTNAME);
      if (root.containsKey("ota_password"))   strlcpy(browsercfg.ota_password, root["ota_password"].as<char *>(), LEN_PASSWORD);
      jsonbuf.clear();
    }
  }

  // Wifi connection
  {
    if (browsercfg.mdns_enabled)
      WiFi.hostname(browsercfg.mdns_hostname);
    
    if (wificfg.mode == M_STATION) {
      Serial.println("Station Mode");
      Serial.println(wificfg.stn_ssid);
      Serial.println(wificfg.stn_password);
      WiFi.mode(WIFI_STA);
      WiFi.begin(wificfg.stn_ssid, wificfg.stn_password);
      for (int i=0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
        delay(500);
      }

      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Could not connect");
        if (wificfg.stn_revertap) {
          Serial.println("Reverting to AP mode");
          wificfg.mode = M_ACCESSPOINT;
          
        } else {
          // Shut down wifi
          WiFi.mode(WIFI_OFF);
        }
      } else {
         strlcpy(wificfg.ip, WiFi.localIP().toString().c_str(), LEN_IP);
      }
    }

    if (wificfg.mode == M_ACCESSPOINT) {
      Serial.println("AP Mode");
      Serial.println(wificfg.ap_ssid);
      Serial.println(wificfg.ap_password);
      WiFi.mode(WIFI_AP);
      WiFi.softAP(wificfg.ap_ssid, wificfg.ap_password);
      strlcpy(wificfg.ip, WiFi.softAPIP().toString().c_str(), LEN_IP);
    }
    
    Serial.println("Wifi connected");
    Serial.println(wificfg.ip);
  }

  // Initialize web services
  {
    if (browsercfg.mdns_enabled) {
      if (!MDNS.begin(browsercfg.mdns_hostname)) {
        Serial.println("Error: Could not start mDNS.");
      }
    }

    if (browsercfg.ota_enabled) {
      ArduinoOTA.setHostname(browsercfg.ota_hostname);
      if (browsercfg.ota_password[0] != 0)
        ArduinoOTA.setPassword(browsercfg.ota_password);
      ArduinoOTA.begin();
    }

    if (browsercfg.http_enabled) {
      json_init();
      server.begin();
      websocket.begin();
      websocket.onEvent(ws_event);
    }
  }

  // Read motor configuration
  {
    File fp = SPIFFS.open(FNAME_MOTORCFG, "r");
    if (fp) {
      size_t size = fp.size();
      std::unique_ptr<char[]> buf(new char[size]);
      fp.readBytes(buf.get(), size);
      JsonObject& root = jsonbuf.parseObject(buf.get());
      if (root.containsKey("mode"))       motorcfg.mode = motorcfg.mode = parse_motormode(root["mode"], motorcfg.mode);
      if (root.containsKey("stepsize"))   motorcfg.stepsize = parse_stepsize(root["stepsize"].as<int>(), motorcfg.stepsize);
      if (root.containsKey("maxspeed"))   motorcfg.maxspeed = root["maxspeed"].as<float>();
      if (root.containsKey("minspeed"))   motorcfg.minspeed = root["minspeed"].as<float>();
      if (root.containsKey("accel"))      motorcfg.accel = root["accel"].as<float>();
      if (root.containsKey("decel"))      motorcfg.decel = root["decel"].as<float>();
      if (root.containsKey("kthold"))     motorcfg.kthold = root["kthold"].as<float>();
      if (root.containsKey("ktrun"))      motorcfg.ktrun = root["ktrun"].as<float>();
      if (root.containsKey("ktaccel"))    motorcfg.ktaccel = root["ktaccel"].as<float>();
      if (root.containsKey("ktdecel"))    motorcfg.ktdecel = root["ktdecel"].as<float>();
      if (root.containsKey("fsspeed"))    motorcfg.fsspeed = root["fsspeed"].as<float>();
      if (root.containsKey("fsboost"))    motorcfg.fsboost = root["fsboost"].as<bool>();
      if (root.containsKey("reverse"))    motorcfg.reverse = root["reverse"].as<bool>();
      jsonbuf.clear();
    }
  }

  // Initialize SPI and Stepper Motor
  {
    ps_spiinit();
    motorcfg_update();
  }

  // Clear any errors, final init
  {
    ps_getstatus(true);
  }
}

unsigned long last_statepoll = 0;
void loop() {
  unsigned long now = millis();

  if ((now - last_statepoll) > 30) {
    ps_status status = ps_getstatus();
    statecache.pos = ps_getpos();
    statecache.mark = ps_getmark();
    statecache.speed = ps_getspeed();
    statecache.busy = status.busy;
    last_statepoll = now;
  }

  if (browsercfg.http_enabled) {
    websocket.loop();
    server.handleClient();
  }

  if (browsercfg.ota_enabled) {
    ArduinoOTA.handle();
  }

  // Reboot if requested
  if (flag_reboot)
    ESP.restart();
}

