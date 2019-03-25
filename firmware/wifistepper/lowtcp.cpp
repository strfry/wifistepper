#include <Arduino.h>
//#include <ArduinoJson.h>
#include <ESP8266WiFi.h>

#include "wifistepper.h"

#define LTC_SIZE      (3)
#define LTC_PORT      (1000)

#define LTCB_ISIZE     (2048)
#define LTCB_OSIZE     (LTCB_ISIZE / 2)

// PACKET LAYOUT
// | MAGIC_1 (1) | MAGIC_2 (2) | VERSION (1) | CHECKSUM (4) | OPCODE (1) | SUBCODE (1) | ADDRESS (1) | NONCE (4) | ID (8) | LENGTH (2) | ... DATA (size = LENGTH) ... |
#define L_HEADER      (25)
#define L_MAGIC_1     (0xAE)
#define L_MAGIC_2     (0x7B11)
#define L_VERSION     (0x01)
#define LC_START      (8)

#define LO_MAGIC_1    (0)
#define LO_MAGIC_2    (1)
#define LO_VERSION    (3)
#define LO_CHECKSUM   (4)
#define LO_OPCODE     (8)
#define LO_SUBCODE    (9)
#define LO_ADDRESS    (10)
#define LO_NONCE      (11)
#define LO_ID         (15)
#define LO_LENGTH     (23)

#define lt_len(len)   (L_HEADER + (len))
static inline uint8_t * lt_body(uint8_t * packet) { return &packet[L_HEADER]; }

#define ADDR_NONE         (0xFF)

#define OPCODE_PING       (0x00)
#define OPCODE_HELO       (0x01)

#define OPCODE_ESTOP      (0x11)
#define OPCODE_STOP       (0x12)
#define OPCODE_GOTO       (0x13)
#define OPCODE_RUN        (0x14)
#define OPCODE_WAITBUSY   (0x21)
#define OPCODE_WAITRUNNING (0x22)
#define OPCODE_WAITMS     (0x23)

#define SUBCODE_NACK      (0x00)
#define SUBCODE_ACK       (0x01)
#define SUBCODE_CMD       (0x02)

#define LTO_PING          (1000)

WiFiServer lowtcp_server(LTC_PORT);
struct {
  WiFiClient sock;
  uint8_t I[LTCB_ISIZE];
  uint8_t O[LTCB_OSIZE];
  size_t Ilen, Olen;
  bool active;
  bool initialized;
  uint32_t nonce;
  struct {
    uint64_t id;
    unsigned long ping;
  } last;
} lowtcp_client[LTC_SIZE];

typedef union { uint64_t u64; uint8_t b[8]; } lt8_t;
typedef union { float f32; int32_t i32; uint32_t u32; uint8_t b[4]; } lt4_t;
typedef union { uint16_t u16; uint8_t b[2]; } lt2_t;

static inline uint64_t lt_buf2uint64(uint8_t * b) { lt8_t c; c.b[0] = b[0]; c.b[1] = b[1]; c.b[2] = b[2]; c.b[3] = b[3]; c.b[4] = b[4]; c.b[5] = b[5]; c.b[6] = b[6]; c.b[7] = b[7]; return c.u64; }
static inline void lt_packuint64(uint64_t i, uint8_t * b) { lt8_t c; c.u64 = i; b[0] = c.b[0]; b[1] = c.b[1]; b[2] = c.b[2]; b[3] = c.b[3]; b[4] = c.b[4]; b[5] = c.b[5]; b[6] = c.b[6]; b[7] = c.b[7]; }

static inline float lt_buf2float(uint8_t * b) { lt4_t c; c.b[0] = b[0]; c.b[1] = b[1]; c.b[2] = b[2]; c.b[3] = b[3]; return c.f32; }
static inline void lt_packfloat(float f, uint8_t * b) { lt4_t c; c.f32 = f; b[0] = c.b[0]; b[1] = c.b[1]; b[2] = c.b[2]; b[3] = c.b[3]; }

static inline int32_t lt_buf2int32(uint8_t * b) { lt4_t c; c.b[0] = b[0]; c.b[1] = b[1]; c.b[2] = b[2]; c.b[3] = b[3]; return c.i32; }
static inline void lt_packint32(int32_t i, uint8_t * b) { lt4_t c; c.i32 = i; b[0] = c.b[0]; b[1] = c.b[1]; b[2] = c.b[2]; b[3] = c.b[3]; }

static inline uint32_t lt_buf2uint32(uint8_t * b) { lt4_t c; c.b[0] = b[0]; c.b[1] = b[1]; c.b[2] = b[2]; c.b[3] = b[3]; return c.u32; }
static inline void lt_packuint32(uint32_t i, uint8_t * b) { lt4_t c; c.u32 = i; b[0] = c.b[0]; b[1] = c.b[1]; b[2] = c.b[2]; b[3] = c.b[3]; }

static inline uint16_t lt_buf2uint16(uint8_t * b) { lt2_t c; c.b[0] = b[0]; c.b[1] = b[1]; return c.u16; }
static inline void lt_packuint16(uint16_t i, uint8_t * b) { lt2_t c; c.u16 = i; b[0] = c.b[0]; b[1] = c.b[1]; }

#define lt_nonce(c)     ((c).nonce)
#define lt_nextid(c)    (++((c).last.id))

static uint32_t lt_checksum32(uint8_t * data, size_t length) {
  
}

static void lt_pack(uint8_t * packet, size_t length, uint8_t opcode, uint8_t subcode, uint8_t address, uint32_t nonce, uint64_t id) {
  packet[LO_MAGIC_1] = L_MAGIC_1;
  lt_packuint16(L_MAGIC_2, &packet[LO_MAGIC_2]);
  packet[LO_VERSION] = L_VERSION;
  packet[LO_OPCODE] = opcode;
  packet[LO_SUBCODE] = subcode;
  packet[LO_ADDRESS] = address;
  lt_packuint32(nonce, &packet[LO_NONCE]);
  lt_packuint64(id, &packet[LO_ID]);
  lt_packuint16(length - L_HEADER, &packet[LO_LENGTH]);
  lt_packuint32(lt_checksum32(&packet[LC_START], length - LC_START), &packet[LO_CHECKSUM]);
}

static void lt_handle(size_t i, uint8_t opcode, uint8_t subcode, uint8_t address, uint8_t * data, size_t length) {
  switch (opcode) {
    case OPCODE_ESTOP: {
      bool hiz = data[0];
      bool soft = data[1];
      m_estop(address, nextid(), hiz, soft);
      break;
    }
    case OPCODE_STOP: {
      bool hiz = data[0];
      bool soft = data[1];
      m_stop(address, nextid(), hiz, soft);
      break;
    }
    case OPCODE_GOTO: {
      int32_t pos = lt_buf2int32(&data[0]);
      m_goto(address, nextid(), pos);
      break;
    }
    case OPCODE_RUN: {
      ps_direction dir = data[0]? FWD : REV;
      float stepss = lt_buf2float(&data[1]);
      m_run(address, nextid(), dir, stepss);
      break;
    }
    case OPCODE_WAITBUSY: {
      m_waitbusy(address, nextid());
      break;
    }
    case OPCODE_WAITRUNNING: {
      m_waitrunning(address, nextid());
      break;
    }
    case OPCODE_WAITMS: {
      int32_t millis = lt_buf2uint32(&data[0]);
      m_waitms(address, nextid(), millis);
      break;
    }
  }
}

void lowtcp_init() {
  lowtcp_server.begin();
  lowtcp_server.setNoDelay(true);

  for (size_t i = 0; i < LTC_SIZE; i++) {
    lowtcp_client[i].active = false;
  }
}

void lowtcp_loop(unsigned long now) {
  // Check all packets for rx
  for (size_t i = 0; i < LTC_SIZE; i++) {
    if (lowtcp_client[i].active && lowtcp_client[i].sock.available()) {
      // Check limit
      if (lowtcp_client[i].Ilen == LTCB_ISIZE) {
        // Input buffer is full, drain here (bad data, could not parse)
        seterror(ESUB_LOWTCP, 0, ETYPE_IBUF, i);
        lowtcp_client[i].Ilen = 0;
      }
      
      // Read in as much as possible
      size_t bytes = lowtcp_client[i].sock.read(&lowtcp_client[i].I[lowtcp_client[i].Ilen], LTCB_ISIZE - lowtcp_client[i].Ilen);
      lowtcp_client[i].Ilen += bytes;
    }
  }

  // Handle payload
  for (size_t i = 0; i < LTC_SIZE; i++) {
    if (lowtcp_client[i].active && lowtcp_client[i].Ilen > 0) {
      uint8_t * B = lowtcp_client[i].I;
      size_t Blen = lowtcp_client[i].Ilen;

      // Check for valid SOF
      if (B[LO_MAGIC_1] != L_MAGIC_1) {
        // Not valid, find first instance of magic, sync to that
        size_t index = 1;
        for (; index < Blen; index++) {
          if (B[index] == L_MAGIC_1) break;
        }

        if (index < lowtcp_client[i].Ilen) memmove(B, &B[index], Blen - index);
        lowtcp_client[i].Ilen -= index;
        continue;
      }

      // Ensure full header is in buffer
      if (Blen < L_HEADER) continue;

      // Check for valid packet header
      // TODO - (assumes valid packet)

      uint8_t opcode = B[LO_OPCODE];
      size_t length = lt_buf2uint16(&B[LO_LENGTH]);
      switch (opcode) {
        case OPCODE_PING: {
          lowtcp_client[i].last.ping = now;
          break;
        }
        default: {
          lt_handle(i, opcode, B[LO_SUBCODE], B[LO_ADDRESS], &B[L_HEADER], length);
          break;
        }
      }

      // Clear packet from buffer
      size_t fullsize = L_HEADER + length;
      memmove(B, &B[fullsize], Blen - fullsize);
      lowtcp_client[i].Ilen -= fullsize;
    }
  }
}

void lowtcp_update(unsigned long now) {
  if (lowtcp_server.hasClient()) {
    size_t i = 0;
    for (; i < LTC_SIZE; i++) {
      if (!lowtcp_client[i].active) {
        memset(&lowtcp_client[i], 0, sizeof(lowtcp_client[i]));
        lowtcp_client[i].sock = lowtcp_server.available();
        lowtcp_client[i].active = true;
        //lowtcp_client[i].nonce = ?  // TODO
        lowtcp_client[i].last.ping = now;
        break;
      }
    }

    if (i == LTC_SIZE) {
      // No open slots
      //lowtcp_server.available().stop();
    }
  }

  for (size_t i = 0; i < LTC_SIZE; i++) {
    if (lowtcp_client[i].active && (timesince(lowtcp_client[i].last.ping, now) > LTO_PING || !lowtcp_client[i].sock.connected())) {
      // Socket has timed out
      //if (lowtcp_client[i].sock.connected()) lowtcp_client[i].sock.stop();
      lowtcp_client[i].active = false;
    }
  }

  if (timesince(sketch.service.lowtcp.last.ping, now) > LTO_PING) {
    sketch.service.lowtcp.last.ping = now;
    state.service.lowtcp.clients = 0;

    uint8_t packet[lt_len(0)] = {0};
    size_t lpacket = sizeof(packet);
    for (size_t i = 0; i < LTC_SIZE; i++) {
      if (lowtcp_client[i].active) {
        lt_pack(packet, lpacket, OPCODE_PING, SUBCODE_NACK, ADDR_NONE, lt_nonce(lowtcp_client[i]), lt_nextid(lowtcp_client[i]));
        if (lowtcp_client[i].sock.availableForWrite() >= lpacket) lowtcp_client[i].sock.write(packet, lpacket);
        state.service.lowtcp.clients += 1;
      }
    }
  }
}

