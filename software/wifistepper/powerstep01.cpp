#include <SPI.h>

#include "powerstep01.h"
#include "powerstep01priv.h"

#define PS_PIN_RST      (15)
#define PS_PIN_CS       (4)

//#define PS_DEBUG

void _ps_xfer(uint8_t * data, size_t len) {
  #ifdef PS_DEBUG
  {
    Serial.print("SPI Write: ");
    for (size_t i = 0; i < len; i++) {
      Serial.print(data[i], BIN);
      Serial.print(", ");
    }
    Serial.println();
  }
  #endif
  
  for (size_t i = 0; i < len; i++) {
    digitalWrite(PS_PIN_CS, LOW);
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    data[i] = SPI.transfer(data[i]);
    SPI.endTransaction();
    digitalWrite(PS_PIN_CS, HIGH);

    // Per datasheet, must raise CS between bytes and hold for at least 625ns
    delayMicroseconds(1);
  }

  #ifdef PS_DEBUG
  {
    Serial.print("SPI Read: ");
    if (data[0] != 0) {
      Serial.print("XX----> ");
    }
    for (size_t i = 0; i < len; i++) {
      Serial.print(data[i], BIN);
      Serial.print(", ");
    }
    Serial.println();
  }
  #endif
}

#define ps_set16(b1, b2, v)       ({b1 = ((v) >> 8) & 0xFF; b2 = (v) & 0xFF;})
#define ps_get16(b1, b2)          ((uint16_t)((b1) << 8) | (b2))
#define ps_set24(b1, b2, b3, v)   ({b1 = ((v) >> 16) & 0xFF; b2 = ((v) >> 8) & 0xFF; b3 = (v) & 0xFF;})
//#define ps_get24(b1, b2, b3)

#define ps_setsplit16(n, v)       ps_set16(n ## _U, n ## _L, (v))
#define ps_getsplit16(n)          ps_get16(n ## _U, n ## _L)

#ifdef PS_DEBUG
void ps_xfer(const char * cmdname, uint8_t * data, size_t len) {
  Serial.print("SPI Cmd: ");
  Serial.println(cmdname);
  _ps_xfer(data, len);
}

void ps_print(const ps_status_reg * s) {
  Serial.println("StepperMotor Status:");
  Serial.print("Stall A: "); Serial.println(s->stall_a, BIN);
  Serial.print("Stall B: "); Serial.println(s->stall_b, BIN);
  Serial.print("OCD: "); Serial.println(s->ocd, BIN);
  Serial.print("Th Status: "); Serial.println(s->th_status, BIN);
  Serial.print("UVLO ADC: "); Serial.println(s->uvlo_adc, BIN);
  Serial.print("UVLO: "); Serial.println(s->uvlo, BIN);
  Serial.print("Step Clk: "); Serial.println(s->stck_mod, BIN);
  Serial.print("Cmd ERR: "); Serial.println(s->cmd_err, BIN);
  Serial.print("Motor Status: "); Serial.println(s->mot_status, BIN);
  Serial.print("Direction: "); Serial.println(s->dir, BIN);
  Serial.print("Sw Event: "); Serial.println(s->sw_evn, BIN);
  Serial.print("Sw F: "); Serial.println(s->sw_f, BIN);
  Serial.print("Busy: "); Serial.println(s->busy, BIN);
  Serial.print("HiZ: "); Serial.println(s->hiz, BIN);
  Serial.println();
}

void ps_print(const ps_stepmode_reg * m) {
  Serial.println("Step Mode:");
  Serial.print("Sync En: "); Serial.println(m->sync_en, BIN);
  Serial.print("Sync Sel: "); Serial.println(m->sync_sel, BIN);
  Serial.print("CM VM: "); Serial.println(m->cm_vm, BIN);
  Serial.print("Step Sel: "); Serial.println(m->step_sel, BIN);
  Serial.println();
}

void ps_print(const ps_fsspd_reg * r) {
  Serial.println("Full Speed:");
  Serial.print("Boost Mode: "); Serial.println(r->boost_mode, BIN);
  Serial.print("FullStep Speed: "); Serial.println(ps_getsplit16(r->fs_spd), BIN);
  Serial.println();
}

void ps_print(const ps_gatecfg1_reg * r) {
  Serial.println("Gate Cfg 1:");
  Serial.print("WD: "); Serial.println(r->wd_en, BIN);
  Serial.print("TBoost: "); Serial.println(r->tboost, BIN);
  Serial.print("Slew: "); Serial.println(r->slew, BIN);
  Serial.println();
}

void ps_print(const ps_config_reg * r) {
  Serial.println("Configuration:");
  Serial.print("V PWM Int: "); Serial.println(r->vm.f_pwm_int, BIN);
  Serial.print("V PWM Dec: "); Serial.println(r->vm.f_pwm_dec, BIN);
  Serial.print("V VS Comp: "); Serial.println(r->vm.en_vscomp, BIN);
  Serial.print("C Pred: "); Serial.println(r->cm.pred_en, BIN);
  Serial.print("C TSW: "); Serial.println(r->cm.tsw, BIN);
  Serial.print("C TQ Reg: "); Serial.println(r->cm.en_tqreg, BIN);
  Serial.print("- VCC: "); Serial.println(r->com.vccval, BIN);
  Serial.print("- UVLO: "); Serial.println(r->com.uvloval, BIN);
  Serial.print("- OC SD: "); Serial.println(r->com.oc_sd, BIN);
  Serial.print("- Switch Mode: "); Serial.println(r->com.sw_mode, BIN);
  Serial.print("- Clock Sel: "); Serial.println(r->com.clk_sel, BIN);
  Serial.println();
}

void ps_print(const ps_alarms_reg * r) {
  Serial.println("Alarms:");
  Serial.print("Overcurrent: "); Serial.println(r->overcurrent, BIN);
  Serial.print("Thermal Shutdown: "); Serial.println(r->thermal_shutdown, BIN);
  Serial.print("Thermal Warning: "); Serial.println(r->thermal_warning, BIN);
  Serial.print("Undervoltage: "); Serial.println(r->undervoltage, BIN);
  Serial.print("ADC Undervoltage: "); Serial.println(r->adc_undervoltage, BIN);
  Serial.print("Stall Detect: "); Serial.println(r->stall_detect, BIN);
  Serial.print("User Switch: "); Serial.println(r->user_switch, BIN);
  Serial.print("Command Error: "); Serial.println(r->command_error, BIN);
  Serial.println();
}

void ps_print(const char * name, uint16_t data) {
  Serial.print(name);
  Serial.print(": 0x");
  Serial.println(data, HEX);
  Serial.println();
}

void ps_print(const char * name, uint8_t data) {
  Serial.print(name);
  Serial.print(": 0x");
  Serial.println(data, HEX);
  Serial.println();
}
#else

#define ps_xfer(cmdname, data, len)   _ps_xfer(data, len)
#define ps_print(...)

#endif


void ps_spiinit() {
  SPI.begin();

  pinMode(PS_PIN_RST, OUTPUT);
  pinMode(PS_PIN_CS, OUTPUT);
  digitalWrite(PS_PIN_RST, HIGH);
  digitalWrite(PS_PIN_RST, LOW);
  digitalWrite(PS_PIN_RST, HIGH);
  digitalWrite(PS_PIN_CS, HIGH);
}

ps_status ps_getstatus(bool clear_errors) {
  uint8_t buf[3] = {clear_errors? CMD_GETSTATUS() : CMD_GETPARAM(PARAM_STATUS), 0, 0};
  ps_status_reg * reg = (ps_status_reg *)&buf[1];
  ps_xfer(clear_errors? "getstatus" : "getparam status", buf, 3);
  ps_print(reg);
  return (ps_status){
    .busy = (bool)reg->busy
  };
}

void ps_waitbusy(ps_waitcb waitf) {
  uint8_t buf[3] = {0, 0, 0};
  ps_status_reg * reg = (ps_status_reg *)&buf[1];
  
  while (true) {
    memset(buf, 0, sizeof(buf));
    buf[0] = CMD_GETPARAM(PARAM_STATUS);
    ps_xfer("getparam status", buf, 3);
    if (reg->busy) return;
    if (waitf != NULL) waitf();
  }
}

ps_stepmode ps_getstepmode() {
  uint8_t buf[2] = {CMD_GETPARAM(PARAM_STEPMODE), 0};
  ps_stepmode_reg * reg = (ps_stepmode_reg *)&buf[1];
  ps_xfer("getparam stepmode", buf, 2);
  ps_print(reg);
  return (ps_stepmode){
    .mode = (ps_mode)reg->cm_vm,
    .stepsize = (ps_stepsize)reg->step_sel,
    .sync_mode = (ps_sync)reg->sync_en,
    .sync_stepsize = (ps_stepsize)reg->sync_sel
  };
}

void ps_setmode(const ps_mode mode, const ps_stepsize stepsize) {
  uint8_t buf[2] = {0, 0};
  ps_stepmode_reg * reg = (ps_stepmode_reg *)&buf[1];
  buf[0] = CMD_GETPARAM(PARAM_STEPMODE);
  ps_xfer("getparam stepmode", buf, 2);
  reg->cm_vm = mode;
  reg->step_sel = stepsize;
  buf[0] = CMD_SETPARAM(PARAM_STEPMODE);
  ps_xfer("setparam stepmode", buf, 2);
}

void ps_setsync(const ps_sync sync, const ps_stepsize stepsize) {
  uint8_t buf[2] = {0, 0};
  ps_stepmode_reg * reg = (ps_stepmode_reg *)&buf[1];
  buf[0] = CMD_GETPARAM(PARAM_STEPMODE);
  ps_xfer("getparam stepmode", buf, 2);
  reg->sync_en = sync;
  reg->sync_sel = stepsize;
  buf[0] = CMD_SETPARAM(PARAM_STEPMODE);
  ps_xfer("setparam stepmode", buf, 2);
}

float ps_getmaxspeed() {
  uint8_t buf[3] = {CMD_GETPARAM(PARAM_MAXSPEED), 0, 0};
  ps_xfer("getparam maxspeed", buf, 3);
  uint16_t maxspeed = ps_get16(buf[1], buf[2]);
  ps_print("Max Speed", maxspeed);
  return ((float)(maxspeed & MAXSPEED_MASK))/MAXSPEED_COEFF;
}

void ps_setmaxspeed(float steps_per_second) {
  uint8_t buf[3] = {CMD_SETPARAM(PARAM_MAXSPEED), 0, 0};
  uint16_t maxspeed = min((int)round(steps_per_second * MAXSPEED_COEFF), MAXSPEED_MASK);
  ps_set16(buf[1], buf[2], maxspeed);
  ps_xfer("setparam maxspeed", buf, 3);
}

ps_fullspeed ps_getfullspeed() {
  uint8_t buf[3] = {CMD_GETPARAM(PARAM_FSSPD), 0, 0};
  ps_fsspd_reg * reg = (ps_fsspd_reg *)&buf[1];
  ps_xfer("getparam fsspd", buf, 3);
  ps_print(reg);
  return (ps_fullspeed){
    .steps_per_sec = ((float)(ps_getsplit16(reg->fs_spd) & FSSPD_MASK) + FSSPD_OFFSET)/FSSPD_COEFF,
    .boost_mode = (bool)reg->boost_mode
  };
}

void ps_setfullspeed(float steps_per_sec, bool boost_mode) {
  uint8_t buf[3] = {CMD_SETPARAM(PARAM_FSSPD), 0, 0};
  ps_fsspd_reg * reg = (ps_fsspd_reg *)&buf[1];
  uint16_t fs_spd = min((int)round(steps_per_sec * FSSPD_COEFF - FSSPD_OFFSET), FSSPD_MASK);
  ps_setsplit16(reg->fs_spd, fs_spd);
  reg->boost_mode = boost_mode? 0x1 : 0x0;
  ps_xfer("setparam fsspd", buf, 3);
}

float ps_getaccel() {
  uint8_t buf[3] = {CMD_GETPARAM(PARAM_ACC), 0, 0};
  ps_xfer("getparam acc", buf, 3);
  uint16_t acc = ps_get16(buf[1], buf[2]);
  ps_print("Acc", acc);
  return ((float)(acc & ACC_MASK))/ACC_COEFF;
}

void ps_setaccel(float steps_per_sec_2) {
  uint8_t buf[3] = {CMD_SETPARAM(PARAM_ACC), 0, 0};
  uint16_t acc = min((int)round(steps_per_sec_2 * ACC_COEFF), ACC_MASK);
  ps_set16(buf[1], buf[2], acc);
  ps_xfer("setparam acc", buf, 3);
}

float ps_getdecel() {
  uint8_t buf[3] = {CMD_GETPARAM(PARAM_DEC), 0, 0};
  ps_xfer("getparam dec", buf, 3);
  uint16_t dec = ps_get16(buf[1], buf[2]);
  ps_print("Dec", dec);
  return ((float)(dec & DEC_MASK)) / DEC_COEFF;
}

void ps_setdecel(float steps_per_sec_2) {
  uint8_t buf[3] = {CMD_SETPARAM(PARAM_DEC), 0, 0};
  uint16_t dec = min((int)round(steps_per_sec_2 * DEC_COEFF), DEC_MASK);
  ps_set16(buf[1], buf[2], dec);
  ps_xfer("setparam dec", buf, 3);
}

ps_slewrate ps_getslewrate() {
  uint8_t buf[3] = {CMD_GETPARAM(PARAM_GATECFG1), 0, 0};
  ps_gatecfg1_reg * reg = (ps_gatecfg1_reg *)&buf[1];
  ps_xfer("getparam gatecfg1", buf, 3);
  ps_print(reg);
  return (ps_slewrate)reg->slew;
}

void ps_setslewrate(ps_slewrate slew) {
  uint8_t buf[3] = {0, 0, 0};
  ps_gatecfg1_reg * reg = (ps_gatecfg1_reg *)&buf[1];
  buf[0] = CMD_GETPARAM(PARAM_GATECFG1);
  ps_xfer("getparam gatecfg1", buf, 3);
  reg->slew = slew;
  buf[0] = CMD_SETPARAM(PARAM_GATECFG1);
  ps_xfer("setparam gatecfg1", buf, 3);
}

ps_ocd ps_getocd() {
  uint8_t buf1[2] = {CMD_GETPARAM(PARAM_OCDTH), 0};
  uint8_t buf2[3] = {CMD_GETPARAM(PARAM_CONFIG), 0, 0};
  ps_config_reg * reg = (ps_config_reg *)&buf2[1];
  ps_xfer("getparam ocdth", buf1, 2);
  ps_xfer("getparam config", buf2, 3);
  ps_print("OCD Th", buf1[1]);
  ps_print("OCD En", reg->com.oc_sd);
  return (ps_ocd){
    .millivolts = ((float)(buf1[1] & OCDTH_MASK)) / OCDTH_COEFF,
    .shutdown = reg->com.oc_sd
  };
}

void ps_setocd(float millivolts, bool shutdown) {
  uint8_t buf1[2] = {CMD_SETPARAM(PARAM_OCDTH), 0};
  uint8_t buf2[3] = {0, 0, 0};
  ps_config_reg * reg = (ps_config_reg *)&buf2[1];
  buf1[1] = min((int)round(millivolts * OCDTH_COEFF), OCDTH_MASK);
  ps_xfer("setparam ocdth", buf1, 2);
  buf2[0] = CMD_GETPARAM(PARAM_CONFIG);
  ps_xfer("getparam config", buf2, 3);
  reg->com.oc_sd = shutdown? 0x1 : 0x0;
  buf2[0] = CMD_SETPARAM(PARAM_CONFIG);
  ps_xfer("setparam config", buf2, 3);
}

ps_clocksel ps_getclocksel() {
  uint8_t buf[3] = {CMD_GETPARAM(PARAM_CONFIG), 0, 0};
  ps_config_reg * reg = (ps_config_reg *)&buf[1];
  ps_xfer("getparam config", buf, 3);
  ps_print(reg);
  return (ps_clocksel)reg->com.clk_sel;
}

void ps_setclocksel(const ps_clocksel clock) {
  uint8_t buf[3] = {0, 0, 0};
  ps_config_reg * reg = (ps_config_reg *)&buf[1];
  buf[0] = CMD_GETPARAM(PARAM_CONFIG);
  ps_xfer("getparam config", buf, 3);
  reg->com.clk_sel = clock;
  buf[0] = CMD_SETPARAM(PARAM_CONFIG);
  ps_xfer("setparam config", buf, 3);
}

ps_swmode ps_getswmode() {
  uint8_t buf[3] = {CMD_GETPARAM(PARAM_CONFIG), 0, 0};
  ps_config_reg * reg = (ps_config_reg *)&buf[1];
  ps_xfer("getparam config", buf, 3);
  ps_print(reg);
  return (ps_swmode)reg->com.sw_mode;
}

void ps_setswmode(const ps_swmode swmode) {
  uint8_t buf[3] = {0, 0, 0};
  ps_config_reg * reg = (ps_config_reg *)&buf[1];
  buf[0] = CMD_GETPARAM(PARAM_CONFIG);
  ps_xfer("getparam config", buf, 3);
  reg->com.sw_mode = swmode;
  buf[0] = CMD_SETPARAM(PARAM_CONFIG);
  ps_xfer("setparam config", buf, 3);
}

void ps_vm_setpwmfreq(uint8_t div, uint8_t mul) {
  uint8_t buf[3] = {0, 0, 0};
  ps_config_reg * reg = (ps_config_reg *)&buf[1];
  buf[0] = CMD_GETPARAM(PARAM_CONFIG);
  ps_xfer("getparam config", buf, 3);
  reg->vm.f_pwm_int = div;
  reg->vm.f_pwm_dec = mul;
  buf[0] = CMD_SETPARAM(PARAM_CONFIG);
  ps_xfer("setparam config", buf, 3);
}

ps_alarms ps_getalarms() {
  uint8_t buf[2] = {CMD_GETPARAM(PARAM_ALARMEN), 0};
  ps_alarms_reg * reg = (ps_alarms_reg *)&buf[1];
  ps_xfer("getparam alarmen", buf, 2);
  ps_print(reg);
  return (ps_alarms){
    .command_error = reg->command_error,
    .overcurrent = reg->overcurrent,
    .undervoltage = reg->undervoltage,
    .thermal_shutdown = reg->thermal_shutdown,
    .user_switch = reg->user_switch,
    .thermal_warning = reg->thermal_warning,
    .stall_detect = reg->stall_detect,
    .adc_undervoltage = reg->adc_undervoltage
  };
}

void ps_setalarms(const ps_alarms * alarms) {
  ps_setalarms(alarms->command_error, alarms->overcurrent, alarms->undervoltage, alarms->thermal_shutdown, alarms->user_switch, alarms->thermal_warning, alarms->stall_detect, alarms->adc_undervoltage);
}

void ps_setalarms(bool command_error, bool overcurrent, bool undervoltage, bool thermal_shutdown, bool user_switch, bool thermal_warning, bool stall_detect, bool adc_undervoltage) {
  uint8_t buf[2] = {CMD_SETPARAM(PARAM_ALARMEN), 0};
  ps_alarms_reg * reg = (ps_alarms_reg *)&buf[1];
  reg->overcurrent = overcurrent? 0x1 : 0x0;
  reg->thermal_shutdown = thermal_shutdown? 0x1 : 0x0;
  reg->thermal_warning = thermal_warning? 0x1 : 0x0;
  reg->undervoltage = undervoltage? 0x1 : 0x0;
  reg->adc_undervoltage = adc_undervoltage? 0x1 : 0x0;
  reg->stall_detect = stall_detect? 0x1 : 0x0;
  reg->user_switch = user_switch? 0x1 : 0x0;
  reg->command_error = command_error? 0x1 : 0x0;
  ps_xfer("setparam alarmen", buf, 2);
}

bool ps_vm_getvoltcomp() {
  uint8_t buf[3] = {CMD_GETPARAM(PARAM_CONFIG), 0, 0};
  ps_config_reg * reg = (ps_config_reg *)&buf[1];
  ps_xfer("getparam config", buf, 3);
  return reg->vm.en_vscomp;
}

void ps_vm_setvoltcomp(bool voltage_compensation) {
  uint8_t buf[3] = {0, 0, 0};
  ps_config_reg * reg = (ps_config_reg *)&buf[1];
  buf[0] = CMD_GETPARAM(PARAM_CONFIG);
  ps_xfer("getparam config", buf, 3);
  reg->vm.en_vscomp = voltage_compensation? 0x1 : 0x0;
  buf[0] = CMD_SETPARAM(PARAM_CONFIG);
  ps_xfer("setparam config", buf, 3);
}

ps_vm_kvals ps_vm_getkvals() {
  ps_vm_kvals kvals = {0.0, 0.0, 0.0, 0.0};
  {
    uint8_t buf[2] = {CMD_GETPARAM(PARAM_KVALHOLD), 0};
    ps_xfer("getparam kvalhold", buf, 2);
    kvals.hold = ((float)buf[1]) * KVALS_COEFF;
  }
  {
    uint8_t buf[2] = {CMD_GETPARAM(PARAM_KVALRUN), 0};
    ps_xfer("getparam kvalrun", buf, 2);
    kvals.run = ((float)buf[1]) * KVALS_COEFF;
  }
  {
    uint8_t buf[2] = {CMD_GETPARAM(PARAM_KVALACC), 0};
    ps_xfer("getparam kvalacc", buf, 2);
    kvals.accel = ((float)buf[1]) * KVALS_COEFF;
  }
  {
    uint8_t buf[2] = {CMD_GETPARAM(PARAM_KVALDEC), 0};
    ps_xfer("getparam kvaldec", buf, 2);
    kvals.decel = ((float)buf[1]) * KVALS_COEFF;
  }
  return kvals;
}

void ps_vm_setkvals(float hold, float run, float accel, float decel) {
  hold = constrain(hold, 0.0, 1.0);
  run = constrain(run, 0.0, 1.0);
  accel = constrain(accel, 0.0, 1.0);
  decel = constrain(decel, 0.0, 1.0);
  {
    uint8_t buf[2] = {CMD_SETPARAM(PARAM_KVALHOLD), (uint8_t)round(hold / KVALS_COEFF)};
    ps_xfer("setparam kvalhold", buf, 2);
  }
  {
    uint8_t buf[2] = {CMD_SETPARAM(PARAM_KVALRUN), (uint8_t)round(run / KVALS_COEFF)};
    ps_xfer("setparam kvalrun", buf, 2);
  }
  {
    uint8_t buf[2] = {CMD_SETPARAM(PARAM_KVALACC), (uint8_t)round(accel / KVALS_COEFF)};
    ps_xfer("setparam kvalacc", buf, 2);
  }
  {
    uint8_t buf[2] = {CMD_SETPARAM(PARAM_KVALDEC), (uint8_t)round(decel / KVALS_COEFF)};
    ps_xfer("setparam kvaldec", buf, 2);
  }
}

void ps_move(ps_dir dir, uint32_t steps) {
  uint8_t buf[4] = {CMD_MOVE(dir), 0, 0, 0};
  steps = steps & MOVE_MASK;
  ps_set24(buf[1], buf[2], buf[3], steps);
  ps_xfer("move", buf, 4);
}

void ps_softstop() {
  uint8_t buf[1] = {CMD_SOFTSTOP()};
  ps_xfer("softstop", buf, 1);
}

