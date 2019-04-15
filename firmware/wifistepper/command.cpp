#include <Arduino.h>
#include <ArduinoJson.h>

#include "powerstep01.h"
#include "wifistepper.h"

//#define CMD_DEBUG

#define QPRE_NONE       (0x00)
#define QPRE_STATUS     (0x20)
#define QPRE_NOTBUSY    (0x40)
#define QPRE_STOPPED    (0x80)

#define CMD_NOP         (QPRE_NONE | 0x00)
#define CMD_STOP        (QPRE_NONE | 0x01)
#define CMD_RUN         (QPRE_NONE | 0x02)
#define CMD_STEPCLK     (QPRE_STOPPED | 0x03)
#define CMD_MOVE        (QPRE_STOPPED | 0x04)
#define CMD_GOTO        (QPRE_NOTBUSY | 0x05)
#define CMD_GOUNTIL     (QPRE_NONE | 0x06)
#define CMD_RELEASESW   (QPRE_NOTBUSY | 0x07)
#define CMD_GOHOME      (QPRE_NOTBUSY | 0x08)
#define CMD_GOMARK      (QPRE_NOTBUSY | 0x09)
#define CMD_RESETPOS    (QPRE_STOPPED | 0x0A)
#define CMD_SETPOS      (QPRE_NONE | 0x0B)
#define CMD_SETMARK     (QPRE_NONE | 0x0C)
#define CMD_SETCONFIG   (QPRE_STOPPED | 0x0D)
#define CMD_WAITBUSY    (QPRE_NOTBUSY | 0x0E)
#define CMD_WAITRUNNING (QPRE_STOPPED | 0x0F)
#define CMD_WAITMS      (QPRE_NONE | 0x10)
#define CMD_WAITSWITCH  (QPRE_STATUS | 0x11)


#define CTO_UPDATE      (10)


extern StaticJsonBuffer<2048> jsonbuf;

#define Q0_SIZE       (2048)
#define Q_SIZE        (512)

uint8_t __q0[Q0_SIZE];
uint8_t __q[QS_SIZE-1][Q_SIZE];


typedef struct ispacked {
  uint32_t ms;
  unsigned int started;
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
  // Initialize queues
  queue[0] = { .len = 0, .maxlen = Q0_SIZE, .Q = __q0 };
  for (size_t i = 1; i < QS_SIZE; i++) {
    queue[i] = { .len = 0, .maxlen = Q_SIZE, .Q = __q[i-1] };
  }
}

void cmd_loop(unsigned long now) {
  state.command.this_command = 0;
  ESP.wdtFeed();

  while (Q0->len > 0) {
    cmd_head_t * head = (cmd_head_t *)(Q0->Q);
    void * Qcmd = (void *)&(Q0->Q[sizeof(cmd_head_t)]);
    
    state.command.this_command = head->id;
    size_t consume = sizeof(cmd_head_t);

    cmd_debug(head->id, head->opcode, "Try exec");

    // Check pre-conditions
    if (head->opcode & (QPRE_STATUS | QPRE_NOTBUSY | QPRE_STOPPED)) {
      sketch.motor.last.status = now;
      state.motor.status = ps_getstatus();

      if ((head->opcode & QPRE_NOTBUSY) && state.motor.status.busy) return;
      if ((head->opcode & QPRE_STOPPED) && state.motor.status.movement != M_STOPPED) return;
    }
      
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
          if (root.containsKey("vm_volt_comp")) config.motor.vm.volt_comp = root["vm_volt_comp"].as<bool>();
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
        sketch_waitms_t * sketch = (sketch_waitms_t *)Qcmd;
        if (sketch->started == 0) sketch->started = millis();
        if (timesince(sketch->started, millis()) < sketch->ms) return;
        consume += sizeof(sketch_waitms_t);
        break;
      }
      case CMD_WAITSWITCH: {
        cmd_waitswitch_t * cmd = (cmd_waitswitch_t *)Qcmd;
        if (cmd->state != state.motor.status.user_switch) return;
        consume += sizeof(cmd_waitswitch_t);
        break;
      }
    }

    memmove(Q0->Q, &(Q0->Q[consume]), Q0->len - consume);
    Q0->len -= consume;
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
    
    bool iserror = state.motor.status.alarms.command_error || state.motor.status.alarms.overcurrent || state.motor.status.alarms.undervoltage || state.motor.status.alarms.thermal_shutdown;
    if (iserror) {
      seterror(ESUB_MOTOR);
    }
  }
  
  if (timesince(sketch.motor.last.state, now) > CTO_UPDATE) {
    sketch.motor.last.state = now;
    state.motor.stepss = ps_getspeed();
    state.motor.pos = motorcfg_pos(ps_getpos());
    state.motor.mark = motorcfg_pos(ps_getmark());
    state.motor.adc = (float)ps_readadc() * MOTOR_ADCCOEFF;
  }
}

static void * cmd_alloc(queue_t * queue, id_t id, size_t len) {
  if (queue == NULL) {
    seterror(ESUB_CMD, id, ETYPE_NOQUEUE);
    return NULL;
  }
  if ((queue->maxlen - queue->len) < len) {
    seterror(ESUB_CMD, id, ETYPE_MEM);
    return NULL;
  }
  void * p = &queue->Q[queue->len];
  queue->len += len;
  return p;
}

static void * cmd_alloc(queue_t * queue, id_t id, uint8_t opcode, size_t len) {
  cmd_head_t * cmd = (cmd_head_t *)cmd_alloc(queue, id, sizeof(cmd_head_t) + len);
  if (cmd == NULL) return NULL;
  *cmd = { .id = id, .opcode = opcode };
  return &cmd[1];
}

bool cmd_nop(queue_t * queue, id_t id) {
  return cmd_alloc(queue, id, CMD_NOP, 0) != NULL;
}

bool cmd_stop(queue_t * queue, id_t id, bool hiz, bool soft) {
  cmd_stop_t * cmd = (cmd_stop_t *)cmd_alloc(queue, id, CMD_STOP, sizeof(cmd_stop_t));
  if (cmd != NULL) *cmd = { .hiz = hiz, .soft = soft };
  return cmd != NULL;
}

bool cmd_run(queue_t * queue, id_t id, ps_direction dir, float stepss) {
  cmd_run_t * cmd = (cmd_run_t *)cmd_alloc(queue, id, CMD_RUN, sizeof(cmd_run_t));
  if (cmd != NULL) *cmd = { .dir = dir, .stepss = stepss };
  return cmd != NULL;
}

bool cmd_stepclock(queue_t * queue, id_t id, ps_direction dir) {
  cmd_stepclk_t * cmd = (cmd_stepclk_t *)cmd_alloc(queue, id, CMD_STEPCLK, sizeof(cmd_stepclk_t));
  if (cmd != NULL) *cmd = { .dir = dir };
  return cmd != NULL;
}

bool cmd_move(queue_t * queue, id_t id, ps_direction dir, uint32_t microsteps) {
  cmd_move_t * cmd = (cmd_move_t *)cmd_alloc(queue, id, CMD_MOVE, sizeof(cmd_move_t));
  if (cmd != NULL) *cmd = { .dir = dir, .microsteps = microsteps };
  return cmd != NULL;
}

bool cmd_goto(queue_t * queue, id_t id, int32_t pos, bool hasdir, ps_direction dir) {
  cmd_goto_t * cmd = (cmd_goto_t *)cmd_alloc(queue, id, CMD_GOTO, sizeof(cmd_goto_t));
  if (cmd != NULL) *cmd = { .hasdir = hasdir, .dir = dir, .pos = pos };
  return cmd != NULL;
}

bool cmd_gountil(queue_t * queue, id_t id, ps_posact action, ps_direction dir, float stepss) {
  cmd_gountil_t * cmd = (cmd_gountil_t *)cmd_alloc(queue, id, CMD_GOUNTIL, sizeof(cmd_gountil_t));
  if (cmd != NULL) *cmd = { .action = action, .dir = dir, .stepss = stepss };
  return cmd != NULL;
}

bool cmd_releasesw(queue_t * queue, id_t id, ps_posact action, ps_direction dir) {
  cmd_releasesw_t * cmd = (cmd_releasesw_t *)cmd_alloc(queue, id, CMD_RELEASESW, sizeof(cmd_releasesw_t));
  if (cmd != NULL) *cmd = { .action = action, .dir = dir };
  return cmd != NULL;
}

bool cmd_gohome(queue_t * queue, id_t id) {
  return cmd_alloc(queue, id, CMD_GOHOME, 0) != NULL;
}

bool cmd_gomark(queue_t * queue, id_t id) {
  return cmd_alloc(queue, id, CMD_GOMARK, 0) != NULL;
}

bool cmd_resetpos(queue_t * queue, id_t id) {
  return cmd_alloc(queue, id, CMD_RESETPOS, 0) != NULL;
}

bool cmd_setpos(queue_t * queue, id_t id, int32_t pos) {
  cmd_setpos_t * cmd = (cmd_setpos_t *)cmd_alloc(queue, id, CMD_SETPOS, sizeof(cmd_setpos_t));
  if (cmd != NULL) *cmd = { .pos = pos };
  return cmd != NULL;
}

bool cmd_setmark(queue_t * queue, id_t id, int32_t mark) {
  cmd_setpos_t * cmd = (cmd_setpos_t *)cmd_alloc(queue, id, CMD_SETMARK, sizeof(cmd_setpos_t));
  if (cmd != NULL) *cmd = { .pos = mark };
  return cmd != NULL;
}

bool cmd_setconfig(queue_t * queue, id_t id, const char * data) {
  size_t ldata = strlen(data) + 1;
  void * buf = cmd_alloc(queue, id, CMD_SETCONFIG, ldata);
  if (buf != NULL) memcpy(buf, data, ldata);
  return buf != NULL;
}

bool cmd_waitbusy(queue_t * queue, id_t id) {
  return cmd_alloc(queue, id, CMD_WAITBUSY, 0) != NULL;
}

bool cmd_waitrunning(queue_t * queue, id_t id) {
  return cmd_alloc(queue, id, CMD_WAITRUNNING, 0) != NULL;
}

bool cmd_waitms(queue_t * queue, id_t id, uint32_t ms) {
  sketch_waitms_t * cmd = (sketch_waitms_t *)cmd_alloc(queue, id, CMD_WAITMS, sizeof(sketch_waitms_t));
  if (cmd != NULL) *cmd = { .ms = ms, .started = 0 };
  return cmd != NULL;
}

bool cmd_waitswitch(queue_t * queue, id_t id, bool state) {
  cmd_waitswitch_t * cmd = (cmd_waitswitch_t *)cmd_alloc(queue, id, CMD_WAITSWITCH, sizeof(cmd_waitswitch_t));
  if (cmd != NULL) *cmd = { .state = state };
  return cmd != NULL;
}



bool cmd_emptyqueue(queue_t * queue, id_t id) {
  if (queue == NULL) {
    seterror(ESUB_CMD, id, ETYPE_NOQUEUE);
    return false;
  }
  queue->len = 0;
  return true;
}

bool cmd_copyqueue(queue_t * queue, id_t id, queue_t * src) {
  if (src == NULL) {
    seterror(ESUB_CMD, id, ETYPE_NOQUEUE);
    return false;
  }
  void * buf = cmd_alloc(queue, id, src->len);
  if (buf == NULL) return false;
  memcpy(buf, src->Q, src->len);
  return true;
}

bool cmd_estop(id_t id, bool hiz, bool soft) {
  if (hiz) {
    if (soft)   ps_softhiz();
    else        ps_hardhiz();
  } else {
    if (soft)   ps_softstop();
    else        ps_hardstop();
  }
  return cmd_emptyqueue(Q0, id);
}

void cmd_clearerror() {
  state.motor.status = ps_getstatus(true);
}

void cmd_readqueue(JsonArray& arr, queue_t * queue) {
  for (auto value : arr) {
    JsonObject& entry = value.as<JsonObject>();
    String type = entry["type"].as<String>();
    id_t id = entry["id"].as<id_t>();
    if (type == "nop") {
      cmd_nop(queue, id);
    } else if (type == "stop") {
      cmd_stop(queue, id, entry["hiz"].as<bool>(), entry["soft"].as<bool>());
    } else if (type == "run") {
      cmd_run(queue, id, parse_direction(entry["dir"].as<String>(), FWD), entry["stepss"].as<float>());
    } else if (type == "stepclock") {
      cmd_stepclock(queue, id, parse_direction(entry["dir"].as<String>(), FWD));
    } else if (type == "move") {
      cmd_move(queue, id, parse_direction(entry["dir"].as<String>(), FWD), entry["microsteps"].as<uint32_t>());
    } else if (type == "goto") {
      cmd_goto(queue, id, entry["pos"].as<int32_t>(), entry["hasdir"].as<bool>(), parse_direction(entry["dir"].as<String>(), FWD));
    } else if (type == "gountil") {
      cmd_gountil(queue, id, parse_action(entry["action"].as<String>(), POS_RESET), parse_direction(entry["dir"].as<String>(), FWD), entry["stepss"].as<float>());
    } else if (type == "releasesw") {
      cmd_releasesw(queue, id, parse_action(entry["action"].as<String>(), POS_RESET), parse_direction(entry["dir"].as<String>(), FWD));
    } else if (type == "gohome") {
      cmd_gohome(queue, id);
    } else if (type == "gomark") {
      cmd_gomark(queue, id);
    } else if (type == "resetpos") {
      cmd_resetpos(queue, id);
    } else if (type == "setpos") {
      cmd_setpos(queue, id, entry["pos"].as<int32_t>());
    } else if (type == "setmark") {
      cmd_setmark(queue, id, entry["mark"].as<int32_t>());
    } else if (type == "setconfig") {
      cmd_setconfig(queue, id, entry["config"].as<char *>());
    } else if (type == "waitbusy") {
      cmd_waitbusy(queue, id);
    } else if (type == "waitrunning") {
      cmd_waitrunning(queue, id);
    } else if (type == "waitms") {
      cmd_waitms(queue, id, entry["ms"].as<uint32_t>());
    } else if (type == "waitswitch") {
      cmd_waitswitch(queue, id, entry["state"].as<bool>());
    }
  }
}

void cmd_writequeue(JsonArray& arr, queue_t * queue) {
  size_t index = 0;
  while (index < queue->len) {
    cmd_head_t * head = (cmd_head_t *)&(queue->Q[index]);
    void * Qcmd = (void *)&(queue->Q[index + sizeof(cmd_head_t)]);

    JsonObject& entry = arr.createNestedObject();
    entry["id"] = head->id;
    
    size_t consume = sizeof(cmd_head_t);
    switch (head->opcode) {
      case CMD_NOP: {
        entry["type"] = "nop";
        break;
      }
      case CMD_STOP: {
        cmd_stop_t * cmd = (cmd_stop_t *)Qcmd;
        entry["type"] = "stop";
        entry["hiz"] = cmd->hiz;
        entry["soft"] = cmd->soft;
        consume += sizeof(cmd_stop_t);
        break;
      }
      case CMD_RUN: {
        cmd_run_t * cmd = (cmd_run_t *)Qcmd;
        entry["type"] = "run";
        entry["dir"] = json_serialize(cmd->dir);
        entry["stepss"] = cmd->stepss;
        consume += sizeof(cmd_run_t);
        break;
      }
      case CMD_STEPCLK: {
        cmd_stepclk_t * cmd = (cmd_stepclk_t *)Qcmd;
        entry["type"] = "stepclock";
        entry["dir"] = json_serialize(cmd->dir);
        consume += sizeof(cmd_stepclk_t);
        break;
      }
      case CMD_MOVE: {
        cmd_move_t * cmd = (cmd_move_t *)Qcmd;
        entry["type"] = "move";
        entry["dir"] = json_serialize(cmd->dir);
        entry["microsteps"] = cmd->microsteps;
        consume += sizeof(cmd_move_t);
        break;
      }
      case CMD_GOTO: {
        cmd_goto_t * cmd = (cmd_goto_t *)Qcmd;
        entry["type"] = "goto";
        entry["pos"] = cmd->pos;
        entry["hasdir"] = cmd->hasdir;
        entry["dir"] = json_serialize(cmd->dir);
        consume += sizeof(cmd_goto_t);
        break;
      }
      case CMD_GOUNTIL: {
        cmd_gountil_t * cmd = (cmd_gountil_t *)Qcmd;
        entry["type"] = "gountil";
        entry["action"] = json_serialize(cmd->action);
        entry["dir"] = json_serialize(cmd->dir);
        entry["stepss"] = cmd->stepss;
        consume += sizeof(cmd_gountil_t);
        break;
      }
      case CMD_RELEASESW: {
        cmd_releasesw_t * cmd = (cmd_releasesw_t *)Qcmd;
        entry["type"] = "releasesw";
        entry["action"] = json_serialize(cmd->action);
        entry["dir"] = json_serialize(cmd->dir);
        consume += sizeof(cmd_releasesw_t);
        break;
      }
      case CMD_GOHOME: {
        entry["type"] = "gohome";
        break;
      }
      case CMD_GOMARK: {
        entry["type"] = "gomark";
        break;
      }
      case CMD_RESETPOS: {
        entry["type"] = "resetpos";
        break;
      }
      case CMD_SETPOS: {
        cmd_setpos_t * cmd = (cmd_setpos_t *)Qcmd;
        entry["type"] = "setpos";
        entry["pos"] = cmd->pos;
        consume += sizeof(cmd_setpos_t);
        break;
      }
      case CMD_SETMARK: {
        cmd_setpos_t * cmd = (cmd_setpos_t *)Qcmd;
        entry["type"] = "setmark";
        entry["mark"] = cmd->pos;
        consume += sizeof(cmd_setpos_t);
        break;
      }
      case CMD_SETCONFIG: {
        const char * data = (const char *)Qcmd;
        size_t ldata = strlen(data);
        entry["type"] = "setconfig";
        entry["config"] = data;
        consume += ldata + 1;
        break;
      }
      case CMD_WAITBUSY: {
        entry["type"] = "waitbusy";
        break;
      }
      case CMD_WAITRUNNING: {
        entry["type"] = "waitrunning";
        break;
      }
      case CMD_WAITMS: {
        sketch_waitms_t * sketch = (sketch_waitms_t *)Qcmd;
        entry["type"] = "waitms";
        entry["ms"] = sketch->ms;
        consume += sizeof(sketch_waitms_t);
        break;
      }
      case CMD_WAITSWITCH: {
        cmd_waitswitch_t * cmd = (cmd_waitswitch_t *)Qcmd;
        entry["type"] = "waitswitch";
        entry["state"] = cmd->state;
        consume += sizeof(cmd_waitswitch_t);
        break;
      }
    }
    index += consume;
  }
}

