#ifndef __WIFISTEPPER_H
#define __WIFISTEPPER_H

#include "powerstep01.h"

#define PRODUCT           "Wi-Fi Stepper"
#define VERSION           "1.0"

#define RESET_PIN         (5)
#define RESET_TIMEOUT     (3000)
#define WIFI_LEDPIN       (16)
#define WIFI_ADCCOEFF     (0.003223) /*(0.003125)*/
#define MOTOR_ADCCOEFF    (2.65625)
#define MOTOR_CLOCK       (CLK_INT16)

#define FNAME_WIFICFG     "/wificfg.json"
#define FNAME_SERVICECFG  "/servicecfg.json"
#define FNAME_DAISYCFG    "/daisycfg.json"
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

#define ispacked __attribute__((packed))

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
  char hostname[LEN_HOSTNAME];
  struct {
    bool enabled;
  } http;
  struct {
    bool enabled;
  } mdns;
  struct {
    bool enabled;
    char username[LEN_USERNAME];
    char password[LEN_PASSWORD];
  } auth;
  struct {
    bool enabled;
  } lowtcp;
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
  struct {
    bool enabled;
    char password[LEN_PASSWORD];
  } ota;
} service_config;

typedef struct ispacked {
  struct {
    bool usercontrol;
    bool is_output;
  } wifiled;
} io_config;

typedef struct {
  bool enabled;
  bool master;
  bool slavewifioff;
} daisy_config;

typedef struct ispacked {
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
    bool volt_comp;
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
  daisy_config daisy;
  io_config io;
  motor_config motor;
} config_t;


#define ESUB_UNK      (0x00)
#define ESUB_WIFI     (0x01)
#define ESUB_CMD      (0x02)
#define ESUB_MOTOR    (0x03)
#define ESUB_DAISY    (0x04)
#define ESUB_LC       (0x05)
#define ESUB_HTTP     (0x06)

#define ETYPE_UNK     (0x00)
#define ETYPE_MEM     (0x01)
#define ETYPE_IBUF    (0x02)
#define ETYPE_OBUF    (0x03)
#define ETYPE_MSG     (0x04)

void seterror(uint8_t subsystem = ESUB_UNK, id_t onid = 0, int type = ETYPE_UNK, int8_t arg = -1);
void clearerror();

typedef struct ispacked {
  bool errored;
  unsigned long when;
  uint8_t subsystem;
  id_t id;
  int type;
  int8_t arg;
} error_state;

typedef struct ispacked {
  id_t this_command;
  id_t last_command;
  unsigned int last_completed;
} command_state;

typedef struct ispacked {
  wifi_mode mode;
  uint32_t ip;
  uint8_t mac[6];
  uint32_t chipid;
  long rssi;
} wifi_state;

typedef struct {
  struct {
    int clients;
  } lowtcp;
  struct {
    int connected;
    int status;
  } mqtt;
} service_state;

typedef struct {
  bool active;
  uint8_t slaves;
} daisy_state;

typedef struct ispacked {
  ps_status status;
  float stepss;
  int pos;
  int mark;
  float adc;
} motor_state;

typedef struct {
  error_state error;
  command_state command;
  wifi_state wifi;
  service_state service;
  daisy_state daisy;
  motor_state motor;
} state_t;

typedef struct {
  struct {
    unsigned long connection_check;
    unsigned long revertap;
    unsigned long rssi;
  } last;
} wifi_sketch;

typedef struct {
  struct {
    struct {
      unsigned long ping;
    } last;
  } lowtcp;
} service_sketch;

typedef struct ispacked {
  io_config io;
  motor_config motor;
} daisy_slaveconfig;

typedef struct ispacked {
  error_state error;
  command_state command;
  wifi_state wifi;
  motor_state motor;
} daisy_slavestate;

typedef struct {
  daisy_slaveconfig config;
  daisy_slavestate state;
} daisy_slave_t;

typedef struct {
  daisy_slave_t * slave;
  struct {
    unsigned long ping_rx;
    unsigned long ping_tx;
    unsigned long config;
    unsigned long state;
  } last;
} daisy_sketch;

typedef struct {
  struct {
    unsigned int status;
    unsigned int state;
  } last;
} motor_sketch;

typedef struct {
  wifi_sketch wifi;
  service_sketch service;
  daisy_sketch daisy;
  motor_sketch motor;
} sketch_t;


// Command structs
typedef struct ispacked {
  id_t id;
  uint8_t opcode;
} cmd_head_t;

typedef struct ispacked {
  bool hiz;
  bool soft;
} cmd_stop_t;

typedef struct ispacked {
  ps_direction dir;
  float stepss;
} cmd_run_t;

typedef struct ispacked {
  ps_direction dir;
} cmd_stepclk_t;

typedef struct ispacked {
  ps_direction dir;
  uint32_t microsteps;
} cmd_move_t;

typedef struct ispacked {
  bool hasdir;
  ps_direction dir;
  int32_t pos;
} cmd_goto_t;

typedef struct ispacked {
  ps_posact action;
  ps_direction dir;
  float stepss;
} cmd_gountil_t;

typedef struct ispacked {
  ps_posact action;
  ps_direction dir;
} cmd_releasesw_t;

typedef struct ispacked {
  int32_t pos;
} cmd_setpos_t;

typedef struct ispacked {
  uint32_t millis;
} cmd_waitms_t;

typedef struct ispacked {
  bool state;
} cmd_waitswitch_t;


void cmd_init();
void cmd_loop(unsigned long now);
void cmd_update(unsigned long now);

// Commands for local Queue
bool cmd_nop(id_t id);
bool cmd_stop(id_t id, bool hiz, bool soft);
bool cmd_run(id_t id, ps_direction dir, float stepss);
bool cmd_stepclock(id_t id, ps_direction dir);
bool cmd_move(id_t id, ps_direction dir, uint32_t microsteps);
//bool cmd_goto(id_t id, int32_t pos);
//bool cmd_goto(id_t id, int32_t pos, ps_direction dir);
bool cmd_goto(id_t id, int32_t pos, bool hasdir = false, ps_direction dir = FWD);
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
bool cmd_waitswitch(id_t id, bool state);

bool cmd_empty(id_t id);
bool cmd_estop(id_t id, bool hiz, bool soft);
void cmd_clearerror();


void daisy_init();
void daisy_loop(unsigned long now);
void daisy_update(unsigned long now);

// Remote management commands
bool daisy_clearerror(uint8_t address, id_t id);
bool daisy_wificontrol(uint8_t address, id_t id, bool enabled);

// Remote queue commands
bool daisy_stop(uint8_t address, id_t id, bool hiz, bool soft);
bool daisy_run(uint8_t address, id_t id, ps_direction dir, float stepss);
bool daisy_stepclock(uint8_t address, id_t id, ps_direction dir);
bool daisy_move(uint8_t address, id_t id, ps_direction dir, uint32_t microsteps);
//bool daisy_goto(uint8_t address, id_t id, int32_t pos);
//bool daisy_goto(uint8_t address, id_t id, int32_t pos, ps_direction dir);
bool daisy_goto(uint8_t address, id_t id, int32_t pos, bool hasdir = false, ps_direction dir = FWD);
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
bool daisy_waitswitch(uint8_t address, id_t id, bool state);

bool daisy_empty(uint8_t address, id_t id);
bool daisy_estop(uint8_t address, id_t id, bool hiz, bool soft);

static inline bool m_stop(uint8_t address, id_t id, bool hiz, bool soft) { if (address == 0) { return cmd_stop(id, hiz, soft); } else { return daisy_stop(address, id, hiz, soft); } }
static inline bool m_run(uint8_t address, id_t id, ps_direction dir, float stepss) { if (address == 0) { return cmd_run(id, dir, stepss); } else { return daisy_run(address, id, dir, stepss); } }
static inline bool m_stepclock(uint8_t address, id_t id, ps_direction dir) { if (address == 0) { return cmd_stepclock(id, dir); } else { return daisy_stepclock(address, id, dir); } }
static inline bool m_move(uint8_t address, id_t id, ps_direction dir, uint32_t microsteps) { if (address == 0) { return cmd_move(id, dir, microsteps); } else { return daisy_move(address, id, dir, microsteps); } }
//static inline bool m_goto(uint8_t address, id_t id, int32_t pos) { if (address == 0) { return cmd_goto(id, pos); } else { return daisy_goto(address, id, pos); } }
//static inline bool m_goto(uint8_t address, id_t id, int32_t pos, ps_direction dir) { if (address == 0) { return cmd_goto(id, pos, dir); } else { return daisy_goto(address, id, pos, dir); } }
static inline bool m_goto(uint8_t address, id_t id, int32_t pos, bool hasdir = false, ps_direction dir = FWD) { if (address == 0) { return cmd_goto(id, pos, hasdir, dir); } else { return daisy_goto(address, id, pos, hasdir, dir); } }
static inline bool m_gountil(uint8_t address, id_t id, ps_posact action, ps_direction dir, float stepss) { if (address == 0) { return cmd_gountil(id, action, dir, stepss); } else { return daisy_gountil(address, id, action, dir, stepss); } }
static inline bool m_releasesw(uint8_t address, id_t id, ps_posact action, ps_direction dir) { if (address == 0) { return cmd_releasesw(id, action, dir); } else { return daisy_releasesw(address, id, action, dir); } }
static inline bool m_gohome(uint8_t address, id_t id) { if (address == 0) { return cmd_gohome(id); } else { return daisy_gohome(address, id); } }
static inline bool m_gomark(uint8_t address, id_t id) { if (address == 0) { return cmd_gomark(id); } else { return daisy_gomark(address, id); } }
static inline bool m_resetpos(uint8_t address, id_t id) { if (address == 0) { return cmd_resetpos(id); } else { return daisy_resetpos(address, id); } }
static inline bool m_setpos(uint8_t address, id_t id, int32_t pos) { if (address == 0) { return cmd_setpos(id, pos); } else { return daisy_setpos(address, id, pos); } }
static inline bool m_setmark(uint8_t address, id_t id, int32_t mark) { if (address == 0) { return cmd_setmark(id, mark); } else { return daisy_setmark(address, id, mark); } }
static inline bool m_setconfig(uint8_t address, id_t id, const char * data) { if (address == 0) { return cmd_setconfig(id, data); } else { return daisy_setconfig(address, id, data); } }
static inline bool m_waitbusy(uint8_t address, id_t id) { if (address == 0) { return cmd_waitbusy(id); } else { return daisy_waitbusy(address, id); } }
static inline bool m_waitrunning(uint8_t address, id_t id) { if (address == 0) { return cmd_waitrunning(id); } else { return daisy_waitrunning(address, id); } }
static inline bool m_waitms(uint8_t address, id_t id, uint32_t millis) { if (address == 0) { return cmd_waitms(id, millis); } else { return daisy_waitms(address, id, millis); } }
static inline bool m_waitswitch(uint8_t address, id_t id, bool state) { if (address == 0) { return cmd_waitswitch(id, state); } else { return daisy_waitswitch(address, id, state); } }
static inline bool m_empty(uint8_t address, id_t id) { if (address == 0) { return cmd_empty(id); } else { return daisy_empty(address, id); } }
static inline bool m_estop(uint8_t address, id_t id, bool hiz, bool soft) { if (address == 0) { return cmd_estop(id, hiz, soft); } else { return daisy_estop(address, id, hiz, soft); } }


void lowtcp_init();
void lowtcp_loop(unsigned long now);
void lowtcp_update(unsigned long now);


void wificfg_read(wifi_config * cfg);
void wificfg_write(wifi_config * const cfg);
bool wificfg_connect(wifi_mode mode, wifi_config * const cfg);
void wificfg_update(unsigned long now);

void servicecfg_read(service_config * cfg);
void servicecfg_write(service_config * const cfg);

void daisycfg_read(daisy_config * cfg);
void daisycfg_write(daisy_config * const cfg);

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
extern sketch_t sketch;

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
