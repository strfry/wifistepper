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
//#define L_HEADER      (25)
#define L_MAGIC_1     (0xAE)
#define L_MAGIC_2     (0x7B11)
//#define L_VERSION     (0x01)
//#define LC_START      (8)

/*#define LO_MAGIC_1    (0)
#define LO_MAGIC_2    (1)
#define LO_VERSION    (3)
#define LO_CHECKSUM   (4)
#define LO_OPCODE     (8)
#define LO_SUBCODE    (9)
#define LO_ADDRESS    (10)
#define LO_NONCE      (11)
#define LO_ID         (15)
#define LO_LENGTH     (23)
*/

typedef struct ispacked {
  uint8_t magic1;
  uint16_t magic2;
  uint8_t type;
} lc_preamble;

typedef struct ispacked {
  uint8_t signature[8];
  uint32_t nonce[4];
  uint64_t challenge;
} lc_crypto;

typedef struct ispacked {
  uint8_t opcode;
  uint8_t subcode;
  uint8_t address;
  uint16_t packetid;
  uint16_t length;
} lc_header;

//#define lt_len(len)   (L_HEADER + (len))
//static inline uint8_t * lt_body(uint8_t * packet) { return &packet[L_HEADER]; }

#define ADDR_NONE         (0xFF)

#define TYPE_ERROR        (0x00)
#define TYPE_HELLO        (0x01)
#define TYPE_GOODBYE      (0x02)
#define TYPE_STD          (0x03)
#define TYPE_CRYPTO       (0x04)
#define TYPE_MAX          TYPE_CRYPTO

typedef struct ispacked {
  bool std_enabled;
  bool crypto_enabled;
  uint32_t chipid;
  char hostname[LEN_HOSTNAME];
} type_hello;

#define OPCODE_PING       (0x00)

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
  struct {
    uint32_t nonce;
    uint64_t challenge;
  } crypto;
  struct {
    unsigned long ping;
  } last;
} lowtcp_client[LTC_SIZE];

/*typedef union { uint64_t u64; uint8_t b[8]; } lt8_t;
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
#define lt_nextid(c)    (++((c).last.id))*/

/*static void lt_pack(uint8_t * packet, size_t length, uint8_t opcode, uint8_t subcode, uint8_t address, uint32_t nonce, uint64_t id) {
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
}*/

static inline void lc_packpreamble(lc_preamble * p, uint8_t type) {
  if (p == NULL) return;
  p->magic1 = L_MAGIC_1;
  p->magic2 = L_MAGIC_2;
  p->type = type;
}

static void lc_send(size_t client, uint8_t * data, size_t len) {
  size_t i = 0;
  if (lowtcp_client[client].Olen == 0 && lowtcp_client[i].sock.availableForWrite() > 0) {
    i = lowtcp_client[i].sock.write(data, len);
  }
  if (i != len) {
    if ((len - i) > (LTCB_OSIZE - lowtcp_client[client].Olen)) {
      seterror(ESUB_LC, 0, ETYPE_OBUF, client);
      return;
    }
    memcpy(&lowtcp_client[client].O[lowtcp_client[client].Olen], &data[i], len - i);
    lowtcp_client[client].Olen += len - i;
  }
}

#define lc_expectlen(elen)  ({ if (len != (elen)) { seterror(ESUB_LC, 0, ETYPE_MSG, client); return; } })

static void lc_handlepacket(size_t client, uint8_t opcode, uint8_t subcode, uint8_t address, uint16_t packetid, uint8_t * data, size_t len) {
  switch (opcode) {
    case OPCODE_ESTOP: {
      lc_expectlen(sizeof(cmd_stop_t));
      cmd_stop_t * cmd = (cmd_stop_t *)data;
      m_estop(address, nextid(), cmd->hiz, cmd->soft);
      break;
    }
    case OPCODE_STOP: {
      lc_expectlen(sizeof(cmd_stop_t));
      cmd_stop_t * cmd = (cmd_stop_t *)data;
      m_stop(address, nextid(), cmd->hiz, cmd->soft);
      break;
    }
    case OPCODE_GOTO: {
      lc_expectlen(sizeof(cmd_goto_t));
      cmd_goto_t * cmd = (cmd_goto_t *)data;
      m_goto(address, nextid(), cmd->pos, cmd->hasdir, cmd->dir);
      break;
    }
    case OPCODE_RUN: {
      lc_expectlen(sizeof(cmd_run_t));
      cmd_run_t * cmd = (cmd_run_t *)data;
      m_run(address, nextid(), cmd->dir, cmd->stepss);
      break;
    }
    case OPCODE_WAITBUSY: {
      lc_expectlen(0);
      m_waitbusy(address, nextid());
      break;
    }
    case OPCODE_WAITRUNNING: {
      lc_expectlen(0);
      m_waitrunning(address, nextid());
      break;
    }
    case OPCODE_WAITMS: {
      lc_expectlen(sizeof(cmd_waitms_t));
      cmd_waitms_t * cmd = (cmd_waitms_t *)data;
      m_waitms(address, nextid(), cmd->millis);
      break;
    }
  }
}

static size_t lc_handletype(size_t client, uint8_t * data, size_t len) {
  lc_preamble * preamble = (lc_preamble *)data;

  // Check preamble
  if (preamble->magic2 != L_MAGIC_2 || preamble->type > TYPE_MAX) {
    // Bad preamble
    return 1;
  }
  
  switch (preamble->type) {
    case TYPE_HELLO: {
      uint8_t reply[sizeof(lc_preamble) + sizeof(type_hello)] = {0};
      
      // Set preamble
      {
        lc_preamble * preamble = (lc_preamble *)(&reply[0]);
        lc_packpreamble(preamble, TYPE_HELLO);
      }
      
      // Set reply payload
      {
        type_hello * payload = (type_hello *)(&reply[sizeof(lc_preamble)]);
        payload->std_enabled = true;    // TODO - set to correct value
        payload->crypto_enabled = true; // TODO - set to correct value
        payload->chipid = state.wifi.chipid;
        strncpy(payload->hostname, config.service.hostname, LEN_HOSTNAME-1);
      }
      
      lowtcp_client[client].initialized = true;
      lc_send(client, reply, sizeof(reply));
      return sizeof(lc_preamble);
    }

    case TYPE_GOODBYE: {
      lowtcp_client[client].sock.stop();
      lowtcp_client[client].active = false;
      return sizeof(lc_preamble);
    }

    case TYPE_STD: {
      // Ensure at lease full header in buffer
      size_t elen = sizeof(lc_preamble) + sizeof(lc_header);
      if (len < elen) return 0;

      // Capture header
      lc_header * header = (lc_header *)data;

      // Ensure full packet in buffer
      elen += header->length;
      if (len < elen) return 0;

      // TODO - validate header
      lc_handlepacket(client, header->opcode, header->subcode, header->address, header->packetid, (uint8_t *)(&header[1]), header->length);
      return elen;
    }

    case TYPE_CRYPTO: {
      // TODO - not supported yet
      return 1;
    }

    default: {
      // Unknown type.
      return 1;
    }
  }

  /*uint8_t opcode = B[LO_OPCODE];
  size_t length = lt_buf2uint16(&B[LO_LENGTH]);
  switch (opcode) {
    case OPCODE_PING: {
      lowtcp_client[ci].last.ping = now;
      break;
    }
    default: {
      lt_handle(ci, opcode, B[LO_SUBCODE], B[LO_ADDRESS], &B[L_HEADER], length);
      break;
    }
  }*/
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
  for (size_t ci = 0; ci < LTC_SIZE; ci++) {
    if (lowtcp_client[ci].active && lowtcp_client[ci].sock.available()) {
      // Check limit
      if (lowtcp_client[ci].Ilen == LTCB_ISIZE) {
        // Input buffer is full, drain here (bad data, could not parse)
        seterror(ESUB_LC, 0, ETYPE_IBUF, ci);
        lowtcp_client[ci].Ilen = 0;
      }
      
      // Read in as much as possible
      size_t bytes = lowtcp_client[ci].sock.read(&lowtcp_client[ci].I[lowtcp_client[ci].Ilen], LTCB_ISIZE - lowtcp_client[ci].Ilen);
      lowtcp_client[ci].Ilen += bytes;
    }
  }

  // Handle payload
  for (size_t ci = 0; ci < LTC_SIZE; ci++) {
    if (lowtcp_client[ci].active && lowtcp_client[ci].Ilen > 0) {
      uint8_t * B = lowtcp_client[ci].I;
      size_t Blen = lowtcp_client[ci].Ilen;

      // Check for valid SOF
      if (B[0] != L_MAGIC_1) {
        // Not valid, find first instance of magic, sync to that
        size_t index = 1;
        for (; index < Blen; index++) {
          if (B[index] == L_MAGIC_1) break;
        }

        if (index < lowtcp_client[ci].Ilen) memmove(B, &B[index], Blen - index);
        lowtcp_client[ci].Ilen -= index;
        continue;
      }

      // Ensure full preamble
      if (Blen < sizeof(lc_preamble)) continue;
      
      // Consume packet
      size_t consume = lc_handletype(ci, B, Blen);

      // Clear packet from buffer
      if (consume > 0) {
        memmove(B, &B[consume], Blen - consume);
        lowtcp_client[ci].Ilen -= consume;
      }
    }
  }
}

void lowtcp_update(unsigned long now) {
  if (lowtcp_server.hasClient()) {
    size_t ci = 0;
    for (; ci < LTC_SIZE; ci++) {
      if (!lowtcp_client[ci].active) {
        memset(&lowtcp_client[ci], 0, sizeof(lowtcp_client[ci]));
        lowtcp_client[ci].sock = lowtcp_server.available();
        lowtcp_client[ci].active = true;
        //lowtcp_client[i].nonce = ?  // TODO
        lowtcp_client[ci].last.ping = now;
        break;
      }
    }

    if (ci == LTC_SIZE) {
      // No open slots
      lowtcp_server.available().stop();
    }
  }

  for (size_t ci = 0; ci < LTC_SIZE; ci++) {
    if (lowtcp_client[ci].active && (timesince(lowtcp_client[ci].last.ping, now) > LTO_PING || !lowtcp_client[ci].sock.connected())) {
      // Socket has timed out
      if (lowtcp_client[ci].sock.connected()) lowtcp_client[ci].sock.stop();
      lowtcp_client[ci].active = false;
    }
  }

  if (timesince(sketch.service.lowtcp.last.ping, now) > LTO_PING) {
    sketch.service.lowtcp.last.ping = now;
    state.service.lowtcp.clients = 0;

    for (size_t i = 0; i < LTC_SIZE; i++) {
      if (lowtcp_client[i].active) {
        /*
        uint8_t packet[lt_len(0)] = {0};
        size_t lpacket = sizeof(packet);
        lt_pack(packet, lpacket, OPCODE_PING, SUBCODE_NACK, ADDR_NONE, lt_nonce(lowtcp_client[i]), lt_nextid(lowtcp_client[i]));
        if (lowtcp_client[i].sock.availableForWrite() >= lpacket) lowtcp_client[i].sock.write(packet, lpacket);*/
        state.service.lowtcp.clients += 1;
      }
    }
  }
}

