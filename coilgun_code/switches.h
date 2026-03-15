
#ifndef SWITCHES_H
#define SWITCHES_H

#include <Arduino.h>


#define NUM_SWITCHES 8

// Important!
// The enums must match the order of switch_pins[] in switches.cpp
typedef enum {
  SafetySwitch       = 0, // Can only fire when enabled
  FireButton         = 1, // Normal fire with thwacker
  NoThwackerSwitch   = 2, // When fire button pressed, don't actually trigger the thwacker
  IgnoreLoadedSwitch = 3, // Ignore whether loader indicates a projectile is present
  ThreeShotSwitch    = 4, // Do a three-round burst when the fire button is pressed
  DisableCoil0Switch = 5, // Disable the first coil
  DisableCoil1Switch = 6, // Disable the second coil
  DisableCoil2Switch = 7  // Disable the third coil
} SwitchesEnum;


void init_switches(void);
void tick_switches(void);

int switch_is_active(SwitchesEnum);

int safety_is_on(void);
int fire_button_pressed(void);


#endif
