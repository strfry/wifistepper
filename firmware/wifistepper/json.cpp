#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <FS.h>

#include "wifistepper.h"

extern ESP8266WebServer server;
extern StaticJsonBuffer<2048> jsonbuf;

extern volatile bool flag_reboot;

extern volatile motor_state motorst;


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
      root["forcesubnet"] = wificfg.stn_forcesubnet;
      root["forcegateway"] = wificfg.stn_forcegateway;
      root["revertap"] = wificfg.stn_revertap;
    }
    root["status"] = "ok";
    JsonVariant v = root;
    server.send(200, "application/json", v.as<String>());
    jsonbuf.clear();
  });
  server.on("/api/wifi/set/off", [](){
    json_addheaders();
    wificfg.mode = M_OFF;
    wificfg_save();
    flag_reboot = server.hasArg("restart") && server.arg("restart") == "true";
    server.send(200, "application/json", json_ok());
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
    if (server.hasArg("forcesubnet")) strlcpy(wificfg.stn_forcesubnet, server.arg("forcesubnet").c_str(), LEN_IP);
    if (server.hasArg("forcegateway")) strlcpy(wificfg.stn_forcegateway, server.arg("forcegateway").c_str(), LEN_IP);
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
    root["hostname"] = servicecfg.hostname;
    root["http_enabled"] = servicecfg.http_enabled;
    root["https_enabled"] = servicecfg.https_enabled;
    root["mdns_enabled"] = servicecfg.mdns_enabled;
    root["auth_enabled"] = servicecfg.auth_enabled;
    root["auth_username"] = servicecfg.auth_username;
    root["auth_password"] = servicecfg.auth_password;
    root["ota_enabled"] = servicecfg.ota_enabled;
    root["ota_password"] = servicecfg.ota_password;
    root["status"] = "ok";
    JsonVariant v = root;
    server.send(200, "application/json", v.as<String>());
    jsonbuf.clear();
  });
  server.on("/api/browser/set", [](){
    json_addheaders();
    if (server.hasArg("hostname"))      strlcpy(servicecfg.hostname, server.arg("hostname").c_str(), LEN_HOSTNAME);
    if (server.hasArg("http_enabled"))  servicecfg.http_enabled = server.arg("http_enabled") == "true";
    if (server.hasArg("https_enabled")) servicecfg.https_enabled = server.arg("https_enabled") == "true";
    if (server.hasArg("mdns_enabled"))  servicecfg.mdns_enabled = server.arg("mdns_enabled") == "true";
    if (server.hasArg("auth_enabled"))  servicecfg.auth_enabled = server.arg("auth_enabled") == "true";
    if (server.hasArg("auth_username")) strlcpy(servicecfg.auth_username, server.arg("auth_username").c_str(), LEN_USERNAME);
    if (server.hasArg("auth_password")) strlcpy(servicecfg.auth_password, server.arg("auth_password").c_str(), LEN_PASSWORD);
    if (server.hasArg("ota_enabled"))   servicecfg.ota_enabled = server.arg("ota_enabled") == "true";
    if (server.hasArg("ota_password"))  strlcpy(servicecfg.ota_password, server.arg("ota_password").c_str(), LEN_PASSWORD);
    servicecfg_save();
    flag_reboot = server.hasArg("restart") && server.arg("restart") == "true";
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
      motorcfg_read();
    }
    JsonObject& root = jsonbuf.createObject();
    root["mode"] = json_serialize(motorcfg.mode);
    root["stepsize"] = json_serialize(motorcfg.stepsize);
    root["ocd"] = motorcfg.ocd;
    root["ocdshutdown"] = motorcfg.ocdshutdown;
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
    root["cm_switchperiod"] = motorcfg.cm_switchperiod;
    root["cm_predict"] = motorcfg.cm_predict;
    root["cm_minon"] = motorcfg.cm_minon;
    root["cm_minoff"] = motorcfg.cm_minoff;
    root["cm_fastoff"] = motorcfg.cm_fastoff;
    root["cm_faststep"] = motorcfg.cm_faststep;
    root["vm_pwmfreq"] = motorcfg.vm_pwmfreq;
    root["vm_stall"] = motorcfg.vm_stall;
    root["vm_bemf_slopel"] = motorcfg.vm_bemf_slopel;
    root["vm_bemf_speedco"] = motorcfg.vm_bemf_speedco;
    root["vm_bemf_slopehacc"] = motorcfg.vm_bemf_slopehacc;
    root["vm_bemf_slopehdec"] = motorcfg.vm_bemf_slopehdec;
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
    if (server.hasArg("ocd"))       motorcfg.ocd = server.arg("ocd").toFloat();
    if (server.hasArg("ocdshutdown")) motorcfg.ocdshutdown = server.arg("ocdshutdown") == "true";
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
    if (server.hasArg("cm_switchperiod")) motorcfg.cm_switchperiod = server.arg("cm_switchperiod").toFloat();
    if (server.hasArg("cm_predict")) motorcfg.cm_predict = server.arg("cm_predict") == "true";
    if (server.hasArg("cm_minon"))  motorcfg.cm_minon = server.arg("cm_minon").toFloat();
    if (server.hasArg("cm_minoff")) motorcfg.cm_minoff = server.arg("cm_minoff").toFloat();
    if (server.hasArg("cm_fastoff")) motorcfg.cm_fastoff = server.arg("cm_fastoff").toFloat();
    if (server.hasArg("cm_faststep")) motorcfg.cm_faststep = server.arg("cm_faststep").toFloat();
    if (server.hasArg("vm_pwmfreq")) motorcfg.vm_pwmfreq = server.arg("vm_pwmfreq").toFloat();
    if (server.hasArg("vm_stall"))  motorcfg.vm_stall = server.arg("vm_stall").toFloat();
    if (server.hasArg("vm_bemf_slopel")) motorcfg.vm_bemf_slopel = server.arg("vm_bemf_slopel").toFloat();
    if (server.hasArg("vm_bemf_speedco")) motorcfg.vm_bemf_speedco = server.arg("vm_bemf_speedco").toFloat();
    if (server.hasArg("vm_bemf_slopehacc")) motorcfg.vm_bemf_slopehacc = server.arg("vm_bemf_slopehacc").toFloat();
    if (server.hasArg("vm_bemf_slopehdec")) motorcfg.vm_bemf_slopehdec = server.arg("vm_bemf_slopehdec").toFloat();
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
    root["position"] = motorcfg_pos(motorst.pos);
    root["mark"] = motorcfg_pos(motorst.mark);
    root["stepss"] = motorst.stepss;
    root["busy"] = motorst.busy;
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
  server.on("/api/motor/reset", [](){
    json_addheaders();
    ps_reset();
    ps_getstatus(true);
    motorcfg_update();
    server.send(200, "application/json", json_ok());
  });
  server.on("/api/motor/pos/reset", [](){
    json_addheaders();
    ps_resetpos();
    server.send(200, "application/json", json_ok());
  });
  server.on("/api/motor/pos/set", [](){
    json_addheaders();
    if (!server.hasArg("position")) {
      server.send(200, "application/json", json_error("position arg must be specified"));
      return;
    }
    ps_setpos(motorcfg_pos(server.arg("position").toInt()));
    server.send(200, "application/json", json_ok());
  });
  server.on("/api/motor/mark/set", [](){
    json_addheaders();
    if (!server.hasArg("position")) {
      server.send(200, "application/json", json_error("position arg must be specified"));
      return;
    }
    ps_setmark(motorcfg_pos(server.arg("position").toInt()));
    server.send(200, "application/json", json_ok());
  });
  server.on("/api/motor/command/run", [](){
    json_addheaders();
    if (!server.hasArg("stepss") || !server.hasArg("direction")) {
      server.send(200, "application/json", json_error("stepss, direction args must be specified."));
      return;
    }
    cmd_run(parse_direction(server.arg("direction"), FWD), server.arg("stepss").toFloat());
    server.send(200, "application/json", json_ok());
  });
  server.on("/api/motor/command/limit", [](){
    json_addheaders();
    if (!server.hasArg("stepss") || !server.hasArg("direction")) {
      server.send(200, "application/json", json_error("stepss, direction args must be specified. Optional savemark"));
      return;
    }
    cmd_limit(parse_direction(server.arg("direction"), FWD), server.arg("stepss").toFloat(), server.hasArg("savemark") && server.arg("savemark") == "true");
    server.send(200, "application/json", json_ok());
  });
  server.on("/api/motor/command/goto", [](){
    json_addheaders();
    if (!server.hasArg("position")) {
      server.send(200, "application/json", json_error("position arg must be specified"));
      return;
    }
    cmd_goto(server.arg("position").toInt(), parse_direction(server.arg("direction"), FWD));
    server.send(200, "application/json", json_ok());
  });
  server.on("/api/motor/command/stepclock", [](){
    json_addheaders();
    if (!server.hasArg("direction")) {
      server.send(200, "application/json", json_error("direction arg must be specified"));
      return;
    }
    cmd_stepclock(parse_direction(server.arg("direction"), FWD));
    server.send(200, "application/json", json_ok());
  });
  server.on("/api/motor/command/stop", [](){
    json_addheaders();
    cmd_stop(server.hasArg("soft") && server.arg("soft") == "true");
    server.send(200, "application/json", json_ok());
  });
  server.on("/api/motor/command/hiz", [](){
    json_addheaders();
    cmd_hiz(server.hasArg("soft") && server.arg("soft") == "true");
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

