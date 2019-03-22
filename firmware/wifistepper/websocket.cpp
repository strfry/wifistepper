#include <WebSocketsServer.h>

#include "wifistepper.h"

extern WebSocketsServer websocket;


typedef union {
  float f32;
  int i32;
  uint8_t b[4];
} wsc4_t;

float ws_buf2float(uint8_t * b) {
  wsc4_t c; c.b[0] = b[0]; c.b[1] = b[1]; c.b[2] = b[2]; c.b[3] = b[3];
  return c.f32;
}

void ws_packfloat(float f, uint8_t * b) {
  wsc4_t c; c.f32 = f;
  b[0] = c.b[0]; b[1] = c.b[1]; b[2] = c.b[2]; b[3] = c.b[3];
}

int ws_buf2int(uint8_t * b) {
  wsc4_t c; c.b[0] = b[0]; c.b[1] = b[1]; c.b[2] = b[2]; c.b[3] = b[3];
  return c.i32;
}

void ws_packint(int i, uint8_t * b) {
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
        case WS_READSTATUS: {
          ps_status status = ps_getstatus(data[1]);
          uint8_t movement = '?';
          switch (status.movement) {
            case M_STOPPED:     movement = 'x';   break;
            case M_ACCEL:       movement = 'a';   break;
            case M_DECEL:       movement = 'd';   break;
            case M_CONSTSPEED:  movement = 's';   break;
          }
          
          uint8_t buf[14] = {0};
          buf[0] = WS_READSTATUS;
          buf[1] = motorcfg_dir(status.direction) == FWD? 'f' : 'r';
          buf[2] = movement;
          buf[3] = status.hiz? 0x1 : 0x0;
          buf[4] = status.busy? 0x1 : 0x0;
          buf[5] = status.user_switch? 0x1 : 0x0;
          buf[6] = status.step_clock? 0x1 : 0x0;
          buf[7] = status.alarms.command_error? 0x1 : 0x0;
          buf[8] = status.alarms.overcurrent? 0x1 : 0x0;
          buf[9] = status.alarms.undervoltage? 0x1 : 0x0;
          buf[10] = status.alarms.thermal_shutdown? 0x1 : 0x0;
          buf[11] = status.alarms.thermal_warning? 0x1 : 0x0;
          buf[12] = status.alarms.stall_detect? 0x1 : 0x0;
          buf[13] = status.alarms.user_switch? 0x1 : 0x0;
          websocket.sendBIN(num, buf, sizeof(buf));
          break;
        }

        case WS_READSTATE: {
          uint8_t buf[1+4+4+4+1] = {0};
          buf[0] = WS_READSTATE;
          ws_packint(motorcfg_pos(state.motor.pos), &buf[1]);
          ws_packint(motorcfg_pos(state.motor.mark), &buf[1+4]);
          ws_packfloat(state.motor.stepss, &buf[1+4+4]);
          buf[1+4+4+4] = state.motor.busy? 0x1 : 0x0;
          websocket.sendBIN(num, buf, sizeof(buf));
          break;
        }

        case WS_CMDSTOP: {
          if (len != 2) {
            Serial.print("Bad CMDSTOP length");
            return;
          }
          cmd_stop(nextid(), false, data[1]);
          break;
        }

        case WS_CMDHIZ: {
          if (len != 2) {
            Serial.print("Bad CMDHIZ length");
            return;
          }
          cmd_stop(nextid(), true, data[1]);
          break;
        }

        case WS_CMDGOTO: {
          if (len != 5) {
            Serial.print("Bad CMDGOTO length");
            return;
          }
          int32_t pos = ws_buf2int(&data[1]);
          cmd_goto(nextid(), pos);
          break;
        }

        case WS_CMDRUN: {
          if (len != 7) {
            Serial.print("Bad CMDRUN length");
            return;
          }
          ps_direction dir = data[1] == 'r'? REV : FWD;
          float stepss = ws_buf2float(&data[2]);
          bool stopswitch = data[6] == 0x1;   // TODO - remove stopswitch param
          cmd_run(nextid(), dir, stepss);
          /*if (stopswitch) {
            ps_gountil(POS_RESET, motorcfg_dir(dir), stepss);
          } else {
            ps_run(motorcfg_dir(dir), stepss);
          }*/
          break;
        }

        case WS_STEPCLOCK: {
          if (len != 2) {
            Serial.print("Bad STEPCLOCK length");
            return;
          }
          ps_direction dir = data[1] == 'r'? REV : FWD;
          cmd_stepclock(nextid(), dir);
          break;
        }

        case WS_POS: {
          if (len != 5) {
            Serial.print("Bad POS length");
            return;
          }
          int pos = ws_buf2int(&data[1]);
          if (pos == 0)   ps_resetpos();
          else            ps_setpos(pos);
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

