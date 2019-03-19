#include <FS.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>

#include "powerstep01.h"
#include "wifistepper.h"

#define DEFAULT_APSSID  {'w','s','x','1','0','0','-','a','p',0}

#define TYPE_HTML     "text/html"
#define TYPE_CSS      "text/css"
#define TYPE_JS       "application/javascript"
#define TYPE_PNG      "image/png"

#define HANDLE_CMDS() ({ cmd_loop(); })

ESP8266WebServer server(PORT_HTTP);
WebSocketsServer websocket(PORT_HTTPWS);
WiFiClient * mqtt_conn = NULL;
PubSubClient * mqtt_client = NULL;

StaticJsonBuffer<2048> jsonbuf;
StaticJsonBuffer<1024> configbuf;

volatile bool flag_reboot = false;
volatile bool flag_wifiled = false;
volatile bool flag_cmderror = false;

volatile command_state commandst = { 0 };
volatile motor_state motorst = { 0 };
volatile service_state servicest = { 0 };

wifi_config wificfg = {
  .mode = M_ACCESSPOINT,
  .ap_ssid = DEFAULT_APSSID,
  .ap_password = {0},
  .ap_encryption = false,
  .ap_channel = 1,
  .ap_hidden = false,
  .stn_ssid = {0},
  .stn_password = {0},
  .stn_encryption = false,
  .stn_forceip = {0},
  .stn_forcesubnet = {0},
  .stn_forcegateway = {0},
  .stn_revertap = true,
  .ip = {0}
};

service_config servicecfg = {
  .hostname = {'w','s','x','1','0','0',0},
  .http_enabled = true,
  .https_enabled = false,
  .mdns_enabled = true,
  .auth_enabled = false,
  .auth_username = {0},
  .auth_password = {0},
  .ota_enabled = true,
  .ota_password = {0},
  .daisycfg = {
    .enabled = true,
    .master = false,
    .id = {0, 0},
    .baudrate = 1000000
  },
  .mqttcfg = {
    .enabled = false,
    .secure = false,
    //.secure = true,
    .server = {'i','o','.','a','d','a','f','r','u','i','t','.','c','o','m',0},
    .port = 1883,
    //.port = 8883,
    .username = {'a','k','l','o','f','a','s',0},
    .key = {'b','b','7','3','3','c','b','e','4','a','9','b','4','7','5','2','a','c','0','d','a','a','b','b','b','e','4','2','f','b','5','7',0},
    .topic_status = {'a','k','l','o','f','a','s','/','f','e','e','d','s','/','s','t','a','t','u','s',0},
    .topic_state = {'a','k','l','o','f','a','s','/','f','e','e','d','s','/','s','t','a','t','e',0},
    .topic_command = {'a','k','l','o','f','a','s','/','f','e','e','d','s','/','c','o','m','m','a','n','d',0},
    .period_state = 2000
  }
};

motor_config motorcfg = {
  .mode = MODE_CURRENT,
  .stepsize = STEP_16,
  .ocd = 500.0,
  .ocdshutdown = true,
  .maxspeed = 10000.0,
  .minspeed = 0.0,
  .accel = 1000.0,
  .decel = 1000.0,
  .kthold = 0.2,
  .ktrun = 0.2,
  .ktaccel = 0.2,
  .ktdecel = 0.2,
  .fsspeed = 2000.0,
  .fsboost = false,
  .cm_switchperiod = 44,
  .cm_predict = true,
  .cm_minon = 21,
  .cm_minoff = 21,
  .cm_fastoff = 4,
  .cm_faststep = 20,
  .vm_pwmfreq = 23.4,
  .vm_stall = 531.25,
  .vm_bemf_slopel = 0.0375,
  .vm_bemf_speedco = 61.5072,
  .vm_bemf_slopehacc = 0.0615,
  .vm_bemf_slopehdec = 0.0615,
  .reverse = false
};



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
  root["stn_forcesubnet"] = wificfg.stn_forcesubnet;
  root["stn_forcegateway"] = wificfg.stn_forcegateway;
  root["stn_revertap"] = wificfg.stn_revertap;
  JsonVariant v = root;
  File cfg = SPIFFS.open(FNAME_WIFICFG, "w");
  root.printTo(cfg);
  cfg.close();
  jsonbuf.clear();
}

void servicecfg_save() {
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
  JsonVariant v = root;
  File cfg = SPIFFS.open(FNAME_SERVICECFG, "w");
  root.printTo(cfg);
  cfg.close();
  jsonbuf.clear();
}

void motorcfg_read() {
  motorcfg.mode = ps_getmode();
  motorcfg.stepsize = ps_getstepsize();
  ps_ocd ocd = ps_getocd();
  motorcfg.ocd = ocd.millivolts;
  motorcfg.ocdshutdown = ocd.shutdown;
  motorcfg.maxspeed = ps_getmaxspeed();
  ps_minspeed minspeed = ps_getminspeed();
  motorcfg.minspeed = minspeed.steps_per_sec;
  motorcfg.accel = ps_getaccel();
  motorcfg.decel = ps_getdecel();
  ps_ktvals ktvals = ps_getktvals();
  motorcfg.kthold = ktvals.hold;
  motorcfg.ktrun = ktvals.run;
  motorcfg.ktaccel = ktvals.accel;
  motorcfg.ktdecel = ktvals.decel;
  ps_fullstepspeed fullstepspeed = ps_getfullstepspeed();
  motorcfg.fsspeed = fullstepspeed.steps_per_sec;
  motorcfg.fsboost = fullstepspeed.boost_mode;
  if (motorcfg.mode == MODE_VOLTAGE) {
    ps_vm_pwmfreq pwmfreq = ps_vm_getpwmfreq();
    motorcfg.vm_pwmfreq = ps_vm_coeffs2pwmfreq(MOTOR_CLOCK, &pwmfreq) / 1000.0;
    motorcfg.vm_stall = ps_vm_getstall();
    ps_vm_bemf bemf = ps_vm_getbemf();
    motorcfg.vm_bemf_slopel = bemf.slopel;
    motorcfg.vm_bemf_speedco = bemf.speedco;
    motorcfg.vm_bemf_slopehacc = bemf.slopehacc;
    motorcfg.vm_bemf_slopehdec = bemf.slopehdec;
  } else {
    motorcfg.cm_switchperiod = ps_cm_getswitchperiod();
    motorcfg.cm_predict = ps_cm_getpredict();
    ps_cm_ctrltimes ctrltimes = ps_cm_getctrltimes();
    motorcfg.cm_minon = ctrltimes.min_on_us;
    motorcfg.cm_minoff = ctrltimes.min_off_us;
    motorcfg.cm_fastoff = ctrltimes.fast_off_us;
    motorcfg.cm_faststep = ctrltimes.fast_step_us;
  }
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

  ps_setocd(motorcfg.ocd, motorcfg.ocdshutdown);
  if (motorcfg.mode == MODE_VOLTAGE) {
    ps_vm_pwmfreq pwmfreq = ps_vm_pwmfreq2coeffs(MOTOR_CLOCK, motorcfg.vm_pwmfreq * 1000.0);
    ps_vm_setpwmfreq(&pwmfreq);
    ps_vm_setstall(motorcfg.vm_stall);
    ps_vm_setbemf(motorcfg.vm_bemf_slopel, motorcfg.vm_bemf_speedco, motorcfg.vm_bemf_slopehacc, motorcfg.vm_bemf_slopehdec);
  } else {
    ps_cm_setswitchperiod(motorcfg.cm_switchperiod);
    ps_cm_setpredict(motorcfg.cm_predict);
    ps_cm_setctrltimes(motorcfg.cm_minon, motorcfg.cm_minoff, motorcfg.cm_fastoff, motorcfg.cm_faststep);
  }
  ps_setvoltcomp(false);
  ps_setswmode(SW_USER);
  ps_setclocksel(MOTOR_CLOCK);

  ps_setktvals(motorcfg.kthold, motorcfg.ktrun, motorcfg.ktaccel, motorcfg.ktdecel);
  ps_setalarmconfig(true, true, true, true);
}

void motorcfg_save() {
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
  JsonVariant v = root;
  File cfg = SPIFFS.open(FNAME_MOTORCFG, "w");
  root.printTo(cfg);
  cfg.close();
  jsonbuf.clear();
}



void static_serve(String contenttype, String path) {
  File fp = SPIFFS.open(path, "r");
  if (fp) {
    server.streamFile(fp, contenttype);
    fp.close();
  } else {
    server.send(404, "text/plain", "File not found.");
  }
}

void static_init() {
  server.on("/", [](){
    String path;
    char def_apssid[] = DEFAULT_APSSID;
    if (wificfg.mode == M_ACCESSPOINT && strcmp(wificfg.ap_ssid, def_apssid) == 0) {
      path = "/settings";
    } else {
      path = "/quickstart";
    }
    server.sendHeader("Location", path);
    server.send(302, "text/plain", String("Redirect to ") + path);
  });
  
  server.on("/quickstart", [](){ static_serve(TYPE_HTML, "/quickstart.html.gz"); });
  server.on("/dashboard", [](){ static_serve(TYPE_HTML, "/dashboard.html.gz"); });
  server.on("/settings", [](){ static_serve(TYPE_HTML, "/settings.html.gz"); });
  server.on("/documentation", [](){ static_serve(TYPE_HTML, "/documentation.html.gz"); });
  server.on("/troubleshoot", [](){ static_serve(TYPE_HTML, "/troubleshoot.html.gz"); });
  server.on("/about", [](){ static_serve(TYPE_HTML, "/about.html.gz"); });
  
  server.on("/js/axios.min.js", [](){ static_serve(TYPE_JS, "/js/axios.min.js.gz"); });
  server.on("/js/clipboard.min.js", [](){ static_serve(TYPE_JS, "/js/clipboard.min.js.gz"); });
  server.on("/js/prism.min.js", [](){ static_serve(TYPE_JS, "/js/prism.min.js.gz"); });
  server.on("/js/vue.min.js", [](){ static_serve(TYPE_JS, "/js/vue.min.js.gz"); });
  server.on("/js/vue-cookies.min.js", [](){ static_serve(TYPE_JS, "/js/vue-cookies.min.js.gz"); });
}



void setup() {
  // Initialize early subsystems
  {
    cmd_init();
    daisy_init();
  }

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
      SPIFFS.remove(FNAME_SERVICECFG);
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
      if (root.containsKey("stn_forcesubnet")) strlcpy(wificfg.stn_forcesubnet, root["stn_forcesubnet"].as<char *>(), LEN_IP);
      if (root.containsKey("stn_forcegateway")) strlcpy(wificfg.stn_forcegateway, root["stn_forcegateway"].as<char *>(), LEN_IP);
      if (root.containsKey("stn_revertap"))   wificfg.stn_revertap = root["stn_revertap"].as<bool>();
      jsonbuf.clear();
      fp.close();
    }
  }

  // Read browser configuration
  {
    File fp = SPIFFS.open(FNAME_SERVICECFG, "r");
    if (fp) {
      size_t size = fp.size();
      std::unique_ptr<char[]> buf(new char[size]);
      fp.readBytes(buf.get(), size);
      JsonObject& root = jsonbuf.parseObject(buf.get());
      if (root.containsKey("hostname"))       strlcpy(servicecfg.hostname, root["hostname"].as<char *>(), LEN_HOSTNAME);
      if (root.containsKey("http_enabled"))   servicecfg.http_enabled = root["http_enabled"].as<bool>();
      if (root.containsKey("https_enabled"))  servicecfg.https_enabled = root["https_enabled"].as<bool>();
      if (root.containsKey("mdns_enabled"))   servicecfg.mdns_enabled = root["mdns_enabled"].as<bool>();
      if (root.containsKey("auth_enabled"))   servicecfg.auth_enabled = root["auth_enabled"].as<bool>();
      if (root.containsKey("auth_username"))  strlcpy(servicecfg.auth_username, root["auth_username"].as<char *>(), LEN_USERNAME);
      if (root.containsKey("auth_password"))  strlcpy(servicecfg.auth_password, root["auth_password"].as<char *>(), LEN_PASSWORD);
      if (root.containsKey("ota_enabled"))    servicecfg.ota_enabled = root["ota_enabled"].as<bool>();
      if (root.containsKey("ota_password"))   strlcpy(servicecfg.ota_password, root["ota_password"].as<char *>(), LEN_PASSWORD);
      jsonbuf.clear();
      fp.close();
    }
  }

  // Wifi connection
  {
    pinMode(WIFI_PIN, OUTPUT);
    digitalWrite(WIFI_PIN, HIGH);
    
    if (servicecfg.mdns_enabled)
      WiFi.hostname(servicecfg.hostname);
    
    if (wificfg.mode == M_STATION) {
      Serial.println("Station Mode");
      Serial.println(wificfg.stn_ssid);
      Serial.println(wificfg.stn_password);
      WiFi.mode(WIFI_STA);
      if (wificfg.stn_forceip[0] != 0 && wificfg.stn_forcesubnet[0] != 0 && wificfg.stn_forcegateway[0] != 0) {
        IPAddress addr, subnet, gateway;
        if (addr.fromString(wificfg.stn_forceip) && subnet.fromString(wificfg.stn_forcesubnet) && gateway.fromString(wificfg.stn_forcegateway)) {
          WiFi.config(addr, subnet, gateway);
        }
      }
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
          wificfg.mode = M_OFF;
        }
      } else {
         strlcpy(wificfg.ip, WiFi.localIP().toString().c_str(), LEN_IP);
         digitalWrite(WIFI_PIN, LOW);
      }
    }

    if (wificfg.mode == M_ACCESSPOINT) {
      Serial.println("AP Mode");
      Serial.println(wificfg.ap_ssid);
      Serial.println(wificfg.ap_password);
      WiFi.mode(WIFI_AP);
      WiFi.softAP(wificfg.ap_ssid, wificfg.ap_password, wificfg.ap_channel, wificfg.ap_hidden);
      strlcpy(wificfg.ip, WiFi.softAPIP().toString().c_str(), LEN_IP);
    }

    if (wificfg.mode == M_OFF) {
      WiFi.mode(WIFI_OFF);
    }
    
    Serial.println("Wifi connected");
    Serial.println(wificfg.ip);
  }

  // Initialize web services
  {
    if (servicecfg.mdns_enabled) {
      if (!MDNS.begin(servicecfg.hostname)) {
        Serial.println("Error: Could not start mDNS.");
      }
    }

    if (servicecfg.ota_enabled) {
      ArduinoOTA.setHostname(servicecfg.hostname);
      if (servicecfg.ota_password[0] != 0)
        ArduinoOTA.setPassword(servicecfg.ota_password);
      ArduinoOTA.begin();
    }

    if (servicecfg.http_enabled) {
      json_init();
      static_init();
      server.begin();
      websocket_init();
    }

    if (servicecfg.mqttcfg.enabled) {
      mqtt_init();
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
      if (root.containsKey("ocd"))        motorcfg.ocd = root["ocd"].as<float>();
      if (root.containsKey("ocdshutdown")) motorcfg.ocdshutdown = root["ocdshutdown"].as<bool>();
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
      if (root.containsKey("cm_switchperiod")) motorcfg.cm_switchperiod = root["cm_switchperiod"].as<float>();
      if (root.containsKey("cm_predict")) motorcfg.cm_predict = root["cm_predict"].as<bool>();
      if (root.containsKey("cm_minon"))   motorcfg.cm_minon = root["cm_minon"].as<float>();
      if (root.containsKey("cm_minoff"))  motorcfg.cm_minoff = root["cm_minoff"].as<float>();
      if (root.containsKey("cm_fastoff")) motorcfg.cm_fastoff = root["cm_fastoff"].as<float>();
      if (root.containsKey("cm_faststep")) motorcfg.cm_faststep = root["cm_faststep"].as<float>();
      if (root.containsKey("vm_pwmfreq")) motorcfg.vm_pwmfreq = root["vm_pwmfreq"].as<float>();
      if (root.containsKey("vm_stall"))   motorcfg.vm_stall = root["vm_stall"].as<float>();
      if (root.containsKey("vm_bemf_slopel")) motorcfg.vm_bemf_slopel = root["vm_bemf_slopel"].as<float>();
      if (root.containsKey("vm_bemf_speedco")) motorcfg.vm_bemf_speedco = root["vm_bemf_speedco"].as<float>();
      if (root.containsKey("vm_bemf_slopehacc")) motorcfg.vm_bemf_slopehacc = root["vm_bemf_slopehacc"].as<float>();
      if (root.containsKey("vm_bemf_slopehdec")) motorcfg.vm_bemf_slopehdec = root["vm_bemf_slopehdec"].as<float>();
      if (root.containsKey("reverse"))    motorcfg.reverse = root["reverse"].as<bool>();
      jsonbuf.clear();
      fp.close();
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

static volatile unsigned long last_statepoll = 0;
void loop() {
  HANDLE_CMDS();
  
  unsigned long now = millis();
  if ((now - last_statepoll) > 10) {
    ps_status status = ps_getstatus();
    motorst.pos = ps_getpos();
    motorst.mark = ps_getmark();
    motorst.stepss = ps_getspeed();
    motorst.busy = status.busy;
    last_statepoll = now;
  }

  if (servicecfg.daisycfg.enabled) {
    daisy_loop();
  }

  HANDLE_CMDS();

  if (servicecfg.http_enabled) {
    websocket.loop();
    server.handleClient();
  }

  HANDLE_CMDS();

  if (servicecfg.mqttcfg.enabled) {
    mqtt_loop(now);
  }

  HANDLE_CMDS();

  if (servicecfg.ota_enabled) {
    ArduinoOTA.handle();
  }

  // Reboot if requested
  if (flag_reboot)
    ESP.restart();

  if (wificfg.mode == M_ACCESSPOINT) {
    bool isoff = ((now / 200) % 20) == 1;
    digitalWrite(WIFI_PIN, isoff? HIGH : LOW);
  }
}

