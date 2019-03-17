#ifndef __WIFISTEPPER_H
#define __WIFISTEPPER_H

#include "powerstep01.h"

#define PRODUCT           "Wi-Fi Stepper"
#define VERSION           "1.0"

#define RESET_PIN         (5)
#define RESET_TIMEOUT     (3000)
#define WIFI_PIN          (16)
#define MOTOR_ADCCOEFF    (2.65625)
#define MOTOR_CLOCK       (CLK_INT16)

#define FNAME_WIFICFG     "/wificfg.json"
#define FNAME_BROWSERCFG  "/browsercfg.json"
#define FNAME_MOTORCFG    "/motorcfg.json"

#define PORT_HTTP     80
#define PORT_HTTPWS   81

#define LEN_HOSTNAME  24
#define LEN_USERNAME  64
#define LEN_URL       256
#define LEN_SSID      32
#define LEN_PASSWORD  64
#define LEN_IP        16

#define TIME_MQTT_RECONNECT   30000


typedef enum {
  M_OFF = 0x0,
  M_ACCESSPOINT = 0x1,
  M_STATION = 0x2
} wifi_mode;

typedef enum {
  WS_READSTATUS = 0x11,
  WS_READSTATE = 0x12,
  
  WS_CMDSTOP = 0x21,
  WS_CMDHIZ = 0x22,
  WS_CMDGOTO = 0x23,
  WS_CMDRUN = 0x24,
  WS_STEPCLOCK = 0x25,

  WS_POS = 0x31,
} ws_opcode;


typedef struct {
  bool enabled;
  char server[LEN_URL];
  int port;
  char username[LEN_USERNAME];
  char key[LEN_PASSWORD];
  
  char endpoint_state[LEN_URL];
  char endpoint_command[LEN_URL];

  unsigned long period_state;
} mqtt_config;

typedef struct {
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
  char stn_forcesubnet[LEN_IP];
  char stn_forcegateway[LEN_IP];
  bool stn_revertap;
  char ip[LEN_IP];
} wifi_config;

typedef struct {
  char hostname[LEN_HOSTNAME];
  bool http_enabled;
  bool https_enabled;
  bool mdns_enabled;
  bool auth_enabled;
  char auth_username[LEN_USERNAME];
  char auth_password[LEN_PASSWORD];
  bool ota_enabled;
  char ota_password[LEN_PASSWORD];
  mqtt_config mqttcfg;
} browser_config;

typedef struct {
  ps_mode mode;
  ps_stepsize stepsize;
  float ocd;
  bool ocdshutdown;
  float maxspeed, minspeed;
  float accel, decel;
  float kthold, ktrun, ktaccel, ktdecel;
  float fsspeed;
  bool fsboost;
  float cm_switchperiod;
  bool cm_predict;
  float cm_minon;
  float cm_minoff;
  float cm_fastoff;
  float cm_faststep;
  float vm_pwmfreq;
  float vm_stall;
  float vm_bemf_slopel;
  float vm_bemf_speedco;
  float vm_bemf_slopehacc;
  float vm_bemf_slopehdec;
  bool reverse;
} motor_config;

typedef struct {
  int pos, mark;
  float stepss;
  bool busy;
} motor_state;


void wificfg_save();
void browsercfg_save();

void motorcfg_read();
void motorcfg_update();
void motorcfg_save();

void json_init();

void websocket_init();

void mqtt_init();
void mqtt_loop(unsigned long looptime);

// Utility functions
extern wifi_config wificfg;
extern browser_config browsercfg;
extern motor_config motorcfg;

static inline ps_direction motorcfg_dir(ps_direction d) {
  if (!motorcfg.reverse)  return d;
  switch (d) {
    case FWD: return REV;
    case REV: return FWD;
  }
}

static inline int motorcfg_pos(int p) {
  return motorcfg.reverse? -p : p;
}

static inline wifi_mode parse_wifimode(const char * m) {
  if (strcmp(m, "station") == 0)  return M_STATION;
  else                            return M_ACCESSPOINT;
}

static inline int parse_channel(const String& c, int d) {
  int i = c.toInt();
  return i > 0 && i <= 13? i : d;
}

static inline ps_mode parse_motormode(const String& m, ps_mode d) {
  if (m == "voltage")       return MODE_VOLTAGE;
  else if (m == "current")  return MODE_CURRENT;
  else                      return d;
}

static inline ps_stepsize parse_stepsize(int s, ps_stepsize d) {
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

static inline ps_direction parse_direction(const String& s, ps_direction d) {
  if (s == "forward")       return FWD;
  else if (s == "reverse")  return REV;
  else                      return d;
}

// JSON functions
static inline const char * json_serialize(wifi_mode m) {
  switch (m) {
    case M_OFF:         return "off";
    case M_ACCESSPOINT: return "accesspoint";
    case M_STATION:     return "station";
    default:            return "";
  }
}

static inline const char * json_serialize(ps_mode m) {
  switch (m) {
    case MODE_VOLTAGE:  return "voltage";
    case MODE_CURRENT:  return "current";
    default:            return "";
  }
}

static inline int json_serialize(ps_stepsize s) {
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

static inline const char * json_serialize(ps_direction d) {
  switch (d) {
    case FWD:   return "forward";
    case REV:   return "reverse";
    default:    return "";
  }
}

static inline const char * json_serialize(ps_movement m) {
  switch (m) {
    case M_STOPPED:     return "idle";
    case M_ACCEL:       return "accelerating";
    case M_DECEL:       return "decelerating";
    case M_CONSTSPEED:  return "spinning";
    default:            return "";
  }
}


#endif

