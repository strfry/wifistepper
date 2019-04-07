#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <FS.h>

#include "wifistepper.h"

extern ESP8266WebServer server;
extern StaticJsonBuffer<2048> jsonbuf;

extern volatile bool flag_reboot;

#define json_ok()         "{\"status\":\"ok\"}"
// TODO - populate reply with id
#define json_okid(id)     "{\"status\":\"ok\",\"id\":" "-1" "}"
#define json_error(msg)   "{\"status\":\"error\",\"message\":\"" msg "\"}"

void json_addheaders() {
  server.sendHeader("Access-Control-Allow-Credentials", "true");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST");
  server.sendHeader("Access-Control-Allow-Headers", "Authorization, application/json");
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
    root["ip"] = IPAddress(state.wifi.ip).toString();
    root["rssi"] = state.wifi.rssi;
    root["mode"] = json_serialize(config.wifi.mode);
    root["accesspoint_ssid"] = config.wifi.accesspoint.ssid;
    root["accesspoint_password"] = config.wifi.accesspoint.encryption? config.wifi.accesspoint.password : "";
    root["accesspoint_encryption"] = config.wifi.accesspoint.encryption;
    root["accesspoint_channel"] = config.wifi.accesspoint.channel;
    root["accesspoint_hidden"] = config.wifi.accesspoint.hidden;
    root["station_ssid"] = config.wifi.station.ssid;
    root["station_password"] = config.wifi.station.encryption? config.wifi.station.password : "";
    root["station_encryption"] = config.wifi.station.encryption;
    root["station_forceip"] = config.wifi.station.forceip;
    root["station_forcesubnet"] = config.wifi.station.forcesubnet;
    root["station_forcegateway"] = config.wifi.station.forcegateway;
    root["station_revertap"] = config.wifi.station.revertap;
    root["status"] = "ok";
    JsonVariant v = root;
    server.send(200, "application/json", v.as<String>());
    jsonbuf.clear();
  });
  server.on("/api/wifi/set/off", [](){
    json_addheaders();
    config.wifi.mode = M_OFF;
    wificfg_write(&config.wifi);
    flag_reboot = true;
    server.send(200, "application/json", json_ok());
  });
  server.on("/api/wifi/set/accesspoint", [](){
    json_addheaders();
    config.wifi.mode = M_ACCESSPOINT;
    if (server.hasArg("ssid"))      strlcpy(config.wifi.accesspoint.ssid, server.arg("ssid").c_str(), LEN_SSID);
    if (server.hasArg("password"))  strlcpy(config.wifi.accesspoint.password, server.arg("password").c_str(), LEN_PASSWORD);
    if (server.hasArg("ssid"))      config.wifi.accesspoint.encryption = server.hasArg("password") && server.arg("password").length() > 0;
    if (server.hasArg("channel"))   config.wifi.accesspoint.channel = parse_channel(server.arg("channel"), config.wifi.accesspoint.channel);
    if (server.hasArg("hidden"))    config.wifi.accesspoint.hidden = server.arg("hidden") == "true";
    wificfg_write(&config.wifi);
    flag_reboot = true;
    server.send(200, "application/json", json_ok());
  });
  server.on("/api/wifi/set/station", [](){
    json_addheaders();
    config.wifi.mode = M_STATION;
    if (server.hasArg("ssid"))      strlcpy(config.wifi.station.ssid, server.arg("ssid").c_str(), LEN_SSID);
    if (server.hasArg("password"))  strlcpy(config.wifi.station.password, server.arg("password").c_str(), LEN_PASSWORD);
    if (server.hasArg("ssid"))      config.wifi.station.encryption = server.hasArg("password") && server.arg("password").length() > 0;
    if (server.hasArg("forceip"))   strlcpy(config.wifi.station.forceip, server.arg("forceip").c_str(), LEN_IP);
    if (server.hasArg("forcesubnet")) strlcpy(config.wifi.station.forcesubnet, server.arg("forcesubnet").c_str(), LEN_IP);
    if (server.hasArg("forcegateway")) strlcpy(config.wifi.station.forcegateway, server.arg("forcegateway").c_str(), LEN_IP);
    if (server.hasArg("revertap"))  config.wifi.station.revertap = server.arg("revertap") == "true";
    wificfg_write(&config.wifi);
    flag_reboot = true;
    server.send(200, "application/json", json_ok());
  });
}

void jsonservice_init() {
  server.on("/api/service/get", [](){
    json_addheaders();
    JsonObject& root = jsonbuf.createObject();
    root["hostname"] = config.service.hostname;
    root["http_enabled"] = config.service.http.enabled;
    root["mdns_enabled"] = config.service.mdns.enabled;
    root["auth_enabled"] = config.service.auth.enabled;
    root["auth_username"] = config.service.auth.username;
    root["auth_password"] = config.service.auth.password;
    root["ota_enabled"] = config.service.ota.enabled;
    root["ota_password"] = config.service.ota.password;
    // TODO add rest of config
    root["status"] = "ok";
    JsonVariant v = root;
    server.send(200, "application/json", v.as<String>());
    jsonbuf.clear();
  });
  server.on("/api/service/set", [](){
    json_addheaders();
    if (server.hasArg("hostname"))      strlcpy(config.service.hostname, server.arg("hostname").c_str(), LEN_HOSTNAME);
    if (server.hasArg("http_enabled"))  config.service.http.enabled = server.arg("http_enabled") == "true";
    if (server.hasArg("mdns_enabled"))  config.service.mdns.enabled = server.arg("mdns_enabled") == "true";
    if (server.hasArg("auth_enabled"))  config.service.auth.enabled = server.arg("auth_enabled") == "true";
    if (server.hasArg("auth_username")) strlcpy(config.service.auth.username, server.arg("auth_username").c_str(), LEN_USERNAME);
    if (server.hasArg("auth_password")) strlcpy(config.service.auth.password, server.arg("auth_password").c_str(), LEN_PASSWORD);
    if (server.hasArg("ota_enabled"))   config.service.ota.enabled = server.arg("ota_enabled") == "true";
    if (server.hasArg("ota_password"))  strlcpy(config.service.ota.password, server.arg("ota_password").c_str(), LEN_PASSWORD);
    // TODO - add rest of service config
    servicecfg_write(&config.service);
    flag_reboot = true;
    server.send(200, "application/json", json_ok());
  });
}

void jsondaisy_init() {
  server.on("/api/daisy/get", [](){
    json_addheaders();
    JsonObject& root = jsonbuf.createObject();
    root["enabled"] = config.daisy.enabled;
    root["master"] = config.daisy.master;
    if (config.daisy.master) {
      root["slaves"] = state.daisy.slaves;
    }
    root["slavewifioff"] = config.daisy.slavewifioff;
    root["status"] = "ok";
    JsonVariant v = root;
    server.send(200, "application/json", v.as<String>());
    jsonbuf.clear();
  });
  server.on("/api/daisy/set", [](){
    json_addheaders();
    if (server.hasArg("enabled"))       config.daisy.enabled = server.arg("enabled") == "true";
    if (server.hasArg("master"))        config.daisy.master = server.arg("master") == "true";
    if (server.hasArg("slavewifioff"))  config.daisy.slavewifioff = server.arg("slavewifioff") == "true";
    daisycfg_write(&config.daisy);
    flag_reboot = true;
    server.send(200, "application/json", json_ok());
  });
  server.on("/api/daisy/wificontrol", [](){
    json_addheaders();
    if (!config.daisy.enabled || !config.daisy.master || !state.daisy.active) {
      server.send(200, "application/json", json_error("daisy not active"));
      return;
    }
    if (!server.hasArg("enabled")) {
      server.send(200, "application/json", json_error("enabled arg must be specified"));
      return;
    }
    bool enabled = server.arg("enabled") == "true";
    for (uint8_t i = 1; i < state.daisy.slaves; i++) {
      daisy_wificontrol(i, nextid(), enabled);
    }
    server.send(200, "application/json", json_ok());
  });
}

void jsoncpu_init() {
  server.on("/api/cpu/adc", [](){
    json_addheaders();
    int adc = analogRead(A0);
    JsonObject& root = jsonbuf.createObject();
    if (server.hasArg("raw") && server.arg("raw") == "true") {
      root["value"] = adc;
    } else {
      root["value"] = adc;
    }
    root["status"] = "ok";
    JsonVariant v = root;
    server.send(200, "application/json", v.as<String>());
    jsonbuf.clear();
  });
}

void jsonmotor_init() {
  server.on("/api/motor/get", [](){
    json_addheaders();
    motor_config * cfg = &config.motor;
    int target = 0;
    if (server.hasArg("target")) {
      target = server.arg("target").toInt();
      if (target != 0 && (!config.daisy.enabled || !config.daisy.master || !state.daisy.active)) {
        server.send(200, "application/json", json_error("failed to get target"));
        return;
      }
      if (target < 0 || target > state.daisy.slaves) {
        server.send(200, "application/json", json_error("invalid target"));
        return;
      }
      if (target > 0) cfg = &sketch.daisy.slave[target - 1].config.motor;
    }
    JsonObject& root = jsonbuf.createObject();
    if (server.hasArg("target")) root["target"] = target;
    root["mode"] = json_serialize(cfg->mode);
    root["stepsize"] = json_serialize(cfg->stepsize);
    root["ocd"] = cfg->ocd;
    root["ocdshutdown"] = cfg->ocdshutdown;
    root["maxspeed"] = cfg->maxspeed;
    root["minspeed"] = cfg->minspeed;
    root["accel"] = cfg->accel;
    root["decel"] = cfg->decel;
    root["kthold"] = cfg->kthold;
    root["ktrun"] = cfg->ktrun;
    root["ktaccel"] = cfg->ktaccel;
    root["ktdecel"] = cfg->ktdecel;
    root["fsspeed"] = cfg->fsspeed;
    root["fsboost"] = cfg->fsboost;
    root["cm_switchperiod"] = cfg->cm.switchperiod;
    root["cm_predict"] = cfg->cm.predict;
    root["cm_minon"] = cfg->cm.minon;
    root["cm_minoff"] = cfg->cm.minoff;
    root["cm_fastoff"] = cfg->cm.fastoff;
    root["cm_faststep"] = cfg->cm.faststep;
    root["vm_pwmfreq"] = cfg->vm.pwmfreq;
    root["vm_stall"] = cfg->vm.stall;
    root["vm_bemf_slopel"] = cfg->vm.bemf_slopel;
    root["vm_bemf_speedco"] = cfg->vm.bemf_speedco;
    root["vm_bemf_slopehacc"] = cfg->vm.bemf_slopehacc;
    root["vm_bemf_slopehdec"] = cfg->vm.bemf_slopehdec;
    root["reverse"] = cfg->reverse;
    root["status"] = "ok";
    JsonVariant v = root;
    server.send(200, "application/json", v.as<String>());
    jsonbuf.clear();
  });
  server.on("/api/motor/set", [](){
    json_addheaders();
    motor_config * cfg = &config.motor;
    int target = 0;
    if (server.hasArg("target")) {
      target = server.arg("target").toInt();
      if (target != 0 && (!config.daisy.enabled || !config.daisy.master || !state.daisy.active)) {
        server.send(200, "application/json", json_error("failed to set target"));
        return;
      }
      if (target < 0 || target > state.daisy.slaves) {
        server.send(200, "application/json", json_error("invalid target"));
        return;
      }
    }
    JsonObject& root = jsonbuf.createObject();
    if (server.hasArg("mode"))      root["mode"] = server.arg("mode");
    if (server.hasArg("stepsize"))  root["stepsize"] = server.arg("stepsize").toInt();
    if (server.hasArg("ocd"))       root["ocd"] = server.arg("ocd").toFloat();
    if (server.hasArg("ocdshutdown")) root["ocdshutdown"] = server.arg("ocdshutdown") == "true";
    if (server.hasArg("maxspeed"))  root["maxspeed"] = server.arg("maxspeed").toFloat();
    if (server.hasArg("minspeed"))  root["minspeed"] = server.arg("minspeed").toFloat();
    if (server.hasArg("accel"))     root["accel"] = server.arg("accel").toFloat();
    if (server.hasArg("decel"))     root["decel"] = server.arg("decel").toFloat();
    if (server.hasArg("kthold"))    root["kthold"] = server.arg("kthold").toFloat();
    if (server.hasArg("ktrun"))     root["ktrun"] = server.arg("ktrun").toFloat();
    if (server.hasArg("ktaccel"))   root["ktaccel"] = server.arg("ktaccel").toFloat();
    if (server.hasArg("ktdecel"))   root["ktdecel"] = server.arg("ktdecel").toFloat();
    if (server.hasArg("fsspeed"))   root["fsspeed"] = server.arg("fsspeed").toFloat();
    if (server.hasArg("fsboost"))   root["fsboost"] = server.arg("fsboost") == "true";
    if (server.hasArg("cm_switchperiod")) root["cm_switchperiod"] = server.arg("cm_switchperiod").toFloat();
    if (server.hasArg("cm_predict")) root["cm_predict"] = server.arg("cm_predict") == "true";
    if (server.hasArg("cm_minon"))  root["cm_minon"] = server.arg("cm_minon").toFloat();
    if (server.hasArg("cm_minoff")) root["cm_minoff"] = server.arg("cm_minoff").toFloat();
    if (server.hasArg("cm_fastoff")) root["cm_fastoff"] = server.arg("cm_fastoff").toFloat();
    if (server.hasArg("cm_faststep")) root["cm_faststep"] = server.arg("cm_faststep").toFloat();
    if (server.hasArg("vm_pwmfreq")) root["vm_pwmfreq"] = server.arg("vm_pwmfreq").toFloat();
    if (server.hasArg("vm_stall"))  root["vm_stall"] = server.arg("vm_stall").toFloat();
    if (server.hasArg("vm_bemf_slopel")) root["vm_bemf_slopel"] = server.arg("vm_bemf_slopel").toFloat();
    if (server.hasArg("vm_bemf_speedco")) root["vm_bemf_speedco"] = server.arg("vm_bemf_speedco").toFloat();
    if (server.hasArg("vm_bemf_slopehacc")) root["vm_bemf_slopehacc"] = server.arg("vm_bemf_slopehacc").toFloat();
    if (server.hasArg("vm_bemf_slopehdec")) root["vm_bemf_slopehdec"] = server.arg("vm_bemf_slopehdec").toFloat();
    if (server.hasArg("reverse"))   root["reverse"] = server.arg("reverse") == "true";
    if (server.hasArg("save"))      root["save"] = server.arg("save") == "true";
    JsonVariant v = root;
    id_t id = nextid();
    m_setconfig(target, id, v.as<String>().c_str());
    jsonbuf.clear();
    server.send(200, "application/json", json_okid(id));
  });
  server.on("/api/motor/state", [](){
    json_addheaders();
    motor_state * st = &state.motor;
    int target = 0;
    if (server.hasArg("target")) {
      target = server.arg("target").toInt();
      if (target != 0 && (!config.daisy.enabled || !config.daisy.master || !state.daisy.active)) {
        server.send(200, "application/json", json_error("failed to get target"));
        return;
      }
      if (target < 0 || target > state.daisy.slaves) {
        server.send(200, "application/json", json_error("invalid target"));
        return;
      }
      if (target > 0) st = &sketch.daisy.slave[target - 1].state.motor;
    }
    JsonObject& root = jsonbuf.createObject();
    if (server.hasArg("target")) root["target"] = target;
    root["stepss"] = st->stepss;
    root["position"] = st->pos;
    root["mark"] = st->mark;
    root["adc"] = st->adc;
    root["direction"] = json_serialize(st->status.direction);
    root["movement"] = json_serialize(st->status.movement);
    root["hiz"] = st->status.hiz;
    root["busy"] = st->status.busy;
    root["switch"] = st->status.user_switch;
    root["stepclock"] = st->status.step_clock;
    JsonObject& alarms = root.createNestedObject("alarms");
    alarms["commanderror"] = st->status.alarms.command_error;
    alarms["overcurrent"] = st->status.alarms.overcurrent;
    alarms["undervoltage"] = st->status.alarms.undervoltage;
    alarms["thermalshutdown"] = st->status.alarms.thermal_shutdown;
    alarms["thermalwarning"] = st->status.alarms.thermal_warning;
    alarms["stalldetect"] = st->status.alarms.stall_detect;
    alarms["switch"] = st->status.alarms.user_switch;
    root["status"] = "ok";
    JsonVariant v = root;
    server.send(200, "application/json", v.as<String>());
    jsonbuf.clear();
  });
  /*server.on("/api/motor/reset", [](){
    json_addheaders();
    ps_reset();
    ps_getstatus(true);
    motorcfg_update();
    server.send(200, "application/json", json_ok());
  });*/
  server.on("/api/motor/pos/reset", [](){
    json_addheaders();
    int target = 0;
    if (server.hasArg("target")) {
      target = server.arg("target").toInt();
      if (target != 0 && (!config.daisy.enabled || !config.daisy.master || !state.daisy.active)) {
        server.send(200, "application/json", json_error("failed to set target"));
        return;
      }
      if (target < 0 || target > state.daisy.slaves) {
        server.send(200, "application/json", json_error("invalid target"));
        return;
      }
    }
    id_t id = nextid();
    m_resetpos(target, id);
    server.send(200, "application/json", json_okid(id));
  });
  server.on("/api/motor/pos/set", [](){
    json_addheaders();
    int target = 0;
    if (server.hasArg("target")) {
      target = server.arg("target").toInt();
      if (target != 0 && (!config.daisy.enabled || !config.daisy.master || !state.daisy.active)) {
        server.send(200, "application/json", json_error("failed to set target"));
        return;
      }
      if (target < 0 || target > state.daisy.slaves) {
        server.send(200, "application/json", json_error("invalid target"));
        return;
      }
    }
    if (!server.hasArg("position")) {
      server.send(200, "application/json", json_error("position arg must be specified"));
      return;
    }
    int pos = server.arg("position").toInt();
    id_t id = nextid();
    m_setpos(target, id, pos);
    server.send(200, "application/json", json_okid(id));
  });
  server.on("/api/motor/mark/set", [](){
    json_addheaders();
    int target = 0;
    if (server.hasArg("target")) {
      target = server.arg("target").toInt();
      if (target != 0 && (!config.daisy.enabled || !config.daisy.master || !state.daisy.active)) {
        server.send(200, "application/json", json_error("failed to set target"));
        return;
      }
      if (target < 0 || target > state.daisy.slaves) {
        server.send(200, "application/json", json_error("invalid target"));
        return;
      }
    }
    if (!server.hasArg("position")) {
      server.send(200, "application/json", json_error("position arg must be specified"));
      return;
    }
    int mark = server.arg("position").toInt();
    id_t id = nextid();
    m_setmark(target, id, mark);
    server.send(200, "application/json", json_okid(id));
  });
  server.on("/api/motor/command/run", [](){
    json_addheaders();
    int target = 0;
    if (server.hasArg("target")) {
      target = server.arg("target").toInt();
      if (target != 0 && (!config.daisy.enabled || !config.daisy.master || !state.daisy.active)) {
        server.send(200, "application/json", json_error("failed to set target"));
        return;
      }
      if (target < 0 || target > state.daisy.slaves) {
        server.send(200, "application/json", json_error("invalid target"));
        return;
      }
    }
    if (!server.hasArg("stepss") || !server.hasArg("direction")) {
      server.send(200, "application/json", json_error("stepss, direction args must be specified."));
      return;
    }
    ps_direction dir = parse_direction(server.arg("direction"), FWD);
    float stepss = server.arg("stepss").toFloat();
    id_t id = nextid();
    m_run(target, id, dir, stepss);
    server.send(200, "application/json", json_okid(id));
  });
  server.on("/api/motor/command/goto", [](){
    json_addheaders();
    int target = 0;
    if (server.hasArg("target")) {
      target = server.arg("target").toInt();
      if (target != 0 && (!config.daisy.enabled || !config.daisy.master || !state.daisy.active)) {
        server.send(200, "application/json", json_error("failed to set target"));
        return;
      }
      if (target < 0 || target > state.daisy.slaves) {
        server.send(200, "application/json", json_error("invalid target"));
        return;
      }
    }
    if (!server.hasArg("position")) {
      server.send(200, "application/json", json_error("position arg must be specified"));
      return;
    }
    id_t id = nextid();
    int pos = server.arg("position").toInt();
    ps_direction dir = parse_direction(server.arg("direction"), FWD);
    m_goto(target, id, pos, server.hasArg("direction"), dir);
    server.send(200, "application/json", json_okid(id));
  });
  server.on("/api/motor/command/stepclock", [](){
    json_addheaders();
    int target = 0;
    if (server.hasArg("target")) {
      target = server.arg("target").toInt();
      if (target != 0 && (!config.daisy.enabled || !config.daisy.master || !state.daisy.active)) {
        server.send(200, "application/json", json_error("failed to set target"));
        return;
      }
      if (target < 0 || target > state.daisy.slaves) {
        server.send(200, "application/json", json_error("invalid target"));
        return;
      }
    }
    if (!server.hasArg("direction")) {
      server.send(200, "application/json", json_error("direction arg must be specified"));
      return;
    }
    ps_direction dir = parse_direction(server.arg("direction"), FWD);
    id_t id = nextid();
    m_stepclock(target, id, dir);
    server.send(200, "application/json", json_okid(id));
  });
  server.on("/api/motor/command/stop", [](){
    json_addheaders();
    int target = 0;
    if (server.hasArg("target")) {
      target = server.arg("target").toInt();
      if (target != 0 && (!config.daisy.enabled || !config.daisy.master || !state.daisy.active)) {
        server.send(200, "application/json", json_error("failed to set target"));
        return;
      }
      if (target < 0 || target > state.daisy.slaves) {
        server.send(200, "application/json", json_error("invalid target"));
        return;
      }
    }
    bool hiz = server.hasArg("hiz") && server.arg("hiz") == "true";
    bool soft = server.hasArg("soft") && server.arg("soft") == "true";
    id_t id = nextid();
    m_stop(target, id, hiz, soft);
    server.send(200, "application/json", json_okid(id));
  });
  server.on("/api/motor/command/estop", [](){
    json_addheaders();
    int target = 0;
    if (server.hasArg("target")) {
      target = server.arg("target").toInt();
      if (target != 0 && (!config.daisy.enabled || !config.daisy.master || !state.daisy.active)) {
        server.send(200, "application/json", json_error("failed to set target"));
        return;
      }
      if (target < 0 || target > state.daisy.slaves) {
        server.send(200, "application/json", json_error("invalid target"));
        return;
      }
    }
    bool hiz = server.hasArg("hiz") && server.arg("hiz") == "true";
    bool soft = server.hasArg("soft") && server.arg("soft") == "true";
    id_t id = nextid();
    m_estop(target, id, hiz, soft);
    server.send(200, "application/json", json_okid(id));
  });
}

void json_init() {
  jsonwifi_init();
  jsonservice_init();
  jsondaisy_init();
  jsoncpu_init();
  jsonmotor_init();
  
  server.on("/api/ping", [](){
    json_addheaders();
    JsonObject& obj = jsonbuf.createObject();
    obj["status"] = "ok";
    obj["data"] = "pong";
    JsonVariant v = obj;
    server.send(200, "application/json", v.as<String>());
    jsonbuf.clear();
  });
  server.on("/api/error/get", [](){
    json_addheaders();
    JsonObject& obj = jsonbuf.createObject();
    obj["errored"] = state.error.errored;
    obj["now"] = millis();
    if (state.error.errored) {
      obj["when"] = state.error.when;
      obj["subsystem"] = state.error.subsystem;
      obj["id"] = state.error.id;
      obj["type"] = state.error.type;
      obj["arg"] = state.error.arg;
    }
    obj["status"] = "ok";
    JsonVariant v = obj;
    server.send(200, "application/json", v.as<String>());
    jsonbuf.clear();
  });
  server.on("/api/error/clear", [](){
    json_addheaders();
    // Clear local errors
    cmd_clearerror();
    clearerror();

    // Clear daisy errors
    if (config.daisy.enabled && config.daisy.master && state.daisy.active) {
      for (uint8_t i = 1; i <= state.daisy.slaves; i++) {
        daisy_clearerror(i, nextid());
      }
    }
    server.send(200, "application/json", json_okid(id));
  });
  server.on("/api/factoryreset", [](){
    json_addheaders();
    SPIFFS.remove(FNAME_WIFICFG);
    SPIFFS.remove(FNAME_SERVICECFG);
    SPIFFS.remove(FNAME_DAISYCFG);
    SPIFFS.remove(FNAME_MOTORCFG);
    flag_reboot = true;
    server.send(200, "application/json", json_ok());
  });
}

