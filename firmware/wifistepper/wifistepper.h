#ifndef __WIFISTEPPER_H
#define __WIFISTEPPER_H

#include "powerstep01.h"

typedef struct __cfg_motor {
  ps_mode mode;
  ps_stepsize stepsize;
  float maxspeed, minspeed;
  float accel, decel;
  float kthold, ktrun, ktaccel, ktdecel;
  
  float fullstepspeed;
  bool fsboost;
} cfg_motor;

#endif

