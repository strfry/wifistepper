#include <Arduino.h>

#include "powerstep01.h"
#include "wifistepper.h"

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
#define CMD_WAITMILLIS  (Q_PRE_NONE | 0x10)

typedef struct __attribute__((packed)) {
  uint8_t opcode;
  int32_t id;
} cmd_head_t;

typedef struct __attribute__((packed)) {
  cmd_head_t H;
  bool hiz;
  bool soft;
} cmd_stop_t;

typedef struct __attribute__((packed)) {
  cmd_head_t H;
  ps_direction dir;
  float stepss;
} cmd_run_t;

typedef struct __attribute__((packed)) {
  cmd_head_t H;
  ps_direction dir;
} cmd_stepclk_t;

typedef struct __attribute__((packed)) {
  cmd_head_t H;
  ps_direction dir;
  uint32_t microsteps;
} cmd_move_t;

typedef struct __attribute__((packed)) {
  cmd_head_t H;
  ps_direction dir;
  int32_t pos;
} cmd_goto_t;

typedef struct __attribute__((packed)) {
  cmd_head_t H;
  ps_posact action;
  ps_direction dir;
  float stepss;
} cmd_gountil_t;

typedef struct __attribute__((packed)) {
  cmd_head_t H;
  ps_posact action;
  ps_direction dir;
} cmd_releasesw_t;

typedef struct __attribute__((packed)) {
  cmd_head_t H;
  int32_t pos;
} cmd_setpos_t;

typedef struct __attribute__((packed)) {
  cmd_head_t H;
  bool wrap;
  uint32_t millis;
} cmd_waitmillis_t;


extern volatile bool flag_cmderror;
extern volatile command_state commandst;

uint8_t Q[Q_SIZE] = {0};
volatile size_t Qlen = 0;
volatile uint32_t Qid = 1;

void cmd_init() {
  
}

void cmd_loop() {
  commandst->this_command = 0;
  if (Qid > 0x7FFFFFFFFFFFFFFF) Qid = 0;
  ESP.wdtFeed();

  while (Qlen > 0) {
    cmd_head_t * head = (cmd_head_t *)Q;
    commandst->this_command = head->id;
    size_t consume = 0;

    // Check pre-conditions
    if ((head->opcode & Q_PRE_BUSY) && ps_isbusy()) return;
    if ((head->opcode & Q_PRE_STOP) && ps_isrunning()) return;
      
    switch (head->opcode) {
      case CMD_NOP:
      case CMD_WAITBUSY:
      case CMD_WAITRUNNING: {
        // Do nothing
        ps_nop();
        consume = sizeof(cmd_head_t);
        break;
      }
      case CMD_STOP: {
        cmd_stop_t * cmd = (cmd_stop_t *)Q;
        if (cmd->hiz) {
          if (cmd->soft)  ps_softhiz();
          else            ps_hardhiz();
        } else {
          if (cmd->soft)  ps_softstop();
          else            ps_hardstop();
        }
        consume = sizeof(cmd_stop_t);
        break;
      }
      case CMD_RUN: {
        cmd_run_t * cmd = (cmd_run_t *)Q;
        ps_run(motorcfg_dir(cmd->dir), cmd->stepss)
        consume = sizeof(cmd_run_t);
        break;
      }
      case CMD_STEPCLK: {
        cmd_stepclk_t * cmd = (cmd_stepclk_t *)Q;
        ps_stepclock(motorcfg_dir(cmd->dir));
        consume = sizeof(cmd_stepclk_t);
        break;
      }
      case CMD_MOVE: {
        cmd_move_t * cmd = (cmd_move_t *)Q;
        ps_move(motorcfg_dir(cmd->dir), cmd->microsteps);
        consume = sizeof(cmd_move_t);
        break;
      }
      case CMD_GOTO: {
        cmd_goto_t * cmd = (cmd_goto_t *)Q;
        if (cmd->dir != 0xFF) ps_goto(motorcfg_pos(cmd->pos));
        else ps_goto(motorcfg_pos(cmd->pos), motorcfg_dir(cmd->dir));
        consume = sizeof(cmd_goto_t);
        break;
      }
      case CMD_GOUNTIL: {
        cmd_gountil_t * cmd = (cmd_gountil_t *)Q;
        ps_gountil(cmd->action, motorcfg_dir(cmd->dir), cmd->stepss);
        consume = sizeof(cmd_gountil_t);
        break;
      }
      case CMD_RELEASESW: {
        cmd_releasesw_t * cmd = (cmd_releasesw_t *)Q;
        ps_releasesw(cmd->action, motorcfg_dir(cmd->dir));
        consume = sizeof(cmd_releasesw_t);
        break;
      }
      case CMD_GOHOME: {
        ps_gohome();
        consume = sizeof(cmd_head_t);
        break;
      }
      case CMD_GOMARK: {
        ps_gomark();
        consume = sizeof(cmd_head_t);
        break;
      }
      case CMD_RESETPOS: {
        ps_resetpos();
        consume = sizeof(cmd_head_t);
        break;
      }
      case CMD_SETPOS: {
        cmd_setpos_t * cmd = (cmd_setpos_t *)Q;
        ps_setpos(cmd->pos);
        consume = sizeof(cmd_setpos_t);
        break;
      }
      case CMD_SETMARK: {
        cmd_setpos_t * cmd = (cmd_setpos_t *)Q;
        ps_setmark(cmd->pos);
        consume = sizeof(cmd_setpos_t);
        break;
      }
      case CMD_SETCONFIG: {
        consume = sizeof(cmd_);
        break;
      }
      case CMD_WAITMILLIS: {
        consume = sizeof(cmd_waitmillis_t);
        break;
      }
    }

    memmove(Q, &Q[consume], Qlen - consume);
    Qlen -= consume;
    commandst.last_command = head->id;
    commandst.last_completed = millis();

    ESP.wdtFeed();
  }
}

#define check_Q(s)    if ((Q_SIZE - Qlen) < (s)) { flag_cmderror = true; return 0; } uint32_t id = Qid++;

/*void cmd_put(uint8_t * data, size_t len) {
  check_Q(len)
  memcpy(&Q[Qlen], data, len);
  Qlen += len;
}*/

static uint32_t cmd_head(uint8_t opcode) {
  check_Q(sizeof(cmd_head_t))
  *(cmd_head_t *)(&Q[Qlen]) = { .opcode = opcode, .id = id };
  Qlen += sizeof(cmd_head_t);
  return id;
}

uint32_t cmd_nop() {
  return cmd_head(CMD_OPCODE);
}

uint32_t cmd_stop(bool hiz, bool soft) {
  check_Q(sizeof(cmd_stophiz_t))
  *(cmd_stophiz_t *)(&Q[Qlen]) = { H = {.opcode = CMD_STOP, .id = id}, .hiz = hiz, .soft = soft };
  Qlen += sizeof(cmd_stophiz_t);
  return id;
}

uint32_t cmd_run(ps_direction dir, float stepss) {
  check_Q(sizeof(cmd_run_t))
  *(cmd_run_t *)(&Q[Qlen]) = { H = {.opcode = CMD_RUN, .id = id}, .dir = dir, .stepss = stepss };
  Qlen += sizeof(cmd_run_t);
  return id;
}

uint32_t cmd_stepclock(ps_direction dir) {
  check_Q(sizeof(cmd_stepclk_t))
  *(cmd_stepclk_t *)(&Q[Qlen]) = { H = {.opcode = CMD_STEPCLK, .id = id}, .dir = dir };
  Qlen += sizeof(cmd_stepclk_t);
  return id;
}

uint32_t cmd_move(ps_direction dir, uint32_t microsteps) {
  check_Q(sizeof(cmd_move_t))
  *(cmd_move_t *)(&Q[Qlen]) = { H = {.opcode = CMD_MOVE, .id = id}, .dir = dir, .microsteps = microsteps };
  Qlen += sizeof(cmd_move_t);
  return id;
}

uint32_t cmd_goto(int32_t pos, ps_direction dir = 0xFF) {
  check_Q(sizeof(cmd_goto_t))
  *(cmd_goto_t *)(&Q[Qlen]) = { H = {.opcode = CMD_GOTO, .id = id}, .pos = pos, .dir = dir };
  Qlen += sizeof(cmd_goto_t);
  return id;
}

uint32_t cmd_gountil(ps_posact action, ps_direction dir, float stepss) {
  check_Q(sizeof(cmd_gountil_t))
  *(cmd_gountil_t *)(&Q[Qlen]) = { H = {.opcode = CMD_GOUNTIL, .id = id}, .action = action, .dir = dir, .stepss = stepss };
  Qlen += sizeof(cmd_gountil_t);
  return id;
}

uint32_t cmd_releasesw(ps_posact action, ps_direction dir) {
  check_Q(sizeof(cmd_releasesw_t))
  *(cmd_releasesw_t *)(&Q[Qlen]) = { H = {.opcode = CMD_RELEASESW, .id = id}, .action = action, .dir = dir };
  Qlen += sizeof(cmd_releasesw_t);
  return id;
}

uint32_t cmd_gohome() {
  return cmd_head(CMD_GOHOME);
}

uint32_t cmd_gomark() {
  return cmd_head(CMD_GOMARK);
}

uint32_t cmd_resetpos() {
  return cmd_head(CMD_RESETPOS);
}

uint32_t cmd_setpos(int32_t pos) {
  check_Q(sizeof(cmd_setpos_t))
  *(cmd_setpos_t *)(&Q[Qlen]) = { H = {.opcode = CMD_SETPOS, .id = id}, .pos = pos };
  Qlen += sizeof(cmd_setpos_t);
  return id;
}

uint32_t cmd_setmark(int32_t mark) {
  check_Q(sizeof(cmd_setpos_t))
  *(cmd_setpos_t *)(&Q[Qlen]) = { H = {.opcode = CMD_SETMARK, .id = id}, .pos = mark };
  Qlen += sizeof(cmd_setpos_t);
  return id;
}

uint32_t cmd_setconfig(const char * config) {
  size_t lhead = sizeof(cmd_head_t);
  size_t lconfig = strlen(config) + 1;
  check_Q(lhead + lconfig)
  *(cmd_head_t *)(&Q[Qlen]) = { .opcode = CMD_SETCONFIG, .id = id };
  memcpy(&Q[Qlen+lhead], config, lconfig);
  Qlen += lhead + lconfig;
  return id;
}

uint32_t cmd_waitbusy() {
  return cmd_head(CMD_WAITBUSY);
}

uint32_t cmd_waitrunning() {
  return cmd_head(CMD_WAITRUNNING);
}

uint32_t cmd_waitmillis(uint32_t millis) {
  check_Q(sizeof(cmd_waitmillis_t))
  *(cmd_waitmillis_t *)(&Q[Qlen]) = { H = {.opcode = CMD_WAITMILLIS, .id = id}, .wrap = false, .millis = millis };
  Qlen += sizeof(cmd_waitmillis_t);
  return id;
}



uint32_t cmd_empty() {
  Qlen = 0;
  return cmd_nop();
}

uint32_t cmd_estop(bool hiz, bool soft) {
  if (hiz) {
    if (soft)   ps_softhiz();
    else        ps_hardhiz();
  } else {
    if (soft)   ps_softstop();
    else        ps_hardstop();
  }
  return cmd_empty();
}




