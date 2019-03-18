#include <Arduino.h>

#include "powerstep01.h"
#include "wifistepper.h"

#define Q_SIZE          (2048)
#define Q_TYPE_PSBUSY   (0x10)

#define CMD_NOP         (0x0)
#define CMD_STOP        (0x1)
#define CMD_HIZ         (0x2)
#define CMD_RUN         (Q_TYPE_PSBUSY | 0x1)
#define CMD_GOTO        (Q_TYPE_PSBUSY | 0x2)
#define CMD_STEPCLOCK   (Q_TYPE_PSBUSY | 0x3)


typedef struct __attribute__((packed)) {
  uint8_t opcode;
  uint8_t dir;
  float stepss;
  bool stopswitch;
} cmd_run_t;

typedef struct __attribute__((packed)) {
  uint8_t opcode;
  int32_t pos;
  uint8_t dir;
} cmd_goto_t;

typedef struct __attribute__((packed)) {
  uint8_t opcode;
  uint8_t dir;
} cmd_stepclock_t;

typedef struct __attribute__((packed)) {
  uint8_t opcode;
  bool soft;
} cmd_stophiz_t;


extern volatile bool flag_cmderror;

uint8_t Q[Q_SIZE] = {0};
volatile size_t Qlen = 0;

void cmd_init() {
  
}

bool cmd_loop() {
  ESP.wdtFeed();
  if (Qlen == 0) return false;
  
  uint8_t opcode = Q[0];
  size_t consume = 0;
  
  if (opcode & Q_TYPE_PSBUSY) {
    // Only continue if not busy
    if (ps_isbusy()) return false;
    
    switch (opcode) {
      case CMD_RUN: {
        cmd_run_t * cmd = (cmd_run_t *)Q;
        consume = sizeof(cmd_run_t);
        if (cmd->stopswitch)  ps_gountil(POS_RESET, motorcfg_dir((ps_direction)cmd->dir), cmd->stepss);
        else                  ps_run(motorcfg_dir((ps_direction)cmd->dir), cmd->stepss);
        break;
      }
      case CMD_GOTO: {
        cmd_goto_t * cmd = (cmd_goto_t *)Q;
        consume = sizeof(cmd_goto_t);
        if (cmd->dir == 0xFF) ps_goto(motorcfg_pos(cmd->pos));
        else                  ps_goto(motorcfg_pos(cmd->pos), motorcfg_dir((ps_direction)cmd->dir));
        break;
      }
      case CMD_STEPCLOCK: {
        cmd_stepclock_t * cmd = (cmd_stepclock_t *)Q;
        consume = sizeof(cmd_stepclock_t);
        ps_stepclock(motorcfg_dir((ps_direction)cmd->dir));
        break;
      }
    }
  } else {
    switch (opcode) {
      case CMD_NOP: {
        consume = sizeof(uint8_t);
        // Do nothing
        break;
      }
      case CMD_STOP:
      case CMD_HIZ: {
        cmd_stophiz_t * cmd = (cmd_stophiz_t *)Q;
        consume = sizeof(cmd_stophiz_t);
        if (opcode == CMD_STOP) {
          if (cmd->soft)  ps_softstop();
          else            ps_hardstop();
        } else if (opcode == CMD_HIZ) {
          if (cmd->soft)  ps_softhiz();
          else            ps_hardhiz();
        }
        break;
      }
    }
  }

  if (consume == 0) {
    // Haven't consumed anything, despite a command in queue
    flag_cmderror = true;
    return false;
  }

  memmove(Q, &Q[consume], Qlen - consume);
  Qlen -= consume;
  return true;
}

#define check_Q(s)   if ((Q_SIZE - Qlen) < (s)) { flag_cmderror = true; return; }

void cmd_put(uint8_t * data, size_t len) {
  check_Q(len)
  memcpy(&Q[Qlen], data, len);
  Qlen += len;
}

void cmd_run(ps_direction dir, float stepss, bool stopswitch) {
  check_Q(sizeof(cmd_run_t))
  *(cmd_run_t *)(&Q[Qlen]) = { .opcode = CMD_RUN, .dir = dir, .stepss = stepss, .stopswitch = stopswitch };
  Qlen += sizeof(cmd_run_t);
}

void cmd_goto(int32_t pos) {
  check_Q(sizeof(cmd_goto_t))
  *(cmd_goto_t *)(&Q[Qlen]) = { .opcode = CMD_GOTO, .pos = pos, .dir = 0xFF };
  Qlen += sizeof(cmd_goto_t);
}

void cmd_goto(int32_t pos, ps_direction dir) {
  check_Q(sizeof(cmd_goto_t))
  *(cmd_goto_t *)(&Q[Qlen]) = { .opcode = CMD_GOTO, .pos = pos, .dir = dir };
  Qlen += sizeof(cmd_goto_t);
}

void cmd_stepclock(ps_direction dir) {
  check_Q(sizeof(cmd_stepclock_t))
  *(cmd_stepclock_t *)(&Q[Qlen]) = { .opcode = CMD_STEPCLOCK, .dir = dir };
  Qlen += sizeof(cmd_stepclock_t);
}

void cmd_stop(bool soft) {
  check_Q(sizeof(cmd_stophiz_t))
  *(cmd_stophiz_t *)(&Q[Qlen]) = { .opcode = CMD_STOP, .soft = soft };
  Qlen += sizeof(cmd_stophiz_t);
}

void cmd_hiz(bool soft) {
  check_Q(sizeof(cmd_stophiz_t))
  *(cmd_stophiz_t *)(&Q[Qlen]) = { .opcode = CMD_HIZ, .soft = soft };
  Qlen += sizeof(cmd_stophiz_t);
}

