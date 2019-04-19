#include <Arduino.h>
#include <Wire.h>

#include "wifistepper.h"

#define ECC_ADDR        (0x60)
#define ECC_PIN_SDA     (2)
#define ECC_PIN_SCL     (5)

#define WAIT_RAND_TYP   (1000)

#define WAIT_RAND_MAX   (23)

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

size_t _ecc_read(uint8_t * data, size_t len) {
  size_t r = 0;
  Wire.requestFrom(ECC_ADDR, len);
  while(Wire.available())
    data[r++] = Wire.read();
  return r;
}

size_t _ecc_waitread(uint8_t * data, size_t len, unsigned int typ_us, unsigned long max_ms) {
  unsigned long start = millis();
  delayMicroseconds(typ_us);
  size_t rlen = 0;
  do {
    rlen = _ecc_read(data, len);
  } while (rlen == 0 && (millis() - start) < max_ms);
  return rlen;
}

void _ecc_write(uint8_t mode, uint8_t * data, size_t len) {
  Wire.beginTransmission(ECC_ADDR);
  Wire.write(mode);
  if (len > 0)
    Wire.write(data, len);
  Wire.endTransmission();
}

void ecc_init() {
  Wire.begin(ECC_PIN_SDA, ECC_PIN_SCL);
}

void ecc_update(unsigned long now) {
  
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
  
  Serial.print("LENGTH=");
  Serial.println(len);
  Serial.print("CRC=");
  Serial.println(_ecc_check(p, len));
}

