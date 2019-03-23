#include <Arduino.h>
#include <ArduinoJson.h>

#include "powerstep01.h"
#include "wifistepper.h"
#include "cmdpriv.h"

//#define CMD_DEBUG

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


#define CTO_UPDATE      (10)


extern StaticJsonBuffer<2048> jsonbuf;
extern volatile bool flag_cmderror;

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

void cmd_loop(unsigned long now) {
  state.command.this_command = 0;
  ESP.wdtFeed();

  while (Qlen > 0) {
    cmd_head_t * head = (cmd_head_t *)Q;
    void * Qcmd = (void *)(&Q[sizeof(cmd_head_t)]);
    
    state.command.this_command = head->id;
    size_t consume = sizeof(cmd_head_t);

    cmd_debug(head->id, head->opcode, "Try exec");

    // Check pre-conditions
    if (head->opcode & (Q_PRE_BUSY | Q_PRE_STOP)) {
      state.motor.status = ps_getstatus();
      sketch.motor.last.status = now;
    }
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
        size_t ldata = strlen(data);
        bool save = false;
        if (ldata > 0) {
          JsonObject& root = jsonbuf.parseObject(data);
          if (root.containsKey("mode"))       config.motor.mode = parse_motormode(root["mode"], config.motor.mode);
          if (root.containsKey("stepsize"))   config.motor.stepsize = parse_stepsize(root["stepsize"].as<int>(), config.motor.stepsize);
          if (root.containsKey("ocd"))        config.motor.ocd = root["ocd"].as<float>();
          if (root.containsKey("ocdshutdown")) config.motor.ocdshutdown = root["ocdshutdown"].as<bool>();
          if (root.containsKey("maxspeed"))   config.motor.maxspeed = root["maxspeed"].as<float>();
          if (root.containsKey("minspeed"))   config.motor.minspeed = root["minspeed"].as<float>();
          if (root.containsKey("accel"))      config.motor.accel = root["accel"].as<float>();
          if (root.containsKey("decel"))      config.motor.decel = root["decel"].as<float>();
          if (root.containsKey("kthold"))     config.motor.kthold = root["kthold"].as<float>();
          if (root.containsKey("ktrun"))      config.motor.ktrun = root["ktrun"].as<float>();
          if (root.containsKey("ktaccel"))    config.motor.ktaccel = root["ktaccel"].as<float>();
          if (root.containsKey("ktdecel"))    config.motor.ktdecel = root["ktdecel"].as<float>();
          if (root.containsKey("fsspeed"))    config.motor.fsspeed = root["fsspeed"].as<float>();
          if (root.containsKey("fsboost"))    config.motor.fsboost = root["fsboost"].as<bool>();
          if (root.containsKey("cm_switchperiod")) config.motor.cm.switchperiod = root["cm_switchperiod"].as<float>();
          if (root.containsKey("cm_predict")) config.motor.cm.predict = root["cm_predict"].as<bool>();
          if (root.containsKey("cm_minon"))   config.motor.cm.minon = root["cm_minon"].as<float>();
          if (root.containsKey("cm_minoff"))  config.motor.cm.minoff = root["cm_minoff"].as<float>();
          if (root.containsKey("cm_fastoff")) config.motor.cm.fastoff = root["cm_fastoff"].as<float>();
          if (root.containsKey("cm_faststep")) config.motor.cm.faststep = root["cm_faststep"].as<float>();
          if (root.containsKey("vm_pwmfreq")) config.motor.vm.pwmfreq = root["vm_pwmfreq"].as<float>();
          if (root.containsKey("vm_stall"))   config.motor.vm.stall = root["vm_stall"].as<float>();
          if (root.containsKey("vm_bemf_slopel")) config.motor.vm.bemf_slopel = root["vm_bemf_slopel"].as<float>();
          if (root.containsKey("vm_bemf_speedco")) config.motor.vm.bemf_speedco = root["vm_bemf_speedco"].as<float>();
          if (root.containsKey("vm_bemf_slopehacc")) config.motor.vm.bemf_slopehacc = root["vm_bemf_slopehacc"].as<float>();
          if (root.containsKey("vm_bemf_slopehdec")) config.motor.vm.bemf_slopehdec = root["vm_bemf_slopehdec"].as<float>();
          if (root.containsKey("reverse"))    config.motor.reverse = root["reverse"].as<bool>();
          save = root.containsKey("save") && root["save"].as<bool>();
          jsonbuf.clear();
        }
        motorcfg_push(&config.motor);
        if (save) motorcfg_write(&config.motor);
        consume += ldata + 1;
        break;
      }
      case CMD_WAITMS: {
        consume += sizeof(sketch_waitms_t);
        break;
      }
    }

    memmove(Q, &Q[consume], Qlen - consume);
    Qlen -= consume;
    state.command.last_command = head->id;
    state.command.last_completed = millis();

    cmd_debug(head->id, head->opcode, "Exec complete");
    ESP.wdtFeed();
  }
}

void cmd_update(unsigned long now) {
  if (timesince(sketch.motor.last.status, now) > CTO_UPDATE) {
    sketch.motor.last.status = now;
    state.motor.status = ps_getstatus();
    state.motor.status.direction = motorcfg_dir(state.motor.status.direction);
  }
  
  if (timesince(sketch.motor.last.state, now) > CTO_UPDATE) {
    sketch.motor.last.state = now;
    state.motor.stepss = ps_getspeed();
    state.motor.pos = motorcfg_pos(ps_getpos());
    state.motor.mark = motorcfg_pos(ps_getmark());
    state.motor.adc = (float)ps_readadc() * MOTOR_ADCCOEFF;
  }
}

/*void cmd_put(id_t id, uint8_t opcode, uint8_t * data, size_t len) {
  void * buf = cmd_alloc(id, opcode, len);
  if (buf != NULL) memcpy(buf, data, data);
  return buf != NULL;
}*/

static void * cmd_alloc(id_t id, uint8_t opcode, size_t len) {
  if ((Q_SIZE - Qlen) < (sizeof(cmd_head_t) + len)) {
    seterror(ESUB_CMD, id);
    return NULL;
  }
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

