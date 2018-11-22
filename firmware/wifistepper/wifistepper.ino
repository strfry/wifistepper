//#include <Arduino.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>

#include "powerstep01.h"
#include "wifistepper.h"

#define MOTOR_ADCCOEFF    2.65625

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

ps_direction motorcfg_dir(ps_direction d) {
  if (!motorcfg.reverse)  return d;
  switch (d) {
    case FWD: return REV;
    case REV: return FWD;
  }
}

ESP8266WebServer server(80);
StaticJsonBuffer<2048> jsonbuf;


ps_mode parse_mode(const String& m, ps_mode d) {
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

#define json_ok() \
  "{\"status\": \"ok\"}"
#define json_error(msg) \
  "{\"status\":\"error\",\"message\":\"" msg "\"}"

void json_addheaders() {
  server.sendHeader("Access-Control-Allow-Credentials", "true");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET");
  server.sendHeader("Access-Control-Allow-Headers", "application/json");
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
    root["status"] = "ok";
    for (int i = 0; i < n; ++i) {
      JsonObject& network = jsonbuf.createObject();
      network["ssid"] = WiFi.SSID(i);
      network["mac"] = WiFi.BSSIDstr(i);
      network["rssi"] = WiFi.RSSI(i);
      network["encryption"] = WiFi.encryptionType(i) != ENC_TYPE_NONE;
      networks.add(network);
    }
    JsonVariant v = root;
    server.send(200, "application/json", v.as<String>());
    jsonbuf.clear();
  });

  server.on("/api/wifi/get", [](){
    
  });

  server.on("/api/wifi/reconnect", [](){
    // TODO
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
    if (server.hasArg("mode"))      motorcfg.mode = parse_mode(server.arg("mode"), motorcfg.mode);
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
  server.on("/api/motor/abspos/get", [](){
    json_addheaders();
    int32_t pos = ps_getpos();
    JsonObject& root = jsonbuf.createObject();
    root["position"] = pos;
    root["status"] = "ok";
    JsonVariant v = root;
    server.send(200, "application/json", v.as<String>());
    jsonbuf.clear();
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
  server.on("/api/motor/mark/get", [](){
    json_addheaders();
    int32_t pos = ps_getmark();
    JsonObject& root = jsonbuf.createObject();
    root["position"] = pos;
    root["status"] = "ok";
    JsonVariant v = root;
    server.send(200, "application/json", v.as<String>());
    jsonbuf.clear();
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
}

void json_init() {
  jsonwifi_init();
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

void setup() {
  Serial.begin(115200);

  // Initialize FS
  {
    SPIFFS.begin();

    // Check reset

    // Create default config if needed
    
  }

  // Read configuration
  {
    //File wificfg_fp = SPIFFS.open("/wifi.json", "r");
    //motorcfg = default_motor();
  }

  // Wifi connection
  {
    WiFi.mode(WIFI_STA);
    WiFi.begin("ATT549", "9071918601");

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
  
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }

  // Initialize web server
  {
    json_init();
    server.begin();
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

void loop() {
  server.handleClient();
  
  //ps_alarms alarms = ps_getalarms();
  //printalarms(&alarms);

  //delay(1000);
  /*
  Serial.println("Loop");

  ps_alarms alarms = ps_getalarms();
  printalarms(&alarms);
  
  ps_move(FWD, 20000);
  ps_waitbusy(wait_callback);
  ps_softstop();
  ps_waitbusy(wait_callback);

  //ps_getstatus();

  ps_move(REV, 20000);
  ps_waitbusy(wait_callback);
  ps_softstop();
  ps_waitbusy(wait_callback);

  //ps_getstatus();
  */
}

