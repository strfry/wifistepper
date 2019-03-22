#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <FS.h>

#include "wifistepper.h"

extern ESP8266WebServer server;
extern StaticJsonBuffer<2048> jsonbuf;

extern volatile bool flag_reboot;


#define json_ok()         "{\"status\":\"ok\"}"
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
    root["ip"] = state.wifi.ip;
    root["mode"] = json_serialize(config.wifi.mode);
    if (config.wifi.mode == M_ACCESSPOINT) {
      root["ssid"] = config.wifi.accesspoint.ssid;
      root["password"] = config.wifi.accesspoint.encryption? config.wifi.accesspoint.password : "";
      root["encryption"] = config.wifi.accesspoint.encryption;
      root["channel"] = config.wifi.accesspoint.channel;
      root["hidden"] = config.wifi.accesspoint.hidden;
      
    } else if (config.wifi.mode == M_STATION) {
      root["ssid"] = config.wifi.station.ssid;
      root["password"] = config.wifi.station.encryption? config.wifi.station.password : "";
      root["encryption"] = config.wifi.station.encryption;
      root["forceip"] = config.wifi.station.forceip;
      root["forcesubnet"] = config.wifi.station.forcesubnet;
      root["forcegateway"] = config.wifi.station.forcegateway;
      root["revertap"] = config.wifi.station.revertap;
    }
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

void jsonbrowser_init() {
  server.on("/api/service/get", [](){
    json_addheaders();
    JsonObject& root = jsonbuf.createObject();
    root["http_enabled"] = config.service.http.enabled;
    root["mdns_enabled"] = config.service.mdns.enabled;
    root["mdns_hostname"] = config.service.mdns.hostname;
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
    if (server.hasArg("http_enabled"))  config.service.http.enabled = server.arg("http_enabled") == "true";
    if (server.hasArg("mdns_enabled"))  config.service.mdns.enabled = server.arg("mdns_enabled") == "true";
    if (server.hasArg("mdns_hostname")) strlcpy(config.service.mdns.hostname, server.arg("mdns_hostname").c_str(), LEN_HOSTNAME);
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
    if (!server.hasArg("cached") || server.arg("cached") != "true") {
      motorcfg_pull(&config.motor);
    }
    JsonObject& root = jsonbuf.createObject();
    root["mode"] = json_serialize(config.motor.mode);
    root["stepsize"] = json_serialize(config.motor.stepsize);
    root["ocd"] = config.motor.ocd;
    root["ocdshutdown"] = config.motor.ocdshutdown;
    root["maxspeed"] = config.motor.maxspeed;
    root["minspeed"] = config.motor.minspeed;
    root["accel"] = config.motor.accel;
    root["decel"] = config.motor.decel;
    root["kthold"] = config.motor.kthold;
    root["ktrun"] = config.motor.ktrun;
    root["ktaccel"] = config.motor.ktaccel;
    root["ktdecel"] = config.motor.ktdecel;
    root["fsspeed"] = config.motor.fsspeed;
    root["fsboost"] = config.motor.fsboost;
    root["cm_switchperiod"] = config.motor.cm.switchperiod;
    root["cm_predict"] = config.motor.cm.predict;
    root["cm_minon"] = config.motor.cm.minon;
    root["cm_minoff"] = config.motor.cm.minoff;
    root["cm_fastoff"] = config.motor.cm.fastoff;
    root["cm_faststep"] = config.motor.cm.faststep;
    root["vm_pwmfreq"] = config.motor.vm.pwmfreq;
    root["vm_stall"] = config.motor.vm.stall;
    root["vm_bemf_slopel"] = config.motor.vm.bemf_slopel;
    root["vm_bemf_speedco"] = config.motor.vm.bemf_speedco;
    root["vm_bemf_slopehacc"] = config.motor.vm.bemf_slopehacc;
    root["vm_bemf_slopehdec"] = config.motor.vm.bemf_slopehdec;
    root["reverse"] = config.motor.reverse;
    root["status"] = "ok";
    JsonVariant v = root;
    server.send(200, "application/json", v.as<String>());
    jsonbuf.clear();
  });
  server.on("/api/motor/set", [](){
    json_addheaders();
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
    cmd_setconfig(nextid(), v.as<String>().c_str());
    jsonbuf.clear();
    server.send(200, "application/json", json_ok());
  });
  server.on("/api/motor/status", [](){
    json_addheaders();
    ps_status status = ps_getstatus(server.arg("clearerrors") == "true");
    JsonObject& root = jsonbuf.createObject();
    root["direction"] = json_serialize(motorcfg_dir(status.direction));
    root["movement"] = json_serialize(status.movement);
    root["hiz"] = status.hiz;
    root["busy"] = status.busy;
    root["switch"] = status.user_switch;
    root["stepclock"] = status.step_clock;
    JsonObject& alarms = root.createNestedObject("alarms");
    alarms["commanderror"] = status.alarms.command_error;
    alarms["overcurrent"] = status.alarms.overcurrent;
    alarms["undervoltage"] = status.alarms.undervoltage;
    alarms["thermalshutdown"] = status.alarms.thermal_shutdown;
    alarms["thermalwarning"] = status.alarms.thermal_warning;
    alarms["stalldetect"] = status.alarms.stall_detect;
    alarms["switch"] = status.alarms.user_switch;
    root["status"] = "ok";
    JsonVariant v = root;
    server.send(200, "application/json", v.as<String>());
    jsonbuf.clear();
  });
  server.on("/api/motor/state", [](){
    json_addheaders();
    JsonObject& root = jsonbuf.createObject();
    root["position"] = motorcfg_pos(state.motor.pos);
    root["mark"] = motorcfg_pos(state.motor.mark);
    root["stepss"] = state.motor.stepss;
    root["busy"] = state.motor.busy;
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
  /*server.on("/api/motor/reset", [](){
    json_addheaders();
    ps_reset();
    ps_getstatus(true);
    motorcfg_update();
    server.send(200, "application/json", json_ok());
  });*/
  server.on("/api/motor/pos/reset", [](){
    json_addheaders();
    cmd_resetpos(nextid());
    server.send(200, "application/json", json_ok());
  });
  server.on("/api/motor/pos/set", [](){
    json_addheaders();
    if (!server.hasArg("position")) {
      server.send(200, "application/json", json_error("position arg must be specified"));
      return;
    }
    cmd_setpos(nextid(), motorcfg_pos(server.arg("position").toInt()));
    server.send(200, "application/json", json_ok());
  });
  server.on("/api/motor/mark/set", [](){
    json_addheaders();
    if (!server.hasArg("position")) {
      server.send(200, "application/json", json_error("position arg must be specified"));
      return;
    }
    cmd_setmark(nextid(), motorcfg_pos(server.arg("position").toInt()));
    server.send(200, "application/json", json_ok());
  });
  server.on("/api/motor/command/run", [](){
    json_addheaders();
    if (!server.hasArg("stepss") || !server.hasArg("direction")) {
      server.send(200, "application/json", json_error("stepss, direction args must be specified."));
      return;
    }
    cmd_run(nextid(), parse_direction(server.arg("direction"), FWD), server.arg("stepss").toFloat());
    server.send(200, "application/json", json_ok());
  });
  server.on("/api/motor/command/goto", [](){
    json_addheaders();
    if (!server.hasArg("position")) {
      server.send(200, "application/json", json_error("position arg must be specified"));
      return;
    }
    if (!server.hasArg("direction")) cmd_goto(nextid(), server.arg("position").toInt());
    else cmd_goto(nextid(), server.arg("position").toInt(), parse_direction(server.arg("direction"), FWD));
    server.send(200, "application/json", json_ok());
  });
  server.on("/api/motor/command/stepclock", [](){
    json_addheaders();
    if (!server.hasArg("direction")) {
      server.send(200, "application/json", json_error("direction arg must be specified"));
      return;
    }
    cmd_stepclock(nextid(), parse_direction(server.arg("direction"), FWD));
    server.send(200, "application/json", json_ok());
  });
  server.on("/api/motor/command/stop", [](){
    json_addheaders();
    cmd_stop(nextid(), false, server.hasArg("soft") && server.arg("soft") == "true");
    server.send(200, "application/json", json_ok());
  });
  server.on("/api/motor/command/hiz", [](){
    json_addheaders();
    cmd_stop(nextid(), true, server.hasArg("soft") && server.arg("soft") == "true");
    server.send(200, "application/json", json_ok());
  });
}

void json_init() {
  jsonwifi_init();
  jsonbrowser_init();
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

  server.on("/api/factoryreset", [](){
    json_addheaders();
    SPIFFS.remove(FNAME_WIFICFG);
    SPIFFS.remove(FNAME_SERVICECFG);
    SPIFFS.remove(FNAME_MOTORCFG);
    flag_reboot = true;
    server.send(200, "application/json", json_ok());
  });
}

