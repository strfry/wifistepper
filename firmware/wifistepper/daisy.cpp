#include <Arduino.h>

#include "wifistepper.h"

#define B_SIZE          (2048)
#define B_MAGIC1        (0xAB)
#define B_MAGIC2        (0x75)
#define B_HEADER        (7)

#define CMD_PING        (0x0)
#define CMD_SIZE        (1)

uint8_t B[B_SIZE] = {0};
volatile size_t Blen = 0;

uint8_t daisy_checksum8(uint8_t * data, size_t length) {
  unsigned int sum = 0;
  for (size_t i = 0; i < length; i++) {
    sum += data[i];
  }
  return (uint8_t)sum;
}

void daisy_init() {
  Serial.begin(servicecfg.daisycfg.baudrate);
  
}

void daisy_loop() {
  Blen += Serial.read((char *)&B[Blen], B_SIZE - Blen);

  // Parse packets
  while (Blen > 0) {
    // Packets are formatted (in bytes bytes)
    // | MAGIC_1 | MAGIC_2 | CHECKSUM | ID[0] | ID[1] | OPCODE | LEN | ... data (num bytes = LEN) ... |
    // Header is first 7 bytes, payload is the following LEN bytes
    // Checksum is calculated using daisy_checksum8() on all bytes after CHECKSUM (LEN + 4 bytes)
    
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
    bool isvalid = B[1] == B_MAGIC2 && B[5] < CMD_SIZE;
    if (isvalid && !servicecfg.daisycfg.master) {
      // We're not master, check to see if ID is for us
      isvalid = B[3] == servicecfg.daisycfg.id[0] && B[4] == servicecfg.daisycfg.id[1];
    }
    if (!isvalid) {
      // Not a valid header, shift out one byte and continue
      if (!servicecfg.daisycfg.master) Serial.write(B, 1);
      memmove(B, &B[1], Blen - 1);
      continue;
    }

    // Make sure we have whole packet
    uint8_t len = B[6];
    if (Blen < (B_HEADER + len)) return;

    // Validate checksum
    uint8_t cksum = daisy_checksum8(&B[3], len+1);
    if (cksum != B[2]) {
      // Bad checksum, shift out one byte and continue
      if (!servicecfg.daisycfg.master) Serial.write(B, 1);
      memmove(B, &B[1], Blen - 1);
      continue;
    }

    // Consume packet

    memmove(B, &B[B_HEADER + len], Blen - (B_HEADER + len));
    Blen -= B_HEADER + len;
  }
}


