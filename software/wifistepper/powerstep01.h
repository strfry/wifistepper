
void ps_spiinit();

typedef struct __ps_status {
  bool busy;
} ps_status;

typedef void (*ps_waitcb)(void);
ps_status ps_getstatus(bool clear_errors =false);
void ps_waitbusy(ps_waitcb waitf =NULL);




/* STEPMODE */
typedef enum __ps_mode {
  MODE_VOLTAGE  = 0x0,
  MODE_CURRENT  = 0x1
} ps_mode;

typedef enum __ps_sync {
  SYNC_BUSY     = 0x0,
  SYNC_STEP     = 0x1
} ps_sync;

typedef enum __ps_stepsize {
  STEP_1    = 0x0,
  STEP_2    = 0x1,
  STEP_4    = 0x2,
  STEP_8    = 0x3,
  STEP_16   = 0x4,
  STEP_32   = 0x5,
  STEP_64   = 0x6,
  STEP_128  = 0x7
} ps_stepsize;

typedef struct __ps_stepmode {
  ps_mode mode;
  ps_stepsize stepsize;
  ps_sync sync_mode;
  ps_stepsize sync_stepsize;
} ps_stepmode;

ps_stepmode ps_getstepmode();
void ps_setmode(const ps_mode mode, const ps_stepsize stepsize);
void ps_setsync(const ps_sync sync, const ps_stepsize stepsize =STEP_1);


/* MAXSPEED */
float ps_getmaxspeed();
void ps_setmaxspeed(float steps_per_second);

/* FULLSPEED */
typedef struct __ps_fullspeed {
  float steps_per_sec;
  bool boost_mode;
} ps_fullspeed;

ps_fullspeed ps_getfullspeed();
void ps_setfullspeed(float steps_per_sec, bool boost_mode =false);

/* ACC */
float ps_getaccel();
void ps_setaccel(float steps_per_sec_2);

/* DEC */
float ps_getdecel();
void ps_setdecel(float steps_per_sec_2);

/* SLEW RATES */
typedef enum __ps_slewrate {
  SR_114        = (0x40 | 0x18),
  SR_220        = (0x60 | 0x0C),
  SR_400        = (0x80 | 0x07),
  SR_520        = (0xA0 | 0x06),
  SR_790        = (0xC0 | 0x03),
  SR_980        = (0xD0 | 0x02)
} ps_slewrate;

ps_slewrate ps_getslewrate();
void ps_setslewrate(ps_slewrate slew);

/* OCDTH */
typedef struct __ps_ocd {
  float millivolts;
  bool shutdown;
} ps_ocd;

ps_ocd ps_getocd();
void ps_setocd(float millivolts, bool shutdown =true);

/* CONFIG */
typedef enum __ps_clocksel {
  CLK_INT16         = 0x0,
  CLK_INT16_EXT2    = 0x8,
  CLK_INT16_EXT4    = 0x9,
  CLK_INT16_EXT8    = 0xA,
  CLK_INT16_EXT16   = 0xB,
  CLK_EXT8_XTAL     = 0x4,
  CLK_EXT16_XTAL    = 0x5,
  CLK_EXT24_XTAL    = 0x6,
  CLK_EXT32_XTAL    = 0x7,
  CLK_EXT8_OSC      = 0xC,
  CLK_EXT16_OSC     = 0xD,
  CLK_EXT24_OSC     = 0xE,
  CLK_EXT32_OSC     = 0xF
} ps_clocksel;

typedef enum __ps_swmode {
  SW_HARDSTOP       = 0x0,
  SW_USER           = 0x1
} ps_swmode;

ps_clocksel ps_getclocksel();
void ps_setclocksel(const ps_clocksel clock);

ps_swmode ps_getswmode();
void ps_setswmode(const ps_swmode swmode);

void ps_vm_setpwmfreq(uint8_t div, uint8_t mul);

/* ALARMEN */
typedef struct __ps_alarms {
  bool command_error;
  bool overcurrent;
  bool undervoltage;
  bool thermal_shutdown;
  bool user_switch;
  bool thermal_warning;
  bool stall_detect;
  bool adc_undervoltage;
} ps_alarms;

ps_alarms ps_getalarms();
void ps_setalarms(const ps_alarms * alarms);
void ps_setalarms(bool command_error, bool overcurrent, bool undervoltage, bool thermal_shutdown, bool user_switch =false, bool thermal_warning =false, bool stall_detect =false, bool adc_undervoltage =false);

/* VM */
bool ps_vm_getvoltcomp();
void ps_vm_setvoltcomp(bool voltage_compensation);

/* KVLAS */
typedef struct __ps_vm_kvals {
  float hold;
  float run;
  float accel;
  float decel;
} ps_vm_kvals;

ps_vm_kvals ps_vm_getkvals();
void ps_vm_setkvals(float hold, float run, float accel, float decel);


/* MOVE */
typedef enum __ps_dir {
  REV       = 0x0,
  FWD       = 0x1
} ps_dir;

void ps_move(ps_dir dir, uint32_t steps);
void ps_softstop();

