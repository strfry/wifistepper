#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "wifistepper.h"

extern WiFiClient mqtt_conn;
extern PubSubClient mqtt_client;
extern StaticJsonBuffer<2048> jsonbuf;

extern browser_config browsercfg;

extern motor_state statecache;

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  
}

bool mqtt_connect() {
  if (!mqtt_client.connected()) {
    if (mqtt_client.connect("", browsercfg.mqttcfg.username, browsercfg.mqttcfg.key)) {
      mqtt_client.subscribe(browsercfg.mqttcfg.endpoint_command);
    } else {
      Serial.println("MQTT Not connected");
      return false;
    }
  }

  Serial.println("MQTT connected");
  return true;
}

void mqtt_init() {
  mqtt_client.setServer(browsercfg.mqttcfg.server, browsercfg.mqttcfg.port);
  mqtt_client.setCallback(mqtt_callback);
  mqtt_connect();
}

static volatile unsigned long last_connect = 0;
static volatile unsigned long last_publish = 0;
void mqtt_loop(unsigned long looptime) {
  bool connected = mqtt_client.connected();
  if (!connected && (looptime - last_connect) > TIME_MQTT_RECONNECT) {
    connected = mqtt_connect();
    last_connect = looptime;
  }

  if (connected && (looptime - last_publish) > browsercfg.mqttcfg.period_state) {
    Serial.println("publish mqtt");
    JsonObject& root = jsonbuf.createObject();
    root["position"] = motorcfg_pos(statecache.pos);
    root["mark"] = motorcfg_pos(statecache.mark);
    root["stepss"] = statecache.stepss;
    root["busy"] = statecache.busy;
    root["status"] = "ok";
    JsonVariant v = root;
    mqtt_client.publish(browsercfg.mqttcfg.endpoint_state, v.as<String>().c_str());
    jsonbuf.clear();
    last_publish = looptime;
  }

  mqtt_client.loop();
}

