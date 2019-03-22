#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "wifistepper.h"

extern WiFiClient * mqtt_conn;
extern PubSubClient * mqtt_client;
extern StaticJsonBuffer<2048> jsonbuf;

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  
}

bool mqtt_connect() {
  if (!mqtt_client->connected()) {
    if (mqtt_client->connect(PRODUCT " v" VERSION, config.service.mqtt.username, config.service.mqtt.key)) {
      mqtt_client->subscribe(config.service.mqtt.command_topic);
    } else {
      Serial.println("MQTT Not connected");
      return false;
    }
  }

  Serial.println("MQTT connected");
  return true;
}

void mqtt_init() {
  /*if (servicecfg.mqttcfg.secure) {
    mqtt_conn = new WiFiClientSecure;
  } else*/
  mqtt_conn = new WiFiClient;
  mqtt_client = new PubSubClient(*mqtt_conn);
  
  mqtt_client->setServer(config.service.mqtt.server, config.service.mqtt.port);
  mqtt_client->setCallback(mqtt_callback);
  mqtt_connect();
}

static volatile unsigned long last_connect = 0;
static volatile unsigned long last_publish = 0;
void mqtt_loop(unsigned long looptime) {
  bool connected = state.service.mqtt.connected = mqtt_client->connected();
  if (!connected && (looptime - last_connect) > TIME_MQTT_RECONNECT) {
    connected = state.service.mqtt.connected = mqtt_connect();
    last_connect = looptime;
  }

  mqtt_client->loop();

  if (connected && (looptime - last_publish) > config.service.mqtt.state_publish_period) {
    JsonObject& root = jsonbuf.createObject();
    root["position"] = motorcfg_pos(state.motor.pos);
    root["mark"] = motorcfg_pos(state.motor.mark);
    root["stepss"] = state.motor.stepss;
    root["busy"] = state.motor.busy;
    JsonVariant v = root;
    mqtt_client->publish(config.service.mqtt.state_topic, v.as<String>().c_str());
    jsonbuf.clear();
    last_publish = looptime;
  }
}

