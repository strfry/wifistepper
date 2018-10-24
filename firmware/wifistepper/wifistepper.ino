//#include <Arduino.h>


#include "powerstep01.h"

void wait_callback() {
  ESP.wdtFeed();
}

void printalarms(ps_alarms * a) {
  Serial.println("Alarms:");
  Serial.print("Command Error: "); Serial.println(a->command_error);
  Serial.print("Overcurrent: "); Serial.println(a->overcurrent);
  Serial.print("Undervoltage: "); Serial.println(a->undervoltage);
  Serial.print("Thermal Shutdown: "); Serial.println(a->thermal_shutdown);
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  
  ps_spiinit();

  ps_setsync(SYNC_BUSY);
  //ps_setmode(MODE_VOLTAGE);                 // V
  ps_setmode(MODE_CURRENT);                 // C
  ps_setstepsize(STEP_16);                  // C
  //ps_setstepsize(STEP_128);                 // V
  
  ps_setmaxspeed(1000);
  ps_setminspeed(10, true);
  ps_setaccel(50);
  ps_setdecel(50);

  ps_setfullstepspeed(2000, false);
  
  ps_setslewrate(SR_520);

  ps_setocd(500, true);
  //ps_vm_setpwmfreq(0, 1);                   // V
  ps_cm_setpredict(true);                   // C
  ps_setvoltcomp(false);
  ps_setswmode(SW_USER);
  ps_setclocksel(CLK_INT16);

  //ps_setktvals(0.125, 0.25, 0.25, 0.25);    // V
  ps_setktvals(0.75, 0.75, 0.75, 0.75);     // C
  ps_setalarmconfig(true, true, true, true);
  
  ps_getstatus(true);
}

void loop() {
  Serial.println("Loop");

  ps_alarms alarms = ps_getalarms();
  printalarms(&alarms);
  
  ps_move(FWD, 2000);
  ps_waitbusy(wait_callback);
  ps_softstop();
  ps_waitbusy(wait_callback);

  //ps_getstatus();

  ps_move(REV, 2000);
  ps_waitbusy(wait_callback);
  ps_softstop();
  ps_waitbusy(wait_callback);

  //ps_getstatus();

}

