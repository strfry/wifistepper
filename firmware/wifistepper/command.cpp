#include <Arduino.h>
#include <ArduinoJson.h>

#include "powerstep01.h"
#include "wifistepper.h"
#include "cmdpriv.h"

#define CMD_DEBUG

#define Q_SIZE          (2048)

#define Q_PRE_NONE      (0x00)
#define Q_PRE_BUSY      (0x20)
#define Q_PRE_STOP      (0x40)

#define CMD_NOP         (Q_PRE_NONE | 0x00)
#define CMD_STOP        (Q_PRE_NONE | 0x01)
#define CMD_RUN         (Q_PRE_NONE | 0x02)
#define CMD_STEPCLK     (Q_PRE_STOP | 0x03)
#define CMD_MOVE        (Q_PRE_STOP | 0x04)
#define CMD_GOTO        (Q_PRE_BUSY | 0x05)
#define CMD_GOUNTIL     (Q_PRE_NONE | 0x06)
#define CMD_RELEASESW   (Q_PRE_BUSY | 0x07)
#define CMD_GOHOME      (Q_PRE_BUSY | 0x08)
#define CMD_GOMARK      (Q_PRE_BUSY | 0x09)
#define CMD_RESETPOS    (Q_PRE_STOP | 0x0A)
#define CMD_SETPOS      (Q_PRE_NONE | 0x0B)
#define CMD_SETMARK     (Q_PRE_NONE | 0x0C)
#define CMD_SETCONFIG   (Q_PRE_STOP | 0x0D)
#define CMD_WAITBUSY    (Q_PRE_BUSY | 0x0E)
#define CMD_WAITRUNNING (Q_PRE_STOP | 0x0F)
#define CMD_WAITMS      (Q_PRE_NONE | 0x10)


extern volatile bool flag_cmderror;
extern volatile command_state commandst;
extern StaticJsonBuffer<2048> jsonbuf;

uint8_t Q[Q_SIZE] = {0};
volatile size_t Qlen = 0;

typedef struct {
  cmd_waitms_t cmd;
  unsigned int timeout;
} sketch_waitms_t;

#ifdef CMD_DEBUG
void cmd_debug(id_t id, uint8_t opcode, const char * msg) {
  Serial.print("Q(");
  Serial.print(id);
  Serial.print(", ");
  Serial.print(millis());
  Serial.print(") 0x");
  Serial.print(opcode, HEX);
  Serial.print(" -> ");
  Serial.println(msg);
}
#else
#define cmd_debug(...)
#endif

void cmd_init() {
  
}

void cmd_loop() {
  commandst.this_command = 0;
  ESP.wdtFeed();

  while (Qlen > 0) {
    cmd_head_t * head = (cmd_head_t *)Q;
    void * Qcmd = (void *)(&Q[sizeof(cmd_head_t)]);
    
    commandst.this_command = head->id;
    size_t consume = sizeof(cmd_head_t);

    cmd_debug(head->id, head->opcode, "Try exec");

    // Check pre-conditions
    if ((head->opcode & Q_PRE_BUSY) && ps_isbusy()) return;
    if ((head->opcode & Q_PRE_STOP) && ps_isrunning()) return;
      
    switch (head->opcode) {
      case CMD_NOP:
      case CMD_WAITBUSY:
      case CMD_WAITRUNNING: {
        // Do nothing
        //ps_nop();
        break;
      }
      case CMD_STOP: {
        cmd_stop_t * cmd = (cmd_stop_t *)Qcmd;
        if (cmd->hiz) {
          if (cmd->soft)  ps_softhiz();
          else            ps_hardhiz();
        } else {
          if (cmd->soft)  ps_softstop();
          else            ps_hardstop();
        }
        consume += sizeof(cmd_stop_t);
        break;
      }
      case CMD_RUN: {
        cmd_run_t * cmd = (cmd_run_t *)Qcmd;
        ps_run(motorcfg_dir(cmd->dir), cmd->stepss);
        consume += sizeof(cmd_run_t);
        break;
      }
      case CMD_STEPCLK: {
        cmd_stepclk_t * cmd = (cmd_stepclk_t *)Qcmd;
        ps_stepclock(motorcfg_dir(cmd->dir));
        consume += sizeof(cmd_stepclk_t);
        break;
      }
      case CMD_MOVE: {
        cmd_move_t * cmd = (cmd_move_t *)Qcmd;
        ps_move(motorcfg_dir(cmd->dir), cmd->microsteps);
        consume += sizeof(cmd_move_t);
        break;
      }
      case CMD_GOTO: {
        cmd_goto_t * cmd = (cmd_goto_t *)Qcmd;
        if (cmd->hasdir)  ps_goto(motorcfg_pos(cmd->pos), motorcfg_dir(cmd->dir));
        else              ps_goto(motorcfg_pos(cmd->pos));
        consume += sizeof(cmd_goto_t);
        break;
      }
      case CMD_GOUNTIL: {
        cmd_gountil_t * cmd = (cmd_gountil_t *)Qcmd;
        ps_gountil(cmd->action, motorcfg_dir(cmd->dir), cmd->stepss);
        consume += sizeof(cmd_gountil_t);
        break;
      }
      case CMD_RELEASESW: {
        cmd_releasesw_t * cmd = (cmd_releasesw_t *)Qcmd;
        ps_releasesw(cmd->action, motorcfg_dir(cmd->dir));
        consume += sizeof(cmd_releasesw_t);
        break;
      }
      case CMD_GOHOME: {
        ps_gohome();
        break;
      }
      case CMD_GOMARK: {
        ps_gomark();
        break;
      }
      case CMD_RESETPOS: {
        ps_resetpos();
        break;
      }
      case CMD_SETPOS: {
        cmd_setpos_t * cmd = (cmd_setpos_t *)Qcmd;
        ps_setpos(cmd->pos);
        consume += sizeof(cmd_setpos_t);
        break;
      }
      case CMD_SETMARK: {
        cmd_setpos_t * cmd = (cmd_setpos_t *)Qcmd;
        ps_setmark(cmd->pos);
        consume += sizeof(cmd_setpos_t);
        break;
      }
      case CMD_SETCONFIG: {
        const char * data = (const char *)Qcmd;
        JsonObject& root = jsonbuf.parseObject(data);
        if (root.containsKey("mode"))       motorcfg.mode = parse_motormode(root["mode"], motorcfg.mode);
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
        motorcfg_update();
        if (root.containsKey("save") && root["save"].as<bool>()) {
          motorcfg_save();
        }
        jsonbuf.clear();
        consume += strlen(data) + 1;
        break;
      }
      case CMD_WAITMS: {
        consume += sizeof(sketch_waitms_t);
        break;
      }
    }

    memmove(Q, &Q[consume], Qlen - consume);
    Qlen -= consume;
    commandst.last_command = head->id;
    commandst.last_completed = millis();

    cmd_debug(head->id, head->opcode, "Exec complete");
    ESP.wdtFeed();
  }
}

/*void cmd_put(uint8_t * data, size_t len) {
  check_Q(len)
  memcpy(&Q[Qlen], data, len);
  Qlen += len;
}*/

static void * cmd_alloc(id_t id, uint8_t opcode, size_t len) {
  if ((Q_SIZE - Qlen) < (sizeof(cmd_head_t) + len)) return NULL;
  *(cmd_head_t *)(&Q[Qlen]) = { .id = id, .opcode = opcode };
  void * p = &Q[Qlen + sizeof(cmd_head_t)];
  Qlen += sizeof(cmd_head_t) + len;
  return p;
}

bool cmd_nop(id_t id) {
  return cmd_alloc(id, CMD_NOP, 0) != NULL;
}

bool cmd_stop(id_t id, bool hiz, bool soft) {
  cmd_stop_t * cmd = (cmd_stop_t *)cmd_alloc(id, CMD_STOP, sizeof(cmd_stop_t));
  if (cmd != NULL) *cmd = { .hiz = hiz, .soft = soft };
  return cmd != NULL;
}

bool cmd_run(id_t id, ps_direction dir, float stepss) {
  cmd_run_t * cmd = (cmd_run_t *)cmd_alloc(id, CMD_RUN, sizeof(cmd_run_t));
  if (cmd != NULL) *cmd = { .dir = dir, .stepss = stepss };
  return cmd != NULL;
}

bool cmd_stepclock(id_t id, ps_direction dir) {
  cmd_stepclk_t * cmd = (cmd_stepclk_t *)cmd_alloc(id, CMD_STEPCLK, sizeof(cmd_stepclk_t));
  if (cmd != NULL) *cmd = { .dir = dir };
  return cmd != NULL;
}

bool cmd_move(id_t id, ps_direction dir, uint32_t microsteps) {
  cmd_move_t * cmd = (cmd_move_t *)cmd_alloc(id, CMD_MOVE, sizeof(cmd_move_t));
  if (cmd != NULL) *cmd = { .dir = dir, .microsteps = microsteps };
  return cmd != NULL;
}

bool cmd_goto(id_t id, int32_t pos) {
  cmd_goto_t * cmd = (cmd_goto_t *)cmd_alloc(id, CMD_GOTO, sizeof(cmd_goto_t));
  if (cmd != NULL) *cmd = { .hasdir = false, .dir = FWD, .pos = pos };
  return cmd != NULL;
}

bool cmd_goto(id_t id, int32_t pos, ps_direction dir) {
  cmd_goto_t * cmd = (cmd_goto_t *)cmd_alloc(id, CMD_GOTO, sizeof(cmd_goto_t));
  if (cmd != NULL) *cmd = { .hasdir = true, .dir = dir, .pos = pos };
  return cmd != NULL;
}

bool cmd_gountil(id_t id, ps_posact action, ps_direction dir, float stepss) {
  cmd_gountil_t * cmd = (cmd_gountil_t *)cmd_alloc(id, CMD_GOUNTIL, sizeof(cmd_gountil_t));
  if (cmd != NULL) *cmd = { .action = action, .dir = dir, .stepss = stepss };
  return cmd != NULL;
}

bool cmd_releasesw(id_t id, ps_posact action, ps_direction dir) {
  cmd_releasesw_t * cmd = (cmd_releasesw_t *)cmd_alloc(id, CMD_RELEASESW, sizeof(cmd_releasesw_t));
  if (cmd != NULL) *cmd = { .action = action, .dir = dir };
  return cmd != NULL;
}

bool cmd_gohome(id_t id) {
  return cmd_alloc(id, CMD_GOHOME, 0) != NULL;
}

bool cmd_gomark(id_t id) {
  return cmd_alloc(id, CMD_GOMARK, 0) != NULL;
}

bool cmd_resetpos(id_t id) {
  return cmd_alloc(id, CMD_RESETPOS, 0) != NULL;
}

bool cmd_setpos(id_t id, int32_t pos) {
  cmd_setpos_t * cmd = (cmd_setpos_t *)cmd_alloc(id, CMD_SETPOS, sizeof(cmd_setpos_t));
  if (cmd != NULL) *cmd = { .pos = pos };
  return cmd != NULL;
}

bool cmd_setmark(id_t id, int32_t mark) {
  cmd_setpos_t * cmd = (cmd_setpos_t *)cmd_alloc(id, CMD_SETMARK, sizeof(cmd_setpos_t));
  if (cmd != NULL) *cmd = { .pos = mark };
  return cmd != NULL;
}

bool cmd_setconfig(id_t id, const char * data) {
  size_t ldata = strlen(data) + 1;
  void * buf = cmd_alloc(id, CMD_SETCONFIG, ldata);
  if (buf != NULL) memcpy(buf, data, ldata);
  return buf != NULL;
}

bool cmd_waitbusy(id_t id) {
  return cmd_alloc(id, CMD_WAITBUSY, 0) != NULL;
}

bool cmd_waitrunning(id_t id) {
  return cmd_alloc(id, CMD_WAITRUNNING, 0) != NULL;
}

bool cmd_waitms(id_t id, uint32_t millis) {
  sketch_waitms_t * cmd = (sketch_waitms_t *)cmd_alloc(id, CMD_WAITMS, sizeof(sketch_waitms_t));
  if (cmd != NULL) *cmd = { .cmd = { .millis = millis }, .timeout = 0 };
  return cmd != NULL;
}



bool cmd_empty(id_t id) {
  cmd_debug(id, 0xFF, "Empty queue");
  Qlen = 0;
  return cmd_nop(id);
}

bool cmd_estop(id_t id, bool hiz, bool soft) {
  cmd_debug(id, 0xFF, "Estop");
  if (hiz) {
    if (soft)   ps_softhiz();
    else        ps_hardhiz();
  } else {
    if (soft)   ps_softstop();
    else        ps_hardstop();
  }
  return cmd_empty(id);
}

