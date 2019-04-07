#include <WebSocketsServer.h>

#include "wifistepper.h"

extern WebSocketsServer websocket;

typedef enum {
  WS_READSTATE  = 0x11,
  WS_ESTOP      = 0x12,
  WS_STOP       = 0x21,
  WS_GOTO       = 0x22,
  WS_RUN        = 0x23,
  WS_STEPCLOCK  = 0x24,
  WS_POS        = 0x31
} ws_opcode;


typedef union {
  float f32;
  int i32;
  uint8_t b[4];
} wsc4_t;

static inline float ws_buf2float(uint8_t * b) {
  wsc4_t c; c.b[0] = b[0]; c.b[1] = b[1]; c.b[2] = b[2]; c.b[3] = b[3];
  return c.f32;
}

static inline void ws_packfloat(float f, uint8_t * b) {
  wsc4_t c; c.f32 = f;
  b[0] = c.b[0]; b[1] = c.b[1]; b[2] = c.b[2]; b[3] = c.b[3];
}

static inline int ws_buf2int(uint8_t * b) {
  wsc4_t c; c.b[0] = b[0]; c.b[1] = b[1]; c.b[2] = b[2]; c.b[3] = b[3];
  return c.i32;
}

static inline void ws_packint(int i, uint8_t * b) {
  wsc4_t c; c.i32 = i;
  b[0] = c.b[0]; b[1] = c.b[1]; b[2] = c.b[2]; b[3] = c.b[3];
}

void ws_event(uint8_t num, WStype_t type, uint8_t * data, size_t len) {
  if (len < 1) return;
  
  switch(type) {
    case WStype_TEXT: {
      // TODO - create text interface
      /*
      Serial.print("WS Text ");
      Serial.println(len);
      for (int i=0; i<len; i++) {
        Serial.printf("R[%d] = 0x", i);
        Serial.println(data[i], HEX);
      }
      websocket.sendBIN(num, data, len);
      */
      break;
    }

    case WStype_BIN: {
      switch (data[0]) {
        case WS_READSTATE: {
          motor_state * st = &state.motor;

          if (len != 2) {
            seterror(ESUB_HTTP);
            return;
          }

          uint8_t target = data[1];
          if ((target != 0 && (!config.daisy.enabled || !config.daisy.master || !state.daisy.active)) || target > state.daisy.slaves) {
            seterror(ESUB_HTTP);
            return;
          }
          if (target > 0) {
            st = &sketch.daisy.slave[target-1].state.motor;
          }
          
          uint8_t movement = '?';
          switch (st->status.movement) {
            case M_STOPPED:     movement = 'x';   break;
            case M_ACCEL:       movement = 'a';   break;
            case M_DECEL:       movement = 'd';   break;
            case M_CONSTSPEED:  movement = 's';   break;
          }
          
          uint8_t buf[27] = {0};
          buf[0] = WS_READSTATE;
          ws_packfloat(st->stepss, &buf[1]);
          ws_packint(st->pos, &buf[5]);
          ws_packint(st->mark, &buf[9]);
          ws_packfloat(st->adc, &buf[13]);
          buf[14] = st->status.direction == FWD? 'f' : 'r';
          buf[15] = movement;
          buf[16] = st->status.hiz? 0x1 : 0x0;
          buf[17] = st->status.busy? 0x1 : 0x0;
          buf[18] = st->status.user_switch? 0x1 : 0x0;
          buf[19] = st->status.step_clock? 0x1 : 0x0;
          buf[20] = st->status.alarms.command_error? 0x1 : 0x0;
          buf[21] = st->status.alarms.overcurrent? 0x1 : 0x0;
          buf[22] = st->status.alarms.undervoltage? 0x1 : 0x0;
          buf[23] = st->status.alarms.thermal_shutdown? 0x1 : 0x0;
          buf[24] = st->status.alarms.thermal_warning? 0x1 : 0x0;
          buf[25] = st->status.alarms.stall_detect? 0x1 : 0x0;
          buf[26] = st->status.alarms.user_switch? 0x1 : 0x0;
          websocket.sendBIN(num, buf, sizeof(buf));
          break;
        }

        case WS_ESTOP: {
          if (len != 4) {
            seterror(ESUB_HTTP);
            return;
          }
          uint8_t target = data[1];
          bool hiz = data[2];
          bool soft = data[3];
          m_estop(target, nextid(), hiz, soft);
          break;
        }

        case WS_STOP: {
          if (len != 4) {
            seterror(ESUB_HTTP);
            return;
          }
          uint8_t target = data[1];
          bool hiz = data[2];
          bool soft = data[3];
          m_stop(target, nextid(), hiz, soft);
          break;
        }

        case WS_GOTO: {
          if (len != 6) {
            seterror(ESUB_HTTP);
            return;
          }
          uint8_t target = data[1];
          int32_t pos = ws_buf2int(&data[2]);
          m_goto(target, nextid(), pos);
          break;
        }

        case WS_RUN: {
          if (len != 7) {
            seterror(ESUB_HTTP);
            return;
          }
          uint8_t target = data[1];
          ps_direction dir = data[2] == 'r'? REV : FWD;
          float stepss = ws_buf2float(&data[3]);
          m_run(target, nextid(), dir, stepss);
          break;
        }

        case WS_STEPCLOCK: {
          if (len != 3) {
            seterror(ESUB_HTTP);
            return;
          }
          uint8_t target = data[1];
          ps_direction dir = data[2] == 'r'? REV : FWD;
          m_stepclock(target, nextid(), dir);
          break;
        }

        case WS_POS: {
          if (len != 6) {
            seterror(ESUB_HTTP);
            return;
          }
          uint8_t target = data[1];
          int pos = ws_buf2int(&data[2]);
          if (pos == 0)   m_resetpos(target, nextid());
          else            m_setpos(target, nextid(), pos);
          break;
        }
      }
      break;
    }
  }
}

void websocket_init() {
  websocket.begin();
  websocket.onEvent(ws_event);
}

