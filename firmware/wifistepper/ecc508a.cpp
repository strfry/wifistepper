#include <Wire.h>

#include "ecc508a.h"

#define ECC_ADDR        (0x60)
#define ECC_PIN_SDA     (2)
#define ECC_PIN_SCL     (5)

uint16_t _ecc_crc16(uint8_t * data, uint8_t length) {
  uint8_t counter;
  uint16_t crc_register = 0;
  uint16_t polynom = 0x8005;
  uint8_t shift_register;
  uint8_t data_bit, crc_bit;
  for (counter = 0; counter < length; counter++) {
    for (shift_register = 0x01; shift_register > 0x00; shift_register <<= 1) {
      data_bit = (data[counter] & shift_register) ? 1 : 0;
      crc_bit = crc_register >> 15;
      crc_register <<= 1;
      if (data_bit != crc_bit)
        crc_register ^= polynom;
    }
  }
  //crc[0] = (uint8_t) (crc_register & 0x00FF);
  //crc[1] = (uint8_t) (crc_register >> 8);

  return crc_register;
}

void _ecc_write(uint8_t mode, uint8_t * data, size_t len) {
  Wire.beginTransmission(ECC_ADDR);
  Wire.write(mode);
  Wire.write(data, len);
  Wire.endTransmission();
}

void ecc_i2cinit() {
  Wire.begin(ECC_PIN_SDA, ECC_PIN_SCL);
}


