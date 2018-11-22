//#include <Arduino.h>
#include <FS.h>
#include <ArduinoJson.h>
//#include <ESPAsyncWebServer.h>
#include <ESP8266WebServer.h>

#include "powerstep01.h"
#include "wifistepper.h"

void wait_callback() {
  ESP.wdtFeed();
}

cfg_motor default_motor() {
  return (cfg_motor){
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
    .fullstepspeed = 1000.0,
    .fsboost = true
  };
}

void printalarms(ps_alarms * a) {
  Serial.println("Alarms:");
  Serial.print("Command Error: "); Serial.println(a->command_error);
  Serial.print("Overcurrent: "); Serial.println(a->overcurrent);
  Serial.print("Undervoltage: "); Serial.println(a->undervoltage);
  Serial.print("Thermal Shutdown: "); Serial.println(a->thermal_shutdown);
  Serial.println();
}

//AsyncWebServer server(80);
ESP8266WebServer server(80);
StaticJsonBuffer<1024> jsonbuf;

cfg_motor motorcfg;


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

const char * json_serialize(ps_direction dir) {
  switch (dir) {
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


    
    JsonVariant v = root;
    server.send(200, "application/json", v.as<String>());
    jsonbuf.clear();
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
    root["direction"] = json_serialize(status.direction);
    root["movement"] = json_serialize(status.movement);
    root["hiz"] = status.hiz;
    root["busy"] = status.busy;
    root["switch"] = status.user_switch;
    root["status"] = "ok";
    JsonVariant v = root;
    server.send(200, "application/json", v.as<String>());
    jsonbuf.clear();
  });
  server.on("/api/motor/command/speed", [](){
    json_addheaders();
    if (!server.hasArg("speed") || !server.hasArg("direction")) {
      server.send(200, "application/json", json_error("speed and direction args not specified"));
      return;
    }

    float speed = server.arg("speed").toFloat();
    ps_direction direction = server.arg("direction") != "forward"? REV : FWD;
    ps_run(direction, speed);
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
    motorcfg = default_motor();
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
    ps_setsync(SYNC_BUSY);
    ps_setmode(motorcfg.mode);
    ps_setstepsize(motorcfg.stepsize);
    ps_setmaxspeed(motorcfg.maxspeed);
    ps_setminspeed(motorcfg.minspeed, true);
    ps_setaccel(motorcfg.accel);
    ps_setdecel(motorcfg.decel);
    ps_setfullstepspeed(motorcfg.fullstepspeed, false);
    
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

#if 0
  // Initialize SPI and Stepper Motor
  {
    ps_spiinit();
  
    ps_setsync(SYNC_BUSY);
    ps_setmode(MODE_VOLTAGE);                 // V
    //ps_setmode(MODE_CURRENT);                 // C
    //ps_setstepsize(STEP_16);                  // C
    ///ps_setstepsize(STEP_128);                 // V
    ps_setstepsize(STEP_1);                 // V
    
    ps_setmaxspeed(1000);
    ps_setminspeed(10, true);
    ps_setaccel(50);
    ps_setdecel(50);
  
    ps_setfullstepspeed(2000, false);
    
    ps_setslewrate(SR_520);
  
    ps_setocd(500, true);
    ps_vm_setpwmfreq(0, 1);                   // V
    //ps_cm_setpredict(true);                   // C
    ps_setvoltcomp(false);
    ps_setswmode(SW_USER);
    ps_setclocksel(CLK_INT16);
  
    ps_setktvals(0.3, 0.3, 0.5, 0.5);    // V
    //ps_setktvals(0.55, 0.55, 0.55, 0.55);     // C
    ps_setalarmconfig(true, true, true, true);
    
    ps_getstatus(true);
  
    ps_run(FWD, 15000);
  }
#endif
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

