
typedef enum __ps_dir {
  REV       = 0x0,
  FWD       = 0x1
} ps_direction;

typedef enum __ps_movement {
  M_STOPPED     = 0x0,
  M_ACCEL       = 0x1,
  M_DECEL       = 0x2,
  M_CONSTSPEED  = 0x3
} ps_movement;

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

typedef struct __ps_status {
  ps_direction direction;
  ps_movement movement;
  bool hiz;
  bool user_switch;
  bool step_clock;
  ps_alarms alarms;
} ps_status;

typedef enum __ps_posact {
  POS_RESET         = 0x0,
  POS_COPYMARK      = 0x1
} ps_posact;


/* ACC */
float ps_getaccel();
void ps_setaccel(float steps_per_sec_2);

/* DEC */
float ps_getdecel();
void ps_setdecel(float steps_per_sec_2);

/* MAXSPEED */
float ps_getmaxspeed();
void ps_setmaxspeed(float steps_per_second);

/* ADCOUT */
int ps_readadc();

/* OCDTH */
typedef struct __ps_ocd {
  float millivolts;
  bool shutdown;
} ps_ocd;

ps_ocd ps_getocd();
void ps_setocd(float millivolts, bool shutdown =true);

/* FULLSPEED */
typedef struct __ps_fullspeed {
  float steps_per_sec;
  bool boost_mode;
} ps_fullspeed;

ps_fullspeed ps_getfullspeed();
void ps_setfullspeed(float steps_per_sec, bool boost_mode =false);

/* STEPMODE */
typedef enum __ps_mode {
  MODE_VOLTAGE  = 0x0,
  MODE_CURRENT  = 0x1
} ps_mode;

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

typedef enum __ps_sync {
  SYNC_BUSY     = 0x0,
  SYNC_STEP     = 0x1
} ps_sync;

typedef struct __ps_stepmode {
  ps_sync sync_mode;
  ps_stepsize sync_stepsize;
} ps_syncinfo;

ps_mode ps_getmode();
ps_stepsize ps_getstepsize();
ps_syncinfo ps_getsync();
void ps_setmode(ps_mode mode);
void ps_setstepsize(ps_stepsize stepsize);
void ps_setsync(ps_sync sync, ps_stepsize stepsize =STEP_1);

/* ALARMEN */
ps_alarms ps_getalarmconfig();
void ps_setalarmconfig(const ps_alarms * alarms);
void ps_setalarmconfig(bool command_error, bool overcurrent, bool undervoltage, bool thermal_shutdown, bool user_switch =false, bool thermal_warning =false, bool stall_detect =false, bool adc_undervoltage =false);
ps_alarms ps_getalarms();

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
void ps_setclocksel(ps_clocksel clock);

ps_swmode ps_getswmode();
void ps_setswmode(ps_swmode swmode);

void ps_vm_setpwmfreq(uint8_t div, uint8_t mul);

/* KTVLAS */
typedef struct __ps_ktvals {
  float hold;
  float run;
  float accel;
  float decel;
} ps_ktvals;

ps_ktvals ps_getktvals();
void ps_setktvals(float hold, float run, float accel, float decel);

/* VM */
bool ps_vm_getvoltcomp();
void ps_vm_setvoltcomp(bool voltage_compensation);

/* CM */
bool ps_cm_gettorqreg();
void ps_cm_settorqreg(bool torque_reg_adcin);


/* MOVE */
uint32_t ps_move(ps_direction dir, uint32_t steps);

void ps_softstop();
void ps_hardstop();
void ps_softhiz();
void ps_hardhiz();

int32_t ps_getpos();
void ps_setpos(int32_t pos);
void ps_resetpos();
void ps_gohome();

int32_t ps_getmark();
void ps_setmark(int32_t mark);
void ps_gomark();
void ps_goto(int32_t pos);
void ps_goto(int32_t pos, ps_direction dir);

float ps_getspeed();
void ps_run(ps_direction dir, float speed);
void ps_stepclock(ps_direction dir);

void ps_gountil(ps_posact act, ps_direction dir, float speed);
void ps_releasesw(ps_posact act, ps_direction dir);



void ps_spiinit();



typedef void (*ps_waitcb)(void);
ps_status ps_getstatus(bool clear_errors =false);
void ps_waitbusy(ps_waitcb waitf =NULL);

void ps_reset();
void ps_nop();

