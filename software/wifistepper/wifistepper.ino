//#include <Arduino.h>


#include "powerstep01.h"

void wait_callback() {
  ESP.wdtFeed();
}

void setup() {
  Serial.begin(115200);
  
  ps_spiinit();

  ps_setsync(SYNC_BUSY);
  ps_setmode(MODE_VOLTAGE, STEP_128);
  ps_setmaxspeed(1000);
  ps_setfullspeed(2000, false);
  
  ps_setaccel(2000);
  ps_setdecel(2000);
  ps_setslewrate(SR_520);

  ps_setocd(250, true);
  ps_vm_setpwmfreq(0, 1);
  ps_vm_setvoltcomp(false);
  ps_setswmode(SW_USER);
  ps_setclocksel(CLK_INT16);

  ps_vm_setkvals(0.125, 0.25, 0.25, 0.25);
  ps_setalarms(true, true, true, true);
  
  ps_getstatus(true);
}

void loop() {
  Serial.println("Loop");
  
  ps_move(FWD, 2000);
  ps_waitbusy(wait_callback);
  ps_softstop();
  ps_waitbusy(wait_callback);

  ps_move(REV, 2000);
  ps_waitbusy(wait_callback);
  ps_softstop();
  ps_waitbusy(wait_callback);
}

