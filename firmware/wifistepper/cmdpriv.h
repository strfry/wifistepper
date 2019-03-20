#ifndef __CMDPRIV_H
#define __CMDPRIV_H

typedef struct __attribute__((packed)) {
  id_t id;
  uint8_t opcode;
} cmd_head_t;

typedef struct __attribute__((packed)) {
  bool hiz;
  bool soft;
} cmd_stop_t;

typedef struct __attribute__((packed)) {
  ps_direction dir;
  float stepss;
} cmd_run_t;

typedef struct __attribute__((packed)) {
  ps_direction dir;
} cmd_stepclk_t;

typedef struct __attribute__((packed)) {
  ps_direction dir;
  uint32_t microsteps;
} cmd_move_t;

typedef struct __attribute__((packed)) {
  bool hasdir;
  ps_direction dir;
  int32_t pos;
} cmd_goto_t;

typedef struct __attribute__((packed)) {
  ps_posact action;
  ps_direction dir;
  float stepss;
} cmd_gountil_t;

typedef struct __attribute__((packed)) {
  ps_posact action;
  ps_direction dir;
} cmd_releasesw_t;

typedef struct __attribute__((packed)) {
  int32_t pos;
} cmd_setpos_t;

typedef struct __attribute__((packed)) {
  uint32_t millis;
} cmd_waitms_t;

#endif
