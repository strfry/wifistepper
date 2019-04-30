#include <Arduino.h>
#include <Wire.h>
//#include <climits>

#include "wifistepper.h"
#include "ecc508a.h"
#include "sha256.h"

#define ECC_ADDR        (0x60)
#define ECC_PIN_SDA     (2)
#define ECC_PIN_SCL     (5)
#define ECC_QMAX        (1024)

#define ECMD_READCFG          (0x01)
#define ECMD_CHECKCFG         (0x02)
#define ECMD_RAND             (0x03)

#define ECMD_HMAC_START       (0x11)
#define ECMD_HMAC_UPDATE      (0x12)
#define ECMD_HMAC_ENDCMD      (0x13)
//#define ECMD_HMAC_ENDREPLY    (0x14)
#define ECMD_SET_NONCE        (0x31)
#define ECMD_SET_GENDIG       (0x32)
#define ECMD_SET_WRITEENC     (0x33)
#define ECMD_PROV_LOCK        (0x41)
#define ECMD_PROV_WRITECFG    (0x42)
#define ECMD_PROV_WRITEDATA   (0x43)

#define RXLEN_OK              (0x04)
#define RXLEN_READCFG         (0x23)
#define RXLEN_RAND            (0x23)
#define RXLEN_HMAC            (0x23)

#define WAIT_CHECK            (0), (0)
#define WAIT_READ             (1), (2)
#define WAIT_WRITE            (7), (26)
#define WAIT_LOCK             (8), (32)
#define WAIT_INFO             (1), (2)
#define WAIT_RAND             (1), (23)
#define WAIT_NONCE            (1), (7)
#define WAIT_GENDIG           (5), (11)
#define WAIT_HMAC             (7), (9)


uint8_t ecc_Q[ECC_QMAX] = {0};
size_t ecc_Qlen = 0;

uint8_t ecc_configzone[128] = {0};
uint8_t ecc_configzone_read = 0;


void print_config(ecc_configzone_t * cfg) {
  Serial.println("ECC Config zone:");
  Serial.print("SN 0: 0x"); Serial.println(cfg->sn_0, HEX);
  Serial.print("SN 1: 0x"); Serial.println(cfg->sn_1, HEX);
  Serial.print("SN 2: 0x"); Serial.println(cfg->sn_2, HEX);
  Serial.print("SN 3: 0x"); Serial.println(cfg->sn_3, HEX);
  Serial.print("RevNum 0: 0x"); Serial.println(cfg->revnum_0, HEX);
  Serial.print("RevNum 1: 0x"); Serial.println(cfg->revnum_1, HEX);
  Serial.print("RevNum 2: 0x"); Serial.println(cfg->revnum_2, HEX);
  Serial.print("RevNum 3: 0x"); Serial.println(cfg->revnum_3, HEX);
  Serial.print("SN 4: 0x"); Serial.println(cfg->sn_4, HEX);
  Serial.print("SN 5: 0x"); Serial.println(cfg->sn_5, HEX);
  Serial.print("SN 6: 0x"); Serial.println(cfg->sn_6, HEX);
  Serial.print("SN 7: 0x"); Serial.println(cfg->sn_7, HEX);
  Serial.print("SN 8: 0x"); Serial.println(cfg->sn_8, HEX);
  Serial.print("I2C_Enable: 0b"); Serial.println(cfg->i2c_enable, BIN);
  Serial.print("I2C_Address: 0x"); Serial.println(cfg->i2c_address, HEX);
  Serial.print("OTPmode: 0x"); Serial.println(cfg->otpmode, HEX);
  Serial.print("ChipMode.SelectorMode: 0b"); Serial.println(cfg->chipmode.selectormode, BIN);
  Serial.print("ChipMode.TTLenable: 0b"); Serial.println(cfg->chipmode.ttlenable, BIN);
  Serial.print("ChipMode.Watchdog: 0b"); Serial.println(cfg->chipmode.watchdog, BIN);
  for (size_t i = 0; i < 16; i++) {
    Serial.print("SlotConfig["); Serial.print(i);
    Serial.print("]: 0x"); Serial.print(cfg->slotconfig[i].bytes[0], HEX);
    Serial.print(", 0x"); Serial.println(cfg->slotconfig[i].bytes[1], HEX);
    Serial.print("  ReadKey: "); Serial.println(cfg->slotconfig[i].bits.readkey);
    Serial.print("  NoMac: 0b"); Serial.println(cfg->slotconfig[i].bits.nomac, BIN);
    Serial.print("  LimitedUse: 0b"); Serial.println(cfg->slotconfig[i].bits.limiteduse, BIN);
    Serial.print("  EncryptRead: 0b"); Serial.println(cfg->slotconfig[i].bits.encryptread, BIN);
    Serial.print("  IsSecret: 0b"); Serial.println(cfg->slotconfig[i].bits.issecret, BIN);
    Serial.print("  WriteKey: "); Serial.println(cfg->slotconfig[i].bits.writekey);
    Serial.print("  WriteConfig: 0b"); Serial.println(cfg->slotconfig[i].bits.writeconfig, BIN);
  }
  //Serial.print("Counter 0 (reverse endian!): "); Serial.println(cfg->counter[0]);
  //Serial.print("Counter 1 (reverse endian!): "); Serial.println(cfg->counter[1]);
  Serial.println("LastKeyUse:");
  for (size_t i = 0; i < 16; i++) {
    Serial.print(" 0x");
    Serial.print(cfg->lastkeyuse[i], HEX);
    if (((i+1) % 4) == 0) Serial.println();
  }
  Serial.println();
  Serial.print("UserExtra: 0x"); Serial.println(cfg->userextra, HEX);
  Serial.print("Selector: 0x"); Serial.println(cfg->selector, HEX);
  Serial.print("** LockValue: 0x"); Serial.println(cfg->lockvalue, HEX);
  Serial.print("** LockConfig: 0x"); Serial.println(cfg->lockconfig, HEX);
  Serial.print("SlotLocked 0: 0b"); Serial.println(cfg->slotlocked.bytes[0], BIN);
  Serial.print("SlotLocked 1: 0b"); Serial.println(cfg->slotlocked.bytes[1], BIN);
  for (size_t i = 0; i < 4; i++) {
    Serial.print("X509format["); Serial.print(i); Serial.print("]: 0x"); Serial.println(cfg->x509format[i], HEX);
  }
  for (size_t i = 0; i < 16; i++) {
    Serial.print("KeyConfig["); Serial.print(i);
    Serial.print("]: 0x"); Serial.print(cfg->keyconfig[i].bytes[0], HEX);
    Serial.print(", 0x"); Serial.println(cfg->keyconfig[i].bytes[1], HEX);
    Serial.print("  Private: 0b"); Serial.println(cfg->keyconfig[i].bits.priv, BIN);
    Serial.print("  PubInfo: 0b"); Serial.println(cfg->keyconfig[i].bits.pubinfo, BIN);
    Serial.print("  KeyType: 0b"); Serial.println(cfg->keyconfig[i].bits.keytype, BIN);
    Serial.print("  Lockable: 0b"); Serial.println(cfg->keyconfig[i].bits.lockable, BIN);
    Serial.print("  ReqRandom: 0b"); Serial.println(cfg->keyconfig[i].bits.reqrandom, BIN);
    Serial.print("  ReqAuth: 0b"); Serial.println(cfg->keyconfig[i].bits.reqauth, BIN);
    Serial.print("  AuthKey: "); Serial.println(cfg->keyconfig[i].bits.authkey);
    Serial.print("  IntrusionDisable: 0b"); Serial.println(cfg->keyconfig[i].bits.intrusiondisable, BIN);
    Serial.print("  X509id: "); Serial.println(cfg->keyconfig[i].bits.x509id);
  }
}

void print_packet(const char * name, uint8_t * p, size_t len) {
  Serial.print("Packet ");
  Serial.print(name);
  Serial.println(":");
  for (size_t i = 0; i < len; i++) {
    Serial.print("(");
    Serial.print(i);
    Serial.print(") = ");
    Serial.println(p[i], HEX);
  }
}

bool _ecc_check(uint8_t * data, size_t len);
void print_check(uint8_t * p, size_t len) {
  Serial.print("LENGTH=");
  Serial.println(len);
  Serial.print("CRC=");
  Serial.println(_ecc_check(p, len));
}

void print_sha(const char * name, uint8_t * sha) {
  Serial.print(name); Serial.print(": (sha256) ");
  for (size_t i = 0; i < 32; i++) Serial.printf("%02x", sha[i]);
  Serial.println();
}

uint16_t _ecc_crc16(uint8_t * data, uint8_t len, uint8_t * crc) {
  uint8_t counter;
  uint16_t crc_register = 0;
  uint16_t polynom = 0x8005;
  uint8_t shift_register;
  uint8_t data_bit, crc_bit;
  for (counter = 0; counter < len; counter++) {
    for (shift_register = 0x01; shift_register > 0x00; shift_register <<= 1) {
      data_bit = (data[counter] & shift_register) ? 1 : 0;
      crc_bit = crc_register >> 15;
      crc_register <<= 1;
      if (data_bit != crc_bit)
        crc_register ^= polynom;
    }
  }
  crc[0] = (uint8_t) (crc_register & 0x00FF);
  crc[1] = (uint8_t) (crc_register >> 8);
}

void _ecc_pack(uint8_t * data, size_t len) {
  if (len == 0 || data == NULL) return;
  data[0] = (uint8_t)len;
  _ecc_crc16(data, len-2, &data[len-2]);
}

bool _ecc_check(uint8_t * data, size_t len) {
  if (len == 0 || data == NULL) return false;
  uint8_t crc[2] = {0, 0};
  _ecc_crc16(data, len-2, crc);
  return data[len-2] == crc[0] && data[len-1] == crc[1];
}

bool _ecc_checkok(uint8_t * data, size_t len) {
  return len == 4 && _ecc_check(data, len) && data[1] == 0x0;
}

void * _ecc_alloc(uint8_t cmd, uint8_t wait_typ, uint8_t wait_max, uint8_t rxlen, uint8_t datalen) {
  if ((ECC_QMAX - ecc_Qlen) < datalen) {
    // Not enough memory in queue
    // TODO - set error
    Serial.println("ERROR: Not enough alloc mem");
    return NULL;
  }

  ecc_cmd_t * C = (ecc_cmd_t *)&ecc_Q[ecc_Qlen];
  *C = {.cmd = cmd, .wait_typ = wait_typ, .wait_max = wait_max, .rxlen = rxlen, .datalen = datalen, .started = 0};
  ecc_Qlen += sizeof(ecc_cmd_t) + datalen;
  return &C[1];
}

size_t _ecc_read(uint8_t * data, size_t len) {
  size_t r = 0;
  Wire.requestFrom(ECC_ADDR, len);
  while(Wire.available()) data[r++] = Wire.read();
  return r;
}

/*
size_t _ecc_waitread(uint8_t * data, size_t len, unsigned int typ_us, unsigned long max_ms) {
  unsigned long start = millis();
  delayMicroseconds(typ_us);
  size_t rlen = 0;
  do {
    rlen = _ecc_read(data, len);
  } while (rlen == 0 && (millis() - start) < max_ms);
  return rlen;
}
*/

void _ecc_write(uint8_t mode, uint8_t * data, size_t len) {
  Wire.beginTransmission(ECC_ADDR);
  Wire.write(mode);
  if (len > 0)
    Wire.write(data, len);
  Wire.endTransmission();
}

void _ecc_writepacket(uint8_t * data, size_t len) {
  _ecc_pack(data, len);
  _ecc_write(0x0, NULL, 0);
  _ecc_write(0x3, data, len);
}

void ecc_init() {
  Wire.begin(ECC_PIN_SDA, ECC_PIN_SCL);
}

void ecc_loop(unsigned long now) {
  // Check if queue has commands
  if (ecc_Qlen == 0) return;

  ecc_cmd_t * C = (ecc_cmd_t *)&ecc_Q[0];
  uint8_t * data = (uint8_t *)&C[1];
  
  // Check if receive
  now = millis();
  if (C->started != 0 && timesince(C->started, now) > C->wait_typ) {
    size_t consume = sizeof(ecc_cmd_t) + C->datalen;

    uint8_t reply[C->rxlen];
    size_t bytes = _ecc_read(reply, C->rxlen);
    
    if (bytes == 0) {
      // No bytes read, check if expired
      if (timesince(C->started, now) > C->wait_max) {
        // Timeout waiting for reply
        // TODO - error
        // TODO - reset i2c bus
        Serial.println("ERROR: timeout waiting for reply");
        
      } else {
        // More time to wait for reply
        consume = 0;
      }
      
    } else if (bytes == C->rxlen) {
      // All reply bytes received
      if (_ecc_check(reply, bytes)) {
        // Checksum passed
        switch (C->cmd) {
          case ECMD_RAND: {
            print_packet("Random REPLY", reply, C->rxlen);
            print_check(reply, C->rxlen);
            break;
          }
          case ECMD_READCFG: {
            print_packet("ReadConfig REPLY", reply, C->rxlen);
            print_check(reply, C->rxlen);
            size_t off = data[0] * 0x20;
            memcpy(&ecc_configzone[off], &reply[1], 32);
            ecc_configzone_read += 1;
            break;
          }

          case ECMD_HMAC_START:
          case ECMD_HMAC_UPDATE: {
            print_packet("Hmac Start/Update REPLY", reply, C->rxlen);
            print_check(reply, C->rxlen);
            if (!_ecc_checkok(reply, C->rxlen)) {
              // TODO - error
              Serial.println("ERROR: Bad reply packet");
            }
            break;
          }
          case ECMD_HMAC_ENDCMD: {
            print_packet("Hmac End REPLY", reply, C->rxlen);
            print_check(reply, C->rxlen);
            print_sha("Hmac Result", &reply[1]);
            break;
          }

          case ECMD_SET_NONCE:
          case ECMD_SET_GENDIG: {
            print_packet("Set Nonce/GenDig REPLY", reply, C->rxlen);
            print_check(reply, C->rxlen);
            if (!_ecc_checkok(reply, C->rxlen)) {
              // TODO - error
              Serial.println("ERROR: Bad reply packet");
            }
            break;
          }

          case ECMD_SET_WRITEENC: {
            print_packet("Set WriteEnc REPLY", reply, C->rxlen);
            print_check(reply, C->rxlen);
            if (!_ecc_checkok(reply, C->rxlen)) {
              // TODO - error
              Serial.println("ERROR: Bad reply packet");
            }
            // TODO set global flag and restart
            break;
          }

          case ECMD_PROV_LOCK:
          case ECMD_PROV_WRITECFG:
          case ECMD_PROV_WRITEDATA: {
            print_packet("Provision Lock/WriteConfig/WriteData REPLY", reply, C->rxlen);
            print_check(reply, C->rxlen);
            if (!_ecc_checkok(reply, C->rxlen)) {
              // TODO - error
              Serial.println("ERROR: Bad reply packet");
            }
            break;
          }
        }

      } else {
        // Bad checksum
        // TODO - error (retransmit?)
        print_packet("ERROR: bad checksum for REPLY", reply, C->rxlen);
      }
      
    } else {
      // Bad number of bytes received, error condition!
      // TODO - error
      // TODO - reset i2c bus
      Serial.println("ERROR: bad number of bytes received.");
    }

    if (consume > 0) {
      memmove(&ecc_Q[0], &ecc_Q[consume], ecc_Qlen - consume);
      ecc_Qlen -= consume;
    }
  }

  // Check if transmit
  if (C->started == 0 && now != 0) {
    size_t consume = 0;
    
    switch (C->cmd) {
      case ECMD_RAND: {
        uint8_t rnd[] = {0, 0x1B, 0x0, 0x0, 0x0, 0, 0};
        _ecc_writepacket(rnd, sizeof(rnd));
        break;
      }
      case ECMD_READCFG: {
        uint8_t readcfg[] = {0, 0x02, 0x80, data[0] << 3, 0x0, 0, 0};
        _ecc_writepacket(readcfg, sizeof(readcfg));
        break;
      }
      case ECMD_CHECKCFG: {
        for (size_t i = 0; i < 0x7F; i++) {
          Serial.print("CONFIG (0x");
          Serial.print(i, HEX);
          Serial.print("): 0x");
          Serial.println(ecc_configzone[i], HEX);
        }
        print_config((ecc_configzone_t *)ecc_configzone);
        consume = sizeof(ecc_cmd_t);
        break;
      }

      case ECMD_HMAC_START: {
        uint8_t start[] = {0, 0x47, 0x04, 0x02, 0x00, 0, 0};
        _ecc_writepacket(start, sizeof(start));
        break;
      }
      case ECMD_HMAC_UPDATE: {
        uint8_t update[81] = {0};
        update[1] = 0x47;
        update[2] = 0x01;
        update[3] = 64;
        update[4] = 0x00;
        memcpy(&update[5], data, 64);
        print_packet("Hmac Update SEND", update, sizeof(update));
        _ecc_writepacket(update, sizeof(update));
        break;
      }
      case ECMD_HMAC_ENDCMD: {
        uint8_t end[64+7] = {0};
        end[1] = 0x47;
        end[2] = 0x05;
        end[3] = C->datalen;
        end[4] = 0x0;
        memcpy(&end[5], data, C->datalen);
        print_packet("Hmac End SEND", end, 7 + C->datalen);
        _ecc_writepacket(end, 7 + C->datalen);
        break;
      }

      case ECMD_SET_NONCE: {
        uint8_t nonce[39] = {0};
        nonce[1] = 0x16;
        nonce[2] = 0x03;
        nonce[3] = 0x00;
        nonce[4] = 0x00;
        memset(&nonce[5], 0, 32);
        print_packet("Set Nonce SEND", nonce, sizeof(nonce));
        _ecc_writepacket(nonce, sizeof(nonce));
        break;
      }
      case ECMD_SET_GENDIG: {
        uint8_t gendig[] = {0, 0x15, 0x02, 0x01, 0x00, 0, 0};
        print_packet("Set GenDig SEND", gendig, sizeof(gendig));
        _ecc_writepacket(gendig, sizeof(gendig));
        break;
      }
      case ECMD_SET_WRITEENC: {
        uint8_t writeenc[71] = {0};
        writeenc[1] = 0x12;
        writeenc[2] = 0x82;
        writeenc[3] = 0x10;
        writeenc[4] = 0x00;
        memcpy(&writeenc[5], &data[0], 64);
        print_packet("Set WriteEnc SEND", writeenc, sizeof(writeenc));
        _ecc_writepacket(writeenc, sizeof(writeenc));
        break;
      }

      case ECMD_PROV_LOCK: {
        uint8_t lock[] = {0, 0x17, 0x80 | data[0], 0x0, 0x0, 0, 0};
        _ecc_writepacket(lock, sizeof(lock));
        break;
      }
      case ECMD_PROV_WRITECFG: {
        uint8_t writecfg[] = {0, 0x12, 0x00, data[0], 0x0, data[1], data[2], data[3], data[4], 0, 0};
        print_packet("Provision WriteConfig SEND", writecfg, sizeof(writecfg));
        _ecc_writepacket(writecfg, sizeof(writecfg));
        break;
      }
      case ECMD_PROV_WRITEDATA: {
        uint8_t writedata[39] = {0};
        writedata[1] = 0x12;
        writedata[2] = 0x82;
        writedata[3] = data[0] << 3;
        writedata[4] = 0x0;
        memcpy(&writedata[5], &data[1], 32);
        print_packet("Provision WriteData SEND", writedata, sizeof(writedata));
        _ecc_writepacket(writedata, sizeof(writedata));
        break;
      }
    }

    C->started = millis();
    if (consume > 0) {
      memmove(&ecc_Q[0], &ecc_Q[consume], ecc_Qlen - consume);
      ecc_Qlen -= consume;
    }
  }
}

void ecc_update(unsigned long now) {
  
}

#if 0
void setup() {
  Serial.begin(115200);
  Serial.println();
  
  ecc_init();

  /*
  // Write cfg (slotconfig + keyconfig)
  uint8_t * wc1 = (uint8_t *)_ecc_alloc(ECMD_PROV_WRITECFG, WAIT_WRITE, RXLEN_OK, 5);
  wc1[0] = 0x05; wc1[1] = 0x00; wc1[2] = 0x00; wc1[3] = 0x82; wc1[4] = 0x20;
  uint8_t * wc2 = (uint8_t *)_ecc_alloc(ECMD_PROV_WRITECFG, WAIT_WRITE, RXLEN_OK, 5);
  wc2[0] = 0x06; wc2[1] = 0x83; wc2[2] = 0x61; wc2[3] = 0x00; wc2[4] = 0x00;
  uint8_t * wc3 = (uint8_t *)_ecc_alloc(ECMD_PROV_WRITECFG, WAIT_WRITE, RXLEN_OK, 5);
  wc3[0] = 0x18; wc3[1] = 0x1c; wc3[2] = 0x00; wc3[3] = 0x1c; wc3[4] = 0x00;
  uint8_t * wc4 = (uint8_t *)_ecc_alloc(ECMD_PROV_WRITECFG, WAIT_WRITE, RXLEN_OK, 5);
  wc4[0] = 0x19; wc4[1] = 0x1c; wc4[2] = 0x00; wc4[3] = 0x1c; wc4[4] = 0x00;

  // Lock cfg zone
  uint8_t * l1 = (uint8_t *)_ecc_alloc(ECMD_PROV_LOCK, WAIT_LOCK, RXLEN_OK, 1);
  l1[0] = 0;
  */

  /*
  // Write data slots
  uint8_t * wd1 = (uint8_t *)_ecc_alloc(ECMD_PROV_WRITEDATA, WAIT_WRITE, RXLEN_OK, 33);
  wd1[0] = 0; memset(&wd1[1], 0, 32); memcpy(&wd1[1], "WSX100 1", 8);
  
  Sha256.init();
  Sha256.write("master", 6);
  uint8_t * master = Sha256.result();
  uint8_t * wd2 = (uint8_t *)_ecc_alloc(ECMD_PROV_WRITEDATA, WAIT_WRITE, RXLEN_OK, 33);
  wd2[0] = 1; memcpy(&wd2[1], master, 32);

  Sha256.init();
  Sha256.write("password", 8);
  uint8_t * password = Sha256.result();
  uint8_t * wd3 = (uint8_t *)_ecc_alloc(ECMD_PROV_WRITEDATA, WAIT_WRITE, RXLEN_OK, 33);
  wd3[0] = 2; memcpy(&wd3[1], password, 32);

  // Lock data zone
  uint8_t * l2 = (uint8_t *)_ecc_alloc(ECMD_PROV_LOCK, WAIT_LOCK, RXLEN_OK, 1);
  l2[0] = 1;
  */

  
  // Read config
  uint8_t * r1 = (uint8_t *)_ecc_alloc(ECMD_READCFG, WAIT_READ, RXLEN_READCFG, 1);
  uint8_t * r2 = (uint8_t *)_ecc_alloc(ECMD_READCFG, WAIT_READ, RXLEN_READCFG, 1);
  uint8_t * r3 = (uint8_t *)_ecc_alloc(ECMD_READCFG, WAIT_READ, RXLEN_READCFG, 1);
  uint8_t * r4 = (uint8_t *)_ecc_alloc(ECMD_READCFG, WAIT_READ, RXLEN_READCFG, 1);
  *r1 = 0; *r2 = 1; *r3 = 2; *r4 = 3;
  _ecc_alloc(ECMD_CHECKCFG, WAIT_CHECK, 0, 0);
  

  /*
  // Get Random number
  _ecc_alloc(ECMD_RAND, WAIT_RAND, RXLEN_RAND, 0);
  */

  /*
  // Reset password
  char * master = "master";
  char * newpass = "newpass1";
  ecc_configzone_t * cfg = (ecc_configzone_t *)ecc_configzone;
  cfg->sn_0 = 0x01;
  cfg->sn_1 = 0x23;
  cfg->sn_8 = 0xee;
  {
    // Set up tempkey register
    _ecc_alloc(ECMD_SET_NONCE, WAIT_NONCE, RXLEN_OK, 0);
    _ecc_alloc(ECMD_SET_GENDIG, WAIT_GENDIG, RXLEN_OK, 0);

    // Hash master password and new password
    uint8_t master_sha[32];
    uint8_t newpass_sha[32];
    Sha256.init(); Sha256.write(master, strlen(master)); memcpy(master_sha, Sha256.result(), 32);
    Sha256.init(); Sha256.write(newpass, strlen(newpass)); memcpy(newpass_sha, Sha256.result(), 32);
    print_sha("Master", master_sha);
    print_sha("New Password", newpass_sha);

    // Compute the value of tempkey register locally
    uint8_t tempkey[32];
    Sha256.init();
    Sha256.write(master_sha, 32);
    Sha256.write((uint8_t)0x15); Sha256.write((uint8_t)0x02); Sha256.write((uint8_t)0x01); Sha256.write((uint8_t)0x00);
    Sha256.write((uint8_t)cfg->sn_8); Sha256.write((uint8_t)cfg->sn_0); Sha256.write((uint8_t)cfg->sn_1);
    for (size_t i = 0; i < (25 + 32); i++) Sha256.write((uint8_t)0x00);
    memcpy(tempkey, Sha256.result(), 32);
    print_sha("TempKey", tempkey);

    // Compute mac
    Sha256.init();
    Sha256.write(tempkey, 32);
    Sha256.write((uint8_t)0x12); Sha256.write((uint8_t)0x82); Sha256.write((uint8_t)0x10); Sha256.write((uint8_t)0x00);
    Sha256.write((uint8_t)cfg->sn_8); Sha256.write((uint8_t)cfg->sn_0); Sha256.write((uint8_t)cfg->sn_1);
    for (size_t i = 0; i < 25; i++) Sha256.write((uint8_t)0x00);
    Sha256.write(newpass_sha, 32);

    // Create command and write data contents
    uint8_t * sm = (uint8_t *)_ecc_alloc(ECMD_SET_WRITEENC, WAIT_WRITE, RXLEN_OK, 64);
    for (size_t i = 0; i < 32; i++) sm[i] = newpass_sha[i] ^ tempkey[i];
    memcpy(&sm[32], Sha256.result(), 32);
    print_sha("Mac", &sm[32]);
  }
  */

  // HMAC
  uint8_t data[] = {
    0xeb,0x77,0xdf,0x4f,0x56,0xec,0xc4,0x25,0x82,0x8c,0x2b,0x79,0xce,0x6c,0x39,0xca,0x50,0x37,0xe0,0x9d,0xd4,0xb3,0xd3,0xfe,0x80,0xa8,0x46,0x2e,0xfe,0x0f,0xb3,0xe9,
    0xeb,0x77,0xdf,0x4f,0x56,0xec,0xc4,0x25,0x82,0x8c,0x2b,0x79,0xce,0x6c,0x39,0xca,0x50,0x37,0xe0,0x9d,0xd4,0xb3,0xd3,0xfe,0x80,0xa8,0x46,0x2e,0xfe,0x0f,0xb3,0xe9
  };
  {
    _ecc_alloc(ECMD_HMAC_START, WAIT_HMAC, RXLEN_OK, 0);

    uint8_t * hu1 = (uint8_t *)_ecc_alloc(ECMD_HMAC_UPDATE, WAIT_HMAC, RXLEN_OK, 64);
    memcpy(&hu1[0], data, 64);

    //uint8_t * he = (uint8_t *)_ecc_alloc(ECMD_HMAC_ENDCMD, WAIT_HMAC, RXLEN_HMAC, 3);
    //he[0] = 0x01; he[1] = 0x02; he[2] = 0x03;

    _ecc_alloc(ECMD_HMAC_ENDCMD, WAIT_HMAC, RXLEN_HMAC, 0);
  }
}

void loop() {
  unsigned long now = millis();

  delay(1);
  ecc_loop(now);
  delay(3);
  ecc_update(now);

  //Serial.print(".");
}
#endif

