#include <Arduino.h>
//#include <ArduinoJson.h>
#include <ESP8266WiFi.h>

#include "wifistepper.h"

#define LTC_SIZE      (2)
#define LTC_PORT      (1000)

#define LTCB_ISIZE     (1024)
#define LTCB_OSIZE     (LTCB_ISIZE / 4)

// PACKET LAYOUT
#define L_MAGIC_1     (0xAE)
#define L_MAGIC_2     (0x7B11)

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
  uint8_t queue;
  uint16_t packetid;
  uint16_t length;
} lc_header;

#define TYPE_ERROR        (0x00)
#define TYPE_HELLO        (0x01)
#define TYPE_GOODBYE      (0x02)
#define TYPE_PING         (0x03)
#define TYPE_STD          (0x04)
#define TYPE_CRYPTO       (0x05)
#define TYPE_MAX          TYPE_CRYPTO

typedef struct ispacked {
  char product[LEN_PRODUCT];
  char model[LEN_INFO];
  char swbranch[LEN_INFO];
  uint16_t version;
  char hostname[LEN_HOSTNAME];
  uint32_t chipid;
  bool std_enabled;
  bool crypto_enabled;
} type_hello;

#define OPCODE_ESTOP        (0x00)
#define OPCODE_SETCONFIG    (0x01)
#define OPCODE_GETCONFIG    (0x02)
#define OPCODE_LASTWILL     (0x03)

#define OPCODE_STOP         (0x11)
#define OPCODE_RUN          (0x12)
#define OPCODE_STEPCLOCK    (0x13)
#define OPCODE_MOVE         (0x14)
#define OPCODE_GOTO         (0x15)
#define OPCODE_GOUNTIL      (0x16)
#define OPCODE_RELEASESW    (0x17)
#define OPCODE_GOHOME       (0x18)
#define OPCODE_GOMARK       (0x19)
#define OPCODE_RESETPOS     (0x1A)
#define OPCODE_SETPOS       (0x1B)
#define OPCODE_SETMARK      (0x1C)

#define OPCODE_WAITBUSY     (0x21)
#define OPCODE_WAITRUNNING  (0x22)
#define OPCODE_WAITMS       (0x23)
#define OPCODE_WAITSWITCH   (0x24)

#define OPCODE_EMPTYQUEUE   (0x31)
#define OPCODE_SAVEQUEUE    (0x32)
#define OPCODE_LOADQUEUE    (0x33)
#define OPCODE_ADDQUEUE     (0x34)
#define OPCODE_COPYQUEUE    (0x35)
#define OPCODE_RUNQUEUE     (0x36)
#define OPCODE_GETQUEUE     (0x37)


#define SUBCODE_NACK      (0x00)
#define SUBCODE_ACK       (0x01)
#define SUBCODE_CMD       (0x02)

#define LTO_PING          (3000)

#define LOWCOM_DEBUG

#ifdef LOWCOM_DEBUG
void lowcom_debug(String msg) {
  Serial.print("(");
  Serial.print(millis());
  Serial.print(") LOWCOM -> ");
  Serial.println(msg);
}
void lowcom_debug(String msg, int i1) {
  Serial.print("(");
  Serial.print(millis());
  Serial.print(") LOWCOM -> ");
  Serial.print(msg);
  Serial.print(" (");
  Serial.print(i1);
  Serial.println(")");
}
void lowcom_debug(String msg, int i1, float f1) {
  Serial.print("(");
  Serial.print(millis());
  Serial.print(") LOWCOM -> ");
  Serial.print(msg);
  Serial.print(" (");
  Serial.print(i1);
  Serial.print(", ");
  Serial.print(f1);
  Serial.println(")");
}
void lowcom_debug(String msg, int i1, int i2) {
  Serial.print("(");
  Serial.print(millis());
  Serial.print(") LOWCOM -> ");
  Serial.print(msg);
  Serial.print(" (");
  Serial.print(i1);
  Serial.print(", ");
  Serial.print(i2);
  Serial.println(")");
}
void lowcom_debug(String msg, int i1, int i2, int i3) {
  Serial.print("(");
  Serial.print(millis());
  Serial.print(") LOWCOM -> ");
  Serial.print(msg);
  Serial.print(" (");
  Serial.print(i1);
  Serial.print(", ");
  Serial.print(i2);
  Serial.print(", ");
  Serial.print(i3);
  Serial.println(")");
}
void lowcom_debug(String msg, int i1, int i2, int i3, int i4, int i5) {
  Serial.print("(");
  Serial.print(millis());
  Serial.print(") LOWCOM -> ");
  Serial.print(msg);
  Serial.print(" (");
  Serial.print(i1);
  Serial.print(", ");
  Serial.print(i2);
  Serial.print(", ");
  Serial.print(i3);
  Serial.print(", ");
  Serial.print(i4);
  Serial.print(", ");
  Serial.print(i5);
  Serial.println(")");
}
#else
#define lowcom_debug(...)
#endif

WiFiServer lowcom_server(LTC_PORT);
struct {
  WiFiClient sock;
  uint8_t I[LTCB_ISIZE];
  uint8_t O[LTCB_OSIZE];
  size_t Ilen, Olen;
  bool active;
  bool initialized;
  uint8_t lastwill;
  struct {
    uint32_t nonce;
    uint16_t packetid;
  } crypto;
  struct {
    unsigned long ping;
  } last;
} lowcom_client[LTC_SIZE];

static inline void lc_packpreamble(lc_preamble * p, uint8_t type) {
  if (p == NULL) return;
  p->magic1 = L_MAGIC_1;
  p->magic2 = L_MAGIC_2;
  p->type = type;
}

static void lc_send(size_t client, uint8_t * data, size_t len) {
  size_t i = 0;
  if (lowcom_client[client].Olen == 0 && lowcom_client[i].sock.availableForWrite() > 0) {
    i = lowcom_client[i].sock.write(data, len);
  }
  if (i != len) {
    if ((len - i) > (LTCB_OSIZE - lowcom_client[client].Olen)) {
      seterror(ESUB_LC, 0, ETYPE_OBUF, client);
      return;
    }
    memcpy(&lowcom_client[client].O[lowcom_client[client].Olen], &data[i], len - i);
    lowcom_client[client].Olen += len - i;
  }
}

static void lc_replyack(size_t client, uint8_t opcode, uint8_t address, uint8_t queue, uint16_t packetid, id_t id) {
  uint8_t packet[sizeof(lc_preamble) + sizeof(lc_header) + sizeof(id_t)] = {0};
  lc_preamble * preamble = (lc_preamble *)&packet[0];
  lc_header * header = (lc_header *)&preamble[1];
  id_t * ackid = (id_t *)&header[1];
  lc_packpreamble(preamble, TYPE_STD);
  *header = {.opcode = opcode, .subcode = SUBCODE_ACK, .address = address, .queue = queue, .packetid = packetid, .length = sizeof(id_t)};
  *ackid = id;
  lc_send(client, packet, sizeof(packet));
}

#define lc_expectlen(elen)  ({ if (len != (elen)) { seterror(ESUB_LC, 0, ETYPE_MSG, client); return; } })

static void lc_handlepacket(size_t client, uint8_t opcode, uint8_t subcode, uint8_t address, uint8_t queue, uint16_t packetid, uint8_t * data, size_t len) {
  lowcom_debug("CMD received", opcode, subcode, address, queue, packetid);

  // TODO check if subcode == CMD

  // Check if client initialized
  if (!lowcom_client[client].initialized) {
    lowcom_debug("ERR client not initialized");
    seterror(ESUB_LC, 0, ETYPE_MSG, client);
    return;
  }

  id_t id = nextid();
  switch (opcode) {
    case OPCODE_ESTOP: {
      lc_expectlen(sizeof(cmd_stop_t));
      cmd_stop_t * cmd = (cmd_stop_t *)data;
      lowcom_debug("CMD estop", cmd->hiz, cmd->soft);
      m_estop(address, id, cmd->hiz, cmd->soft);
      lc_replyack(client, opcode, address, queue, packetid, id);
      break;
    }
    case OPCODE_SETCONFIG: {
      if (len == 0 || data[len-1] != 0) {
        seterror(ESUB_LC, 0, ETYPE_MSG, client);
        return;
      }
      lowcom_debug("CMD setconfig");
      m_setconfig(address, queue, id, (const char *)data);
      lc_replyack(client, opcode, address, queue, packetid, id);
      break;
    }
    case OPCODE_GETCONFIG: {
      break;
    }
    case OPCODE_LASTWILL: {
      lc_expectlen(0);
      lowcom_debug("CMD lastwill", queue);
      lowcom_client[client].lastwill = queue;
      lc_replyack(client, opcode, 0, queue, packetid, id);
      break;
    }
    
    case OPCODE_STOP: {
      lc_expectlen(sizeof(cmd_stop_t));
      cmd_stop_t * cmd = (cmd_stop_t *)data;
      lowcom_debug("CMD stop", cmd->hiz, cmd->soft);
      m_stop(address, queue, id, cmd->hiz, cmd->soft);
      lc_replyack(client, opcode, address, queue, packetid, id);
      break;
    }
    case OPCODE_RUN: {
      lc_expectlen(sizeof(cmd_run_t));
      cmd_run_t * cmd = (cmd_run_t *)data;
      lowcom_debug("CMD run", cmd->dir, cmd->stepss);
      m_run(address, queue, id, cmd->dir, cmd->stepss);
      lc_replyack(client, opcode, address, queue, packetid, id);
      break;
    }
    case OPCODE_STEPCLOCK: {
      lc_expectlen(sizeof(cmd_stepclk_t));
      cmd_stepclk_t * cmd = (cmd_stepclk_t *)data;
      lowcom_debug("CMD stepclock", cmd->dir);
      m_stepclock(address, queue, id, cmd->dir);
      lc_replyack(client, opcode, address, queue, packetid, id);
      break;
    }
    case OPCODE_MOVE: {
      lc_expectlen(sizeof(cmd_move_t));
      cmd_move_t * cmd = (cmd_move_t *)data;
      lowcom_debug("CMD move", cmd->dir, (float)cmd->microsteps);
      m_move(address, queue, id, cmd->dir, cmd->microsteps);
      lc_replyack(client, opcode, address, queue, packetid, id);
      break;
    }
    case OPCODE_GOTO: {
      lc_expectlen(sizeof(cmd_goto_t));
      cmd_goto_t * cmd = (cmd_goto_t *)data;
      lowcom_debug("CMD goto", cmd->pos, cmd->hasdir, cmd->dir);
      m_goto(address, queue, id, cmd->pos, cmd->hasdir, cmd->dir);
      lc_replyack(client, opcode, address, queue, packetid, id);
      break;
    }
    case OPCODE_GOUNTIL: {
      lc_expectlen(sizeof(cmd_gountil_t));
      cmd_gountil_t * cmd = (cmd_gountil_t *)data;
      lowcom_debug("CMD gountil", cmd->action, cmd->dir, cmd->stepss);
      m_gountil(address, queue, id, cmd->action, cmd->dir, cmd->stepss);
      lc_replyack(client, opcode, address, queue, packetid, id);
      break;
    }
    case OPCODE_RELEASESW: {
      lc_expectlen(sizeof(cmd_releasesw_t));
      cmd_releasesw_t * cmd = (cmd_releasesw_t *)data;
      lowcom_debug("CMD releasesw", cmd->action, cmd->dir);
      m_releasesw(address, queue, id, cmd->action, cmd->dir);
      lc_replyack(client, opcode, address, queue, packetid, id);
      break;
    }
    case OPCODE_GOHOME: {
      lc_expectlen(0);
      lowcom_debug("CMD gohome");
      m_gohome(address, queue, id);
      lc_replyack(client, opcode, address, queue, packetid, id);
      break;
    }
    case OPCODE_GOMARK: {
      lc_expectlen(0);
      lowcom_debug("CMD gomark");
      m_gomark(address, queue, id);
      lc_replyack(client, opcode, address, queue, packetid, id);
      break;
    }
    case OPCODE_RESETPOS: {
      lc_expectlen(0);
      lowcom_debug("CMD resetpos");
      m_resetpos(address, queue, id);
      lc_replyack(client, opcode, address, queue, packetid, id);
      break;
    }
    case OPCODE_SETPOS: {
      lc_expectlen(sizeof(cmd_setpos_t));
      cmd_setpos_t * cmd = (cmd_setpos_t *)data;
      lowcom_debug("CMD setpos", cmd->pos);
      m_setpos(address, queue, id, cmd->pos);
      lc_replyack(client, opcode, address, queue, packetid, id);
      break;
    }
    case OPCODE_SETMARK: {
      lc_expectlen(sizeof(cmd_setpos_t));
      cmd_setpos_t * cmd = (cmd_setpos_t *)data;
      lowcom_debug("CMD setpos", cmd->pos);
      m_setmark(address, queue, id, cmd->pos);
      lc_replyack(client, opcode, address, queue, packetid, id);
      break;
    }
    
    case OPCODE_WAITBUSY: {
      lc_expectlen(0);
      lowcom_debug("CMD waitbusy");
      m_waitbusy(address, queue, id);
      lc_replyack(client, opcode, address, queue, packetid, id);
      break;
    }
    case OPCODE_WAITRUNNING: {
      lc_expectlen(0);
      lowcom_debug("CMD waitrunning");
      m_waitrunning(address, queue, id);
      lc_replyack(client, opcode, address, queue, packetid, id);
      break;
    }
    case OPCODE_WAITMS: {
      lc_expectlen(sizeof(cmd_waitms_t));
      cmd_waitms_t * cmd = (cmd_waitms_t *)data;
      lowcom_debug("CMD waitms", cmd->ms);
      m_waitms(address, queue, id, cmd->ms);
      lc_replyack(client, opcode, address, queue, packetid, id);
      break;
    }
    case OPCODE_WAITSWITCH: {
      lc_expectlen(sizeof(cmd_waitsw_t));
      cmd_waitsw_t * cmd = (cmd_waitsw_t *)data;
      lowcom_debug("CMD waitswitch", cmd->state);
      m_waitswitch(address, queue, id, cmd->state);
      lc_replyack(client, opcode, address, queue, packetid, id);
      break;
    }

    case OPCODE_EMPTYQUEUE: {
      lc_expectlen(0);
      lowcom_debug("CMD emptyqueue");
      m_emptyqueue(address, queue, id);
      lc_replyack(client, opcode, address, queue, packetid, id);
      break;
    }
    case OPCODE_SAVEQUEUE: {
      lc_expectlen(0);
      lowcom_debug("CMD savequeue");
      m_savequeue(address, queue, id);
      lc_replyack(client, opcode, address, queue, packetid, id);
      break;
    }
    case OPCODE_LOADQUEUE: {
      lc_expectlen(0);
      lowcom_debug("CMD loadqueue");
      m_loadqueue(address, queue, id);
      lc_replyack(client, opcode, address, queue, packetid, id);
      break;
    }
    case OPCODE_ADDQUEUE: {
      break;
    }
    case OPCODE_COPYQUEUE: {
      lc_expectlen(sizeof(uint8_t));
      lowcom_debug("CMD copyqueue", data[0]);
      m_copyqueue(address, queue, id, data[0]);
      lc_replyack(client, opcode, address, queue, packetid, id);
      break;
    }
    case OPCODE_RUNQUEUE: {
      lc_expectlen(0);
      lowcom_debug("CMD runqueue");
      m_copyqueue(address, 0, id, queue);
      lc_replyack(client, opcode, address, queue, packetid, id);
      break;
    }
    case OPCODE_GETQUEUE: {
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
      lowcom_debug("RX hello");
      uint8_t reply[sizeof(lc_preamble) + sizeof(type_hello)] = {0};
      
      // Set preamble
      {
        lc_preamble * preamble = (lc_preamble *)(&reply[0]);
        lc_packpreamble(preamble, TYPE_HELLO);
      }
      
      // Set reply payload
      {
        type_hello * payload = (type_hello *)(&reply[sizeof(lc_preamble)]);
        strncpy(payload->product, PRODUCT, LEN_PRODUCT-1);
        strncpy(payload->model, MODEL, LEN_INFO-1);
        strncpy(payload->swbranch, SWBRANCH, LEN_INFO-1);
        payload->version = VERSION;
        strncpy(payload->hostname, config.service.hostname, LEN_HOSTNAME-1);
        payload->chipid = state.wifi.chipid;
        payload->std_enabled = true;    // TODO - set to correct value
        payload->crypto_enabled = true; // TODO - set to correct value
      }
      
      lowcom_client[client].initialized = true;
      lc_send(client, reply, sizeof(reply));
      return sizeof(lc_preamble);
    }
    case TYPE_GOODBYE: {
      lowcom_debug("RX goodbye");
      if (lowcom_client[client].lastwill != 0) {
        // Execute last will
        cmdq_copy(Q0, nextid(), queue_get(lowcom_client[client].lastwill));

        // Execute last will on slaves
        if (config.daisy.enabled && config.daisy.master && state.daisy.active) {
          for (uint8_t i = 1; i <= state.daisy.slaves; i++) {
            daisy_emptyqueue(i, 0, nextid());
            daisy_copyqueue(i, 0, nextid(), lowcom_client[client].lastwill);
          }
        }
        
        lowcom_client[client].lastwill = 0;
      }
      lc_send(client, data, len);
      yield();
      //lowcom_client[client].sock.stop();
      lowcom_client[client].active = false;
      return sizeof(lc_preamble);
    }
    case TYPE_PING: {
      lowcom_debug("RX ping");
      lowcom_client[client].last.ping = millis();
      return sizeof(lc_preamble);
    }
    case TYPE_STD: {
      lowcom_debug("RX std");
      
      // Ensure at lease full header in buffer
      size_t elen = sizeof(lc_preamble) + sizeof(lc_header);
      if (len < elen) return 0;

      // Capture header
      lc_header * header = (lc_header *)&preamble[1];

      // Ensure full packet in buffer
      elen += header->length;
      if (len < elen) return 0;

      // TODO - validate header
      lc_handlepacket(client, header->opcode, header->subcode, header->address, header->queue, header->packetid, (uint8_t *)&header[1], header->length);
      return elen;
    }
    case TYPE_CRYPTO: {
      lowcom_debug("RX crypto");
      
      // TODO - not supported yet
      return 1;
    }
    default: {
      // Unknown type.
      lowcom_debug("RX unknown type", preamble->type);
      return 1;
    }
  }
}

void lowcom_init() {
  lowcom_server.begin();
  lowcom_server.setNoDelay(true);

  for (size_t i = 0; i < LTC_SIZE; i++) {
    lowcom_client[i].active = false;
  }
}

void lowcom_loop(unsigned long now) {
  // Check all packets for rx
  for (size_t ci = 0; ci < LTC_SIZE; ci++) {
    if (lowcom_client[ci].active && lowcom_client[ci].sock.available()) {
      // Check limit
      if (lowcom_client[ci].Ilen == LTCB_ISIZE) {
        // Input buffer is full, drain here (bad data, could not parse)
        seterror(ESUB_LC, 0, ETYPE_IBUF, ci);
        lowcom_client[ci].Ilen = 0;
      }
      
      // Read in as much as possible
      size_t bytes = lowcom_client[ci].sock.read(&lowcom_client[ci].I[lowcom_client[ci].Ilen], LTCB_ISIZE - lowcom_client[ci].Ilen);
      lowcom_client[ci].Ilen += bytes;
    }
  }

  // Handle payload
  for (size_t ci = 0; ci < LTC_SIZE; ci++) {
    if (lowcom_client[ci].active && lowcom_client[ci].Ilen > 0) {
      uint8_t * B = lowcom_client[ci].I;
      size_t Blen = lowcom_client[ci].Ilen;

      // Check for valid SOF
      if (B[0] != L_MAGIC_1) {
        // Not valid, find first instance of magic, sync to that
        size_t index = 1;
        for (; index < Blen; index++) {
          if (B[index] == L_MAGIC_1) break;
        }

        if (index < lowcom_client[ci].Ilen) memmove(B, &B[index], Blen - index);
        lowcom_client[ci].Ilen -= index;
        continue;
      }

      // Ensure full preamble
      if (Blen < sizeof(lc_preamble)) continue;
      
      // Consume packet
      size_t consume = lc_handletype(ci, B, Blen);

      // Clear packet from buffer
      if (consume > 0) {
        memmove(B, &B[consume], Blen - consume);
        lowcom_client[ci].Ilen -= consume;
      }
    }
  }
}

void lowcom_update(unsigned long now) {
  // Accept new clients
  if (lowcom_server.hasClient()) {
    lowcom_debug("NEW client");
    
    size_t ci = 0;
    for (; ci < LTC_SIZE; ci++) {
      if (!lowcom_client[ci].active) {
        lowcom_debug("NEW slot", ci);
        
        memset(&lowcom_client[ci], 0, sizeof(lowcom_client[ci]));
        lowcom_client[ci].sock = lowcom_server.available();
        lowcom_client[ci].active = true;
        //lowcom_client[i].nonce = ?  // TODO
        lowcom_client[ci].last.ping = now;
        break;
      }
    }

    if (ci == LTC_SIZE) {
      // No open slots
      lowcom_debug("ERR no client slots available");
      //lowcom_server.available().stop();
    }
  }

  // Handle client socket close
  for (size_t ci = 0; ci < LTC_SIZE; ci++) {
    if (lowcom_client[ci].active && (timesince(lowcom_client[ci].last.ping, millis()) > LTO_PING || !lowcom_client[ci].sock.connected())) {
      // Socket has timed out
      lowcom_debug("KILL client disconnect", ci);
      lc_preamble goodbye = {0};
      lc_packpreamble(&goodbye, TYPE_GOODBYE);
      lc_handletype(ci, (uint8_t *)&goodbye, sizeof(lc_preamble));
    }
  }

  // Ping clients
  if (timesince(sketch.service.lowcom.last.ping, now) > LTO_PING) {
    sketch.service.lowcom.last.ping = now;
    state.service.lowcom.clients = 0;

    for (size_t i = 0; i < LTC_SIZE; i++) {
      if (lowcom_client[i].active) {
        lowcom_debug("PING client", i);
        
        lc_preamble ping = {0};
        lc_packpreamble(&ping, TYPE_PING);
        if (lowcom_client[i].sock.availableForWrite() >= sizeof(lc_preamble)) {
          lowcom_debug("PING client write", i);
          lowcom_client[i].sock.write((uint8_t *)&ping, sizeof(lc_preamble));
        }
        state.service.lowcom.clients += 1;
      }
    }
  }
}

