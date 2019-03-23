#ifndef __WIFISTEPPER_H
#define __WIFISTEPPER_H

#include "powerstep01.h"

#define PRODUCT           "Wi-Fi Stepper"
#define VERSION           "1.0"

#define RESET_PIN         (5)
#define RESET_TIMEOUT     (3000)
#define WIFILED_PIN       (16)
#define MOTOR_ADCCOEFF    (2.65625)
#define MOTOR_CLOCK       (CLK_INT16)

#define FNAME_WIFICFG     "/wificfg.json"
#define FNAME_SERVICECFG  "/servicecfg.json"
#define FNAME_MOTORCFG    "/motorcfg.json"

#define PORT_HTTP     80
#define PORT_HTTPWS   81

#define LEN_HOSTNAME  24
#define LEN_USERNAME  64
#define LEN_URL       256
#define LEN_SSID      32
#define LEN_PASSWORD  64
#define LEN_IP        16
#define LEN_ID        2

#define TIME_MQTT_RECONNECT   30000

#define ID_START      (1)
typedef uint32_t id_t;
id_t nextid();
id_t currentid();

unsigned long timesince(unsigned long t1, unsigned long t2);


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
  wifi_mode mode;
  struct {
    char ssid[LEN_SSID];
    char password[LEN_PASSWORD];
    bool encryption;
    int channel;
    bool hidden;
  } accesspoint;
  struct {
    char ssid[LEN_SSID];
    char password[LEN_PASSWORD];
    bool encryption;
    char forceip[LEN_IP];
    char forcesubnet[LEN_IP];
    char forcegateway[LEN_IP];
    bool revertap;
  } station;
} wifi_config;

typedef struct {
  struct {
    bool enabled;
  } http;
  struct {
    bool enabled;
    char hostname[LEN_HOSTNAME];
  } mdns;
  struct {
    bool enabled;
    char username[LEN_USERNAME];
    char password[LEN_PASSWORD];
  } auth;
  struct {
    bool enabled;
    char password[LEN_PASSWORD];
  } ota;
  struct {
    bool enabled;
    bool master;
    int baudrate;
  } daisy;
  struct {
    bool enabled;
    char server[LEN_URL];
    int port;
    char username[LEN_USERNAME];
    char key[LEN_PASSWORD];
    char state_topic[LEN_URL];
    float state_publish_period;
    char command_topic[LEN_URL];
  } mqtt;
} service_config;

typedef struct {
  struct {
    bool usercontrol;
    bool is_output;
  } wifiled;
} io_config;

typedef struct {
  ps_mode mode;
  ps_stepsize stepsize;
  float ocd;
  bool ocdshutdown;
  float maxspeed;
  float minspeed;
  float accel, decel;
  float kthold;
  float ktrun;
  float ktaccel;
  float ktdecel;
  float fsspeed;
  bool fsboost;
  struct {
    float switchperiod;
    bool predict;
    float minon;
    float minoff;
    float fastoff;
    float faststep;
  } cm;
  struct {
    float pwmfreq;
    float stall;
    float bemf_slopel;
    float bemf_speedco;
    float bemf_slopehacc;
    float bemf_slopehdec;
  } vm;
  bool reverse;
} motor_config;

typedef struct {
  wifi_config wifi;
  service_config service;
  io_config io;
  motor_config motor;
} config_t;


#define ESUB_UNK      (0x00)
#define ESUB_WIFI     (0x01)
#define ESUB_CMD      (0x02)
#define ESUB_MOTOR    (0x03)
#define ESUB_DAISY    (0x04)

void seterror(uint8_t subsystem, id_t onid = 0, int type = 0);

typedef struct {
  bool errored;
  unsigned long when;
  uint8_t subsystem;
  id_t id;
  int type;
} error_state;

typedef struct {
  id_t this_command;
  id_t last_command;
  unsigned int last_completed;
} command_state;

typedef struct {
  char ip[LEN_IP];
} wifi_state;

typedef struct {
  struct {
    uint8_t numslaves;
    bool active;
    struct {
      unsigned long ping;
      unsigned long config;
      unsigned long state;
    } last;
  } daisy;
  struct {
    int connected;
    int status;
  } mqtt;
} service_state;

typedef struct {
  int pos, mark;
  float stepss;
  bool busy;
} motor_state;

typedef struct {
  error_state error;
  command_state command;
  wifi_state wifi;
  service_state service;
  motor_state motor;
} state_t;

typedef struct {
  io_config io;
  motor_config motor;
} daisy_slaveconfig;

typedef struct {
  error_state error;
  command_state command;
  motor_state motor;
} daisy_slavestate;

void cmd_init();
void cmd_loop();

// Commands for local Queue
//void cmd_put(id_t id, uint8_t opcode, uint8_t * data, size_t len);
bool cmd_nop(id_t id);
bool cmd_stop(id_t id, bool hiz, bool soft);
bool cmd_run(id_t id, ps_direction dir, float stepss);
bool cmd_stepclock(id_t id, ps_direction dir);
bool cmd_move(id_t id, ps_direction dir, uint32_t microsteps);
bool cmd_goto(id_t id, int32_t pos);
bool cmd_goto(id_t id, int32_t pos, ps_direction dir);
bool cmd_gountil(id_t id, ps_posact action, ps_direction dir, float stepss);
bool cmd_releasesw(id_t id, ps_posact action, ps_direction dir);
bool cmd_gohome(id_t id);
bool cmd_gomark(id_t id);
bool cmd_resetpos(id_t id);
bool cmd_setpos(id_t id, int32_t pos);
bool cmd_setmark(id_t id, int32_t mark);
bool cmd_setconfig(id_t id, const char * data);
bool cmd_waitbusy(id_t id);
bool cmd_waitrunning(id_t id);
bool cmd_waitms(id_t id, uint32_t millis);

bool cmd_empty(id_t id);
bool cmd_estop(id_t id, bool hiz, bool soft);



void daisy_init();
void daisy_loop(unsigned long now);
void daisy_check(unsigned long now);

// Remote queue commands
bool daisy_run(uint8_t address, id_t id, ps_direction dir, float stepss);
bool daisy_stepclock(uint8_t address, id_t id, ps_direction dir);
bool daisy_move(uint8_t address, id_t id, ps_direction dir, uint32_t microsteps);
bool daisy_goto(uint8_t address, id_t id, int32_t pos);
bool daisy_goto(uint8_t address, id_t id, int32_t pos, ps_direction dir);
bool daisy_gountil(uint8_t address, id_t id, ps_posact action, ps_direction dir, float stepss);
bool daisy_releasesw(uint8_t address, id_t id, ps_posact action, ps_direction dir);
bool daisy_gohome(uint8_t address, id_t id);
bool daisy_gomark(uint8_t address, id_t id);
bool daisy_resetpos(uint8_t address, id_t id);
bool daisy_setpos(uint8_t address, id_t id, int32_t pos);
bool daisy_setmark(uint8_t address, id_t id, int32_t mark);
bool daisy_setconfig(uint8_t address, id_t id, const char * data);
bool daisy_waitbusy(uint8_t address, id_t id);
bool daisy_waitrunning(uint8_t address, id_t id);
bool daisy_waitms(uint8_t address, id_t id, uint32_t millis);

bool daisy_empty(uint8_t address, id_t id);
bool daisy_estop(uint8_t address, id_t id, bool hiz, bool soft);


void wificfg_read(wifi_config * cfg);
void wificfg_write(wifi_config * const cfg);

void servicecfg_read(service_config * cfg);
void servicecfg_write(service_config * const cfg);

void motorcfg_read(motor_config * cfg);
void motorcfg_write(motor_config * const cfg);
void motorcfg_pull(motor_config * cfg);
void motorcfg_push(motor_config * const cfg);


void json_init();

void websocket_init();

void mqtt_init();
void mqtt_loop(unsigned long looptime);

// Utility functions
extern config_t config;
extern state_t state;

static inline ps_direction motorcfg_dir(ps_direction d) {
  if (!config.motor.reverse)  return d;
  switch (d) {
    case FWD: return REV;
    case REV: return FWD;
  }
}

static inline int motorcfg_pos(int32_t p) {
  return config.motor.reverse? -p : p;
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
