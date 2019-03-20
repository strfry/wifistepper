#include <Arduino.h>

#include "wifistepper.h"
#include "cmdpriv.h"

#define B_SIZE          (2048)
#define B_MAGIC1        (0xAB)
#define B_MAGIC2        (0x75)

#define B_HEADER          (11)
#define BO_MAGIC1         (0)
#define BO_MAGIC2         (1)
#define BO_CHECKSUM_HEAD  (2)
#define BO_CHECKSUM_BODY  (3)
#define BO_ADDRESS        (4)
#define BO_PACKET_ID      (5)
#define BO_OPCODE         (9)
#define BO_LENGTH         (10)


#define SELF            (0x00)


#define CMD_PING        (0x00)
#define CMD_ACK         (0x01)

#define CMD_STOP        (0x02)
#define CMD_RUN         (0x03)
#define CMD_STEPCLK     (0x04)
#define CMD_MOVE        (0x05)
#define CMD_GOTO        (0x06)
#define CMD_GOUNTIL     (0x07)
#define CMD_RELEASESW   (0x08)
#define CMD_GOHOME      (0x09)
#define CMD_GOMARK      (0x0A)
#define CMD_RESETPOS    (0x0B)
#define CMD_SETPOS      (0x0C)
#define CMD_SETMARK     (0x0D)
#define CMD_SETCONFIG   (0x0E)
#define CMD_WAITBUSY    (0x0F)
#define CMD_WAITRUNNING (0x10)
#define CMD_WAITMS      (0x11)
#define CMD_EMPTY       (0x12)
#define CMD_ESTOP       (0x13)

#define CMD_SIZE        (20)


typedef union {
  uint8_t b[4];
  float f32;
  int32_t i32;
  uint32_t u32;
} bc4_t;

static inline float b_buf2float(uint8_t * b) {bc4_t c; c.b[0] = b[0]; c.b[1] = b[1]; c.b[2] = b[2]; c.b[3] = b[3]; return c.f32; }
static inline void b_packfloat(float f, uint8_t * b) { bc4_t c; c.f32 = f; b[0] = c.b[0]; b[1] = c.b[1]; b[2] = c.b[2]; b[3] = c.b[3]; }
static inline int32_t b_buf2int32(uint8_t * b) { bc4_t c; c.b[0] = b[0]; c.b[1] = b[1]; c.b[2] = b[2]; c.b[3] = b[3]; return c.i32; }
static inline void b_packint32(int32_t i, uint8_t * b) { bc4_t c; c.i32 = i; b[0] = c.b[0]; b[1] = c.b[1]; b[2] = c.b[2]; b[3] = c.b[3]; }

// In Buffer
uint8_t B[B_SIZE] = {0};
volatile size_t Blen = 0;
volatile size_t Bskip = 0;

// Outbox
uint8_t O[B_SIZE] = {0};
volatile size_t Olen = 0;
#define daisy_dumpoutbox()  ({ if (Olen > 0) { Serial.write(O, Olen); Olen = 0; } })

// Ack list
#define A_SIZE        (1024)
id_t A[A_SIZE] = {0};
volatile size_t Alen = 0;


static uint8_t daisy_checksum8(uint8_t * data, size_t length) {
  unsigned int sum = 0;
  for (size_t i = 0; i < length; i++) {
    sum += data[i];
  }
  return (uint8_t)sum;
}

void daisy_init() {
  Serial.begin(servicecfg.daisycfg.baudrate);
  
}

static void * daisy_Oalloc(uint8_t address, id_t id, uint8_t opcode, size_t len) {
  // Check if we have enough memory in buffers
  if (len > 0xFF || (B_SIZE - Olen) < (B_HEADER + len) || Alen >= A_SIZE) return NULL;
  
  // Add id to ack list
  A[Alen++] = id;

  // Allocate and initialize packet
  uint8_t * packet = &O[Olen];
  packet[BO_MAGIC1] = B_MAGIC1;
  packet[BO_MAGIC2] = B_MAGIC2;
  packet[BO_ADDRESS] = address;
  *(id_t *)(&packet[BO_PACKET_ID]) = id;
  packet[BO_OPCODE] = opcode;
  packet[BO_LENGTH] = (uint8_t)len;

  // Extend outbox buffer size
  Olen += B_HEADER + len;

  // Return the packet payload
  return &packet[B_HEADER];
}

static void daisy_Opack(void * data) {
  if (data == NULL) return;

  // Find start of packet
  uint8_t * packet = ((uint8_t *)data) - B_HEADER;

  // Compute checksums
  size_t len = packet[BO_LENGTH];
  packet[BO_CHECKSUM_BODY] = daisy_checksum8(&packet[B_HEADER], len);
  packet[BO_CHECKSUM_HEAD] = daisy_checksum8(&packet[BO_CHECKSUM_BODY], B_HEADER - BO_CHECKSUM_BODY);
}

static void daisy_master_consume(int8_t address, id_t id, uint8_t opcode, void * data, uint8_t len) {
  
}

static void daisy_ack(id_t id) {
  daisy_Opack(daisy_Oalloc(SELF, id, CMD_ACK, 0));
}

static void daisy_slave_consume(id_t id, uint8_t opcode, void * data, uint8_t len) {
  
}

void daisy_loop() {
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
    
    // Packets are formatted (in bytes bytes)
    // | MAGIC_1 | MAGIC_2 | CHECKSUM_HEAD | CHECKSUM_BODY | ADDRESS | PACKET_ID[0] | PACKET_ID[1] | PACKET_ID[2] | PACKET_ID[3] | OPCODE | LEN | ... DATA (size = LEN) ... |
    // Header is first 11 bytes, payload is the following LEN bytes
    // CHECKSUM_BODY is calculated first using daisy_checksum8() on all DATA (size = LEN)
    // CHECKSUM_HEAD is calculated second using daisy_checksum8() on all bytes starting from CHECKSUM_BODY and ending with LEN (8 bytes total)
    
    // Sync to start of packet (SOP)
    size_t i = 0;
    for (; i < Blen; i++) {
      if (B[i] == B_MAGIC1) break;
    }

    // Forward all unparsed data if not master
    if (i > 0 && !servicecfg.daisycfg.master) Serial.write(B, i);
  
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
    if (Blen < B_HEADER) return;

    // Check rest of header
    bool isvalid = B[BO_MAGIC2] == B_MAGIC2 && B[BO_OPCODE] < CMD_SIZE && daisy_checksum8(&B[BO_CHECKSUM_BODY], B_HEADER - BO_CHECKSUM_BODY) == B[BO_CHECKSUM_HEAD];
    if (isvalid && !servicecfg.daisycfg.master && B[BO_ADDRESS] != 0x01) {
      // We're not master and the packet is not for us, skip it
      B[BO_ADDRESS] -= 1;
      Bskip = B_HEADER + B[BO_LENGTH];
      continue;
    }
    if (!isvalid) {
      // Not a valid header, shift out one byte and continue
      if (!servicecfg.daisycfg.master) Serial.write(B, 1);
      memmove(B, &B[1], Blen - 1);
      continue;
    }

    // Make sure we have whole packet
    uint8_t len = B[BO_LENGTH];
    if (Blen < (B_HEADER + len)) return;

    // Validate checksum
    if (daisy_checksum8(&B[B_HEADER], len) != B[BO_CHECKSUM_BODY]) {
      // Bad checksum, shift out one byte and continue
      if (!servicecfg.daisycfg.master) Serial.write(B, 1);
      memmove(B, &B[1], Blen - 1);
      continue;
    }

    // Opportunistically dump outbox here
    daisy_dumpoutbox();

    // Consume packet
    {
      int8_t addr = (int8_t)B[BO_ADDRESS];
      id_t id = *(id_t *)(&B[BO_PACKET_ID]);
      uint8_t opcode = B[BO_OPCODE];
      void * data = &B[B_HEADER];
      uint8_t len = B[BO_LENGTH];
      if (servicecfg.daisycfg.master)   daisy_master_consume(addr, id, opcode, data, len);
      else                              daisy_slave_consume(id, opcode, data, len);
    }

    // Opportunistically dump outbox here
    daisy_dumpoutbox();

    // Clear packet from buffer
    memmove(B, &B[B_HEADER + len], Blen - (B_HEADER + len));
    Blen -= B_HEADER + len;
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

