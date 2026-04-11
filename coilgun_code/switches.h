
#ifndef SWITCHES_H
#define SWITCHES_H

#include <Arduino.h>


#define NUM_SWITCHES 8


// Important!
// The enums must match the order of switch_pins[] in switches.cpp
typedef enum {
  SafetySwitch       = 0,
  FireButton         = 1,
  DisableCoil0Switch = 2, // Disable the first coil
  DisableCoil1Switch = 3, // Disable the second coil
  DisableCoil2Switch = 4, // Disable the third coil
  NoThwackerSwitch   = 5, // When fire button pressed, don't actually trigger the thwacker
  IgnoreLoadedSwitch = 6, // Try to fire even if the loader indicates there's no projectile
  ThreeShotSwitch    = 7  // Do a three-round burst when the fire button is pressed
} SwitchesEnum;


void init_switches(void);
void tick_switches(void);

int switch_is_active(SwitchesEnum);

int safety_is_on(void);
int fire_button_pressed(void);


#endif
