#include <Arduino.h>

#include "wifistepper.h"
#include "cmdpriv.h"

#define B_SIZE          (2048)
#define B_MAGIC         (0xAB)

/*#define BO_MAGIC1         (0)
#define BO_MAGIC2         (1)
#define BO_CHECKSUM_HEAD  (2)
#define BO_CHECKSUM_BODY  (3)
#define BO_ADDRESS        (4)
#define BO_PACKET_ID      (5)
#define BO_OPCODE         (9)
#define BO_LENGTH         (10)*/


#define CP_DAISY        (0x00)
//#define CP_CONFIG       (0x20)
//#define CP_STATE        (0x40)
#define CP_MOTOR        (0x40)

#define CMD_PING        (CP_DAISY | 0x00)
#define CMD_ACK         (CP_DAISY | 0x01)
#define CMD_SYNC        (CP_DAISY | 0x02)
#define CMD_CONFIG      (CP_DAISY | 0x03)
#define CMD_STATE       (CP_DAISY | 0x04)

#define CMD_STOP        (CP_MOTOR | 0x01)
#define CMD_RUN         (CP_MOTOR | 0x02)
#define CMD_STEPCLK     (CP_MOTOR | 0x03)
#define CMD_MOVE        (CP_MOTOR | 0x04)
#define CMD_GOTO        (CP_MOTOR | 0x05)
#define CMD_GOUNTIL     (CP_MOTOR | 0x06)
#define CMD_RELEASESW   (CP_MOTOR | 0x07)
#define CMD_GOHOME      (CP_MOTOR | 0x08)
#define CMD_GOMARK      (CP_MOTOR | 0x09)
#define CMD_RESETPOS    (CP_MOTOR | 0x0A)
#define CMD_SETPOS      (CP_MOTOR | 0x0B)
#define CMD_SETMARK     (CP_MOTOR | 0x0C)
#define CMD_SETCONFIG   (CP_MOTOR | 0x0D)
#define CMD_WAITBUSY    (CP_MOTOR | 0x0E)
#define CMD_WAITRUNNING (CP_MOTOR | 0x0F)
#define CMD_WAITMS      (CP_MOTOR | 0x10)
#define CMD_EMPTY       (CP_MOTOR | 0x11)
#define CMD_ESTOP       (CP_MOTOR | 0x12)

#define SELF            (0x00)

#define CTO_PING        (1000)
#define CTO_CONFIG      (2000)
#define CTO_STATE       (250)

typedef struct __attribute__((packed)) {
  uint8_t magic;
  uint8_t head_checksum;
  uint8_t body_checksum;
  uint8_t address;
  id_t id;
  uint8_t opcode;
  uint16_t length;
} daisy_head_t;

/*typedef union {
  uint8_t b[4];
  float f32;
  int32_t i32;
  uint32_t u32;
} bc4_t;

static inline float b_buf2float(uint8_t * b) {bc4_t c; c.b[0] = b[0]; c.b[1] = b[1]; c.b[2] = b[2]; c.b[3] = b[3]; return c.f32; }
static inline void b_packfloat(float f, uint8_t * b) { bc4_t c; c.f32 = f; b[0] = c.b[0]; b[1] = c.b[1]; b[2] = c.b[2]; b[3] = c.b[3]; }
static inline int32_t b_buf2int32(uint8_t * b) { bc4_t c; c.b[0] = b[0]; c.b[1] = b[1]; c.b[2] = b[2]; c.b[3] = b[3]; return c.i32; }
static inline void b_packint32(int32_t i, uint8_t * b) { bc4_t c; c.i32 = i; b[0] = c.b[0]; b[1] = c.b[1]; b[2] = c.b[2]; b[3] = c.b[3]; }
*/

// In Buffer
uint8_t B[B_SIZE] = {0};
volatile size_t Blen = 0;
volatile size_t Bskip = 0;

// Outbox
uint8_t O[B_SIZE] = {0};
volatile size_t Olen = 0;
#define daisy_dumpoutbox()  ({ if (Olen > 0) { Serial.print("Write outbox: "); Serial.println(Olen); Serial.write(O, Olen); Olen = 0; } })

// Ack list
#define A_SIZE        (64)
id_t * A = {0};
volatile size_t Alen = 0;

// Slave vars
extern daisy_slavestate * daisy_slavestates;

static uint8_t daisy_checksum8(uint8_t * data, size_t length) {
  unsigned int sum = 0;
  for (size_t i = 0; i < length; i++) {
    sum += data[i];
  }
  return (uint8_t)sum;
}

static uint8_t daisy_checksum8(daisy_head_t * head) {
  uint8_t * uhead = (uint8_t *)head;
  return daisy_checksum8(&uhead[offsetof(daisy_head_t, body_checksum)], sizeof(daisy_head_t) - offsetof(daisy_head_t, body_checksum));
}

void daisy_init() {
  Serial.begin(config.service.daisy.baudrate);
  if (config.service.daisy.master) {
    A = new id_t[A_SIZE];
  }
}

static void * daisy_Oalloc(uint8_t address, id_t id, uint8_t opcode, size_t len) {
  // Check if we have enough memory in buffers
  // TODO - uncomment here
  if (/*!state.service.daisy.active ||*/ len > 0xFFFF || (B_SIZE - Olen) < (sizeof(daisy_head_t) + len) || Alen >= A_SIZE) {
    if (id != 0) seterror(ESUB_DAISY, id);
    return NULL;
  }
  
  // Add id to ack list
  if (config.service.daisy.master && id != 0) {
    A[Alen++] = id;
  }

  // Allocate and initialize packet
  daisy_head_t * packet = (daisy_head_t *)(&O[Olen]);
  packet->magic = B_MAGIC;
  packet->address = address;
  packet->id = id;
  packet->opcode = opcode;
  packet->length = (uint16_t)len;

  // Extend outbox buffer size
  Olen += sizeof(daisy_head_t) + len;

  // Return the packet payload
  return &packet[1];
}

static void daisy_Opack(void * data) {
  if (data == NULL) return;

  uint8_t * udata = (uint8_t *)data;

  // Find start of packet
  daisy_head_t * packet = (daisy_head_t *)(udata - sizeof(daisy_head_t));

  // Compute checksums
  size_t len = packet->length;
  packet->body_checksum = daisy_checksum8(udata, len);
  packet->head_checksum = daisy_checksum8(packet);
}

static void daisy_masterconsume(int8_t address, id_t id, uint8_t opcode, void * data, uint16_t len) {
  if (id != 0) {
    if (A[0] != id) {
      // Handle Ack error
      seterror(ESUB_DAISY, A[0]);

      // Shift up the Ack list
      size_t i = 0;
      for (; i < Alen; i++) {
        if (A[i] == id) break;
      }
      memmove(A, &A[i], sizeof(id_t) * (Alen - i));
      Alen -= i;
      
    } else {
      // Ack good, pop ack from list
      memmove(A, &A[1], sizeof(id_t) * (Alen - 1));
      Alen -= 1;
    }
  }
  
  switch (opcode) {
    case CMD_PING: {
      if (address >= 0) {
        // Bad address value, should be negative
        return;
      }
      uint8_t numslaves = abs(address);
      if (state.service.daisy.numslaves != numslaves) {
        // New number of slaves present, reallocate states and sync
        daisy_slavestates = (daisy_slavestates == NULL)? (daisy_slavestate *)malloc(sizeof(daisy_slavestate) * numslaves) : (daisy_slavestate *)realloc(daisy_slavestates, sizeof(daisy_slavestate) * numslaves);
        memset(daisy_slavestates, 0, sizeof(daisy_slavestate) * numslaves);
        for (uint8_t i = 0; i < numslaves; i++) {
          // Send sync to slave
          daisy_Opack(daisy_Oalloc(i, nextid(), CMD_SYNC, 0));
        }

        // update number of slaves
        state.service.daisy.numslaves = numslaves;
      }
      break;
    }
  }
}

void daisy_check(unsigned long now) {
  unsigned long ping_delta = timesince(state.service.daisy.last.ping, now);

  if (ping_delta > CTO_PING) {
    state.service.daisy.active = false;
  }
  
  if (config.service.daisy.master && ping_delta > (CTO_PING / 4)) {
    daisy_Opack(daisy_Oalloc(SELF, currentid(), CMD_PING, 0));
  }

  if (/*state.service.daisy.active && */!config.service.daisy.master && timesince(state.service.daisy.last.config, now) > CTO_CONFIG) {
    state.service.daisy.last.config = now;
    daisy_slaveconfig * slaveconfig = (daisy_slaveconfig *)daisy_Oalloc(SELF, 0, CMD_CONFIG, sizeof(daisy_slaveconfig));
    if (slaveconfig != NULL) {
      memcpy(&slaveconfig->io, &config.io, sizeof(io_config));
      memcpy(&slaveconfig->motor, &state.motor, sizeof(motor_config));
    }
    daisy_Opack(slaveconfig);
  }

  if (/*state.service.daisy.active && */!config.service.daisy.master && timesince(state.service.daisy.last.state, now) > CTO_STATE) {
    state.service.daisy.last.state = now;
    daisy_slavestate * slavestate = (daisy_slavestate *)daisy_Oalloc(SELF, 0, CMD_STATE, sizeof(daisy_slavestate));
    if (slavestate != NULL) {
      memcpy(&slavestate->error, &state.error, sizeof(error_state));
      memcpy(&slavestate->command, &state.command, sizeof(command_state));
      memcpy(&slavestate->motor, &state.motor, sizeof(motor_state));
    }
    daisy_Opack(slavestate);
  }
}

static void daisy_ack(id_t id) {
  daisy_Opack(daisy_Oalloc(SELF, id, CMD_ACK, 0));
}

#define daisy_expectlen(elen)  ({ if (len != (elen)) { seterror(ESUB_DAISY, id); return; } })

static void daisy_slaveconsume(id_t id, uint8_t opcode, void * data, uint16_t len) {
  switch (opcode) {
    case CMD_STOP: {
      daisy_expectlen(sizeof(cmd_stop_t));
      cmd_stop_t * cmd = (cmd_stop_t *)data;
      cmd_stop(id, cmd->hiz, cmd->soft);
      daisy_ack(id);
      break;
    }
    case CMD_RUN: {
      daisy_expectlen(sizeof(cmd_run_t));
      cmd_run_t * cmd = (cmd_run_t *)data;
      cmd_run(id, cmd->dir, cmd->stepss);
      daisy_ack(id);
      break;
    }
    case CMD_STEPCLK: {
      daisy_expectlen(sizeof(cmd_stepclk_t));
      cmd_stepclk_t * cmd = (cmd_stepclk_t *)data;
      cmd_stepclock(id, cmd->dir);
      daisy_ack(id);
      break;
    }
    case CMD_MOVE: {
      daisy_expectlen(sizeof(cmd_move_t));
      cmd_move_t * cmd = (cmd_move_t *)data;
      cmd_move(id, cmd->dir, cmd->microsteps);
      daisy_ack(id);
      break;
    }
    case CMD_GOTO: {
      daisy_expectlen(sizeof(cmd_goto_t));
      cmd_goto_t * cmd = (cmd_goto_t *)data;
      if (cmd->hasdir)  cmd_goto(id, cmd->pos, cmd->dir);
      else              cmd_goto(id, cmd->pos);
      daisy_ack(id);
      break;
    }
    case CMD_GOUNTIL: {
      daisy_expectlen(sizeof(cmd_gountil_t));
      cmd_gountil_t * cmd = (cmd_gountil_t *)data;
      cmd_gountil(id, cmd->action, cmd->dir, cmd->stepss);
      daisy_ack(id);
      break;
    }
    case CMD_RELEASESW: {
      daisy_expectlen(sizeof(cmd_releasesw_t));
      cmd_releasesw_t * cmd = (cmd_releasesw_t *)data;
      cmd_releasesw(id, cmd->action, cmd->dir);
      daisy_ack(id);
      break;
    }
    case CMD_GOHOME: {
      daisy_expectlen(0);
      cmd_gohome(id);
      daisy_ack(id);
      break;
    }
    case CMD_GOMARK: {
      daisy_expectlen(0);
      cmd_gomark(id);
      daisy_ack(id);
      break;
    }
    case CMD_RESETPOS: {
      daisy_expectlen(0);
      cmd_resetpos(id);
      daisy_ack(id);
      break;
    }
    case CMD_SETPOS: {
      daisy_expectlen(sizeof(cmd_setpos_t));
      cmd_setpos_t * cmd = (cmd_setpos_t *)data;
      cmd_setpos(id, cmd->pos);
      daisy_ack(id);
      break;
    }
    case CMD_SETMARK: {
      daisy_expectlen(sizeof(cmd_setpos_t));
      cmd_setpos_t * cmd = (cmd_setpos_t *)data;
      cmd_setmark(id, cmd->pos);
      daisy_ack(id);
      break;
    }
    case CMD_SETCONFIG: {
      if (((const char *)data)[len-1] != 0) {
        seterror(ESUB_DAISY, id);
        return;
      }
      cmd_setconfig(id, (const char *)data);
      daisy_ack(id);
      break;
    }
    case CMD_WAITBUSY: {
      daisy_expectlen(0);
      cmd_waitbusy(id);
      daisy_ack(id);
      break;
    }
    case CMD_WAITRUNNING: {
      daisy_expectlen(0);
      cmd_waitrunning(id);
      daisy_ack(id);
      break;
    }
    case CMD_WAITMS: {
      daisy_expectlen(sizeof(cmd_waitms_t));
      cmd_waitms_t * cmd = (cmd_waitms_t *)data;
      cmd_waitms(id, cmd->millis);
      daisy_ack(id);
      break;
    }
    case CMD_EMPTY: {
      daisy_expectlen(0);
      cmd_empty(id);
      daisy_ack(id);
      break;
    }
    case CMD_ESTOP: {
      daisy_expectlen(sizeof(cmd_stop_t));
      cmd_stop_t * cmd = (cmd_stop_t *)data;
      cmd_estop(id, cmd->hiz, cmd->soft);
      daisy_ack(id);
      break;
    }
  }
}

void daisy_loop(unsigned long now) {
  Blen += Serial.read((char *)&B[Blen], B_SIZE - Blen);

  // If no packets to parse, dump outbox
  if (Blen == 0) {
    daisy_dumpoutbox();
  }

  // Parse packets
  while (Blen > 0) {
    // Skip bytes if needed
    if (Bskip > 0) {
      size_t skipped = min(Blen, Bskip);
      Serial.write(B, skipped);
      memmove(B, &B[skipped], Blen - skipped);
      Bskip -= skipped;
      Blen -= skipped;
      continue;
    }
    
    // Sync to start of packet (SOP)
    size_t i = 0;
    for (; i < Blen; i++) {
      if (B[i] == B_MAGIC) break;
    }

    // Forward all unparsed data if not master
    if (i > 0 && !config.service.daisy.master) Serial.write(B, i);
  
    // Either at end or SOP
    if (i == Blen) {
      // At end, empty buf and return
      Blen = 0;
      break;
    }
  
    // Move packet to beginning of buffer
    if (i != 0) {
      memmove(B, &B[i], Blen - i);
      Blen -= i;
    }

    // Ensure length of header
    if (Blen < sizeof(daisy_head_t)) return;

    daisy_head_t * head = (daisy_head_t *)B;

    // Check rest of header
    bool isvalid = daisy_checksum8(head) == head->head_checksum;
    
    // If this is a ping, set active flag
    if (isvalid && head->opcode == CMD_PING) {
      state.service.daisy.active = true;
      state.service.daisy.last.ping = now;
    }

    // Check if it's for us
    if (isvalid && !config.service.daisy.master && head->address != 0x01) {
      // We're not master and the packet is not for us, skip it
      head->address -= 1;
      Bskip = sizeof(daisy_head_t) + head->length;
      continue;
    }
    if (!isvalid) {
      // Not a valid header, shift out one byte and continue
      if (!config.service.daisy.master) Serial.write(B, 1);
      memmove(B, &B[1], Blen - 1);
      Blen -= 1;
      continue;
    }

    // Make sure we have whole packet
    if (Blen < (sizeof(daisy_head_t) + head->length)) return;

    // Validate checksum
    if (daisy_checksum8((uint8_t *)&head[1], head->length) != head->body_checksum) {
      // Bad checksum, shift out one byte and continue
      if (!config.service.daisy.master) Serial.write(B, 1);
      memmove(B, &B[1], Blen - 1);
      Blen -= 1;
      continue;
    }

    // Opportunistically dump outbox here
    daisy_dumpoutbox();

    // Consume packet
    {
      if (config.service.daisy.master)  daisy_masterconsume(head->address, head->id, head->opcode, &head[1], head->length);
      else                              daisy_slaveconsume(head->id, head->opcode, &head[1], head->length);
    }

    // Opportunistically dump outbox here
    daisy_dumpoutbox();

    // Clear packet from buffer
    size_t fullsize = sizeof(daisy_head_t) + head->length;
    memmove(B, &B[fullsize], Blen - fullsize);
    Blen -= fullsize;
  }
}


bool daisy_stop(uint8_t address, id_t id, bool hiz, bool soft) {
  cmd_stop_t * cmd = (cmd_stop_t *)daisy_Oalloc(address, id, CMD_STOP, sizeof(cmd_stop_t));
  if (cmd != NULL) *cmd = { .hiz = hiz, .soft = soft };
  daisy_Opack(cmd);
  return cmd != NULL;
}

bool daisy_run(uint8_t address, id_t id, ps_direction dir, float stepss) {
  cmd_stepclk_t * cmd = (cmd_stepclk_t *)daisy_Oalloc(address, id, CMD_STEPCLK, sizeof(cmd_stepclk_t));
  if (cmd != NULL) *cmd = { .dir = dir };
  daisy_Opack(cmd);
  return cmd != NULL;
}

bool daisy_stepclock(uint8_t address, id_t id, ps_direction dir) {
  cmd_stepclk_t * cmd = (cmd_stepclk_t *)daisy_Oalloc(address, id, CMD_STEPCLK, sizeof(cmd_stepclk_t));
  if (cmd != NULL) *cmd = { .dir = dir };
  daisy_Opack(cmd);
  return cmd != NULL;
}

bool daisy_move(uint8_t address, id_t id, ps_direction dir, uint32_t microsteps) {
  cmd_move_t * cmd = (cmd_move_t *)daisy_Oalloc(address, id, CMD_MOVE, sizeof(cmd_move_t));
  if (cmd != NULL) *cmd = { .dir = dir, .microsteps = microsteps };
  daisy_Opack(cmd);
  return cmd != NULL;
}

bool daisy_goto(uint8_t address, id_t id, int32_t pos) {
  cmd_goto_t * cmd = (cmd_goto_t *)daisy_Oalloc(address, id, CMD_GOTO, sizeof(cmd_goto_t));
  if (cmd != NULL) *cmd = { .hasdir = false, .dir = FWD, .pos = pos };
  daisy_Opack(cmd);
  return cmd != NULL;
}

bool daisy_goto(uint8_t address, id_t id, int32_t pos, ps_direction dir) {
  cmd_goto_t * cmd = (cmd_goto_t *)daisy_Oalloc(address, id, CMD_GOTO, sizeof(cmd_goto_t));
  if (cmd != NULL) *cmd = { .hasdir = true, .dir = dir, .pos = pos };
  daisy_Opack(cmd);
  return cmd != NULL;
}

bool daisy_gountil(uint8_t address, id_t id, ps_posact action, ps_direction dir, float stepss) {
  cmd_gountil_t * cmd = (cmd_gountil_t *)daisy_Oalloc(address, id, CMD_GOUNTIL, sizeof(cmd_gountil_t));
  if (cmd != NULL) *cmd = { .action = action, .dir = dir, .stepss = stepss };
  daisy_Opack(cmd);
  return cmd != NULL;
}

bool daisy_releasesw(uint8_t address, id_t id, ps_posact action, ps_direction dir) {
  cmd_releasesw_t * cmd = (cmd_releasesw_t *)daisy_Oalloc(address, id, CMD_RELEASESW, sizeof(cmd_releasesw_t));
  if (cmd != NULL) *cmd = { .action = action, .dir = dir };
  daisy_Opack(cmd);
  return cmd != NULL;
}

bool daisy_gohome(uint8_t address, id_t id) {
  void * p = daisy_Oalloc(address, id, CMD_GOHOME, 0);
  daisy_Opack(p);
  return p != NULL;
}

bool daisy_gomark(uint8_t address, id_t id) {
  void * p = daisy_Oalloc(address, id, CMD_GOMARK, 0);
  daisy_Opack(p);
  return p != NULL;
}

bool daisy_resetpos(uint8_t address, id_t id) {
  void * p = daisy_Oalloc(address, id, CMD_RESETPOS, 0);
  daisy_Opack(p);
  return p != NULL;
}

bool daisy_setpos(uint8_t address, id_t id, int32_t pos) {
  cmd_setpos_t * cmd = (cmd_setpos_t *)daisy_Oalloc(address, id, CMD_SETPOS, sizeof(cmd_setpos_t));
  if (cmd != NULL) *cmd = { .pos = pos };
  daisy_Opack(cmd);
  return cmd != NULL;
}

bool daisy_setmark(uint8_t address, id_t id, int32_t mark) {
  cmd_setpos_t * cmd = (cmd_setpos_t *)daisy_Oalloc(address, id, CMD_SETMARK, sizeof(cmd_setpos_t));
  if (cmd != NULL) *cmd = { .pos = mark };
  daisy_Opack(cmd);
  return cmd != NULL;
}

bool daisy_setconfig(uint8_t address, id_t id, const char * data) {
  size_t ldata = strlen(data) + 1;
  void * buf = daisy_Oalloc(address, id, CMD_SETCONFIG, ldata);
  if (buf != NULL) memcpy(buf, data, ldata);
  daisy_Opack(buf);
  return buf != NULL;
}

bool daisy_waitbusy(uint8_t address, id_t id) {
  void * p = daisy_Oalloc(address, id, CMD_WAITBUSY, 0);
  daisy_Opack(p);
  return p != NULL;
}

bool daisy_waitrunning(uint8_t address, id_t id) {
  void * p = daisy_Oalloc(address, id, CMD_WAITRUNNING, 0);
  daisy_Opack(p);
  return p != NULL;
}

bool daisy_waitms(uint8_t address, id_t id, uint32_t millis) {
  cmd_waitms_t * cmd = (cmd_waitms_t *)daisy_Oalloc(address, id, CMD_WAITMS, sizeof(cmd_waitms_t));
  if (cmd != NULL) *cmd = { .millis = millis };
  daisy_Opack(cmd);
  return cmd != NULL;
}

bool daisy_empty(uint8_t address, id_t id) {
  void * p = daisy_Oalloc(address, id, CMD_EMPTY, 0);
  daisy_Opack(p);
  return p != NULL;
}

bool daisy_estop(uint8_t address, id_t id, bool hiz, bool soft) {
  void * p = daisy_Oalloc(address, id, CMD_ESTOP, 0);
  daisy_Opack(p);
  return p != NULL;
}

