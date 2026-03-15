
#include "coilgun.h"
#include "error.h"
#include "loader.h"
#include "pins.h"
#include "safety.h"
#include "switches.h"
#include "thwacker.h"


// Max allowed time from firing enabled to projectile leaving barrel
// Thwacker takes ~32ms from activation to projectile reaching first opto
// From there firing takes ~15ms to finish for a normal dowel (~50ms total)
// But this can take much longer with lighter projectiles.
#define SHORT_FIRING_TIMEOUT_MS 200

// If we disable the loader/thwacker for manual loading of other projectiles, this timeout needs to be rather long.
// With a loader attached (like the magazine loader), it's untenable to position the projectile to be blocking the first opto.
// Much easier is to just shove it in the end with enough speed to guarantee it makes it.
// So have the timeout extra long, so you can press the fire button then immediately shove the projectile in, and it will fire.
#define LONG_FIRING_TIMEOUT_MS 1500

#define FIRING_EN_LED_ON  digitalWriteFast(FIRING_EN_LED_PIN, HIGH)
#define FIRING_EN_LED_OFF digitalWriteFast(FIRING_EN_LED_PIN, LOW )


// Must be in firing order (0th index is the first opto/coil)
static const int opto_pins[NUM_OPTOS_COILS] = {OPTO_0_PIN, OPTO_1_PIN, OPTO_2_PIN};
static const int coil_pins[NUM_OPTOS_COILS] = {COIL_0_PIN, COIL_1_PIN, COIL_2_PIN};

static int enable_firing = 0;
static uint32_t firing_timeout_timer = 0;


static int s_opto_triggered(uint8_t);
static void s_turn_coil_on(uint8_t);


void init_coilgun(void) {
  for(int i = 0; i < NUM_OPTOS_COILS; i++) {
    pinMode(opto_pins[i], INPUT); // Do not set pull resistor, need these pains to be 5V-tolerant
    pinMode(coil_pins[i], OUTPUT);
  }
  turn_coils_off();

  init_safety_checking();

  pinMode(FIRING_EN_LED_PIN, OUTPUT);
  FIRING_EN_LED_OFF;
}

typedef enum {
  ResetCoilgun,
  WaitForFirstOpto,
  FireNextCoil,
  WaitForOptoTrigger,
  WaitForOptoUntrigger,
} CoilgunStateEnum;

void tick_coilgun(void) {
  static CoilgunStateEnum state = ResetCoilgun;
  static uint8_t active_coil = 0;

  if(enable_firing) { FIRING_EN_LED_ON;  }
  else              { FIRING_EN_LED_OFF; }

  /*==================================================*/

  uint32_t firing_timeout = SHORT_FIRING_TIMEOUT_MS;
  // If manual projectile loading is needed, use the longer timeout to give time for that once fire button is pressed
  if(switch_is_active(NoThwackerSwitch) || current_loader() == NoneLoader) {
    firing_timeout = LONG_FIRING_TIMEOUT_MS;
  }

  // If firing is allowed, disable it after some time
  // Normally this is done automatically after the projectile reaches the end, but maybe we failed to fire one properly
  if(millis() - firing_timeout_timer >= firing_timeout) {
    enable_firing = 0;
  }

  if(safety_is_on() || enable_firing == 0) {
    turn_coils_off();
    enable_firing = 0; // Needed else you could fire, then quickly turn safety off and it would fire
    state = ResetCoilgun;

    return;
  }

  /*==================================================*/

  if(state == ResetCoilgun) {
    for(int i = 0; i < NUM_OPTOS_COILS; i++) {
      // First opto is allowed to be triggered (maybe a projectile was put in manually)
      if(i == 0) { continue; }

      // No other opto may be triggered when starting a firing sequence
      if(s_opto_triggered(i)) {
        error("Unexpected opto triggered before firing");
      }
    }

    active_coil = 0;
    state = WaitForFirstOpto;
  }

  else if(state == WaitForFirstOpto) {
    if(s_opto_triggered(0)) {
      // Once projectile reaches first opto, thwacker has done its job
      // This feels like it should be somewhere else, but that would require some extra kinda-weird infrastructure
      // (ability to indicate when this state machine has reached this point, as a sticky flag)
      turn_thwacker_off();
      state = FireNextCoil;
    }
  }

  else if(state == FireNextCoil) {
    if(active_coil >= NUM_OPTOS_COILS) { error("State machine tried to fire too many coils"); }

    turn_coils_off();

    SwitchesEnum disable_switch = DisableCoil0Switch;
    switch(active_coil) {
      case 0: disable_switch = DisableCoil0Switch; break;
      case 1: disable_switch = DisableCoil1Switch; break;
      case 2: disable_switch = DisableCoil2Switch; break;
    }
    if(!switch_is_active(disable_switch)) {
      s_turn_coil_on(active_coil);
    }

    state = WaitForOptoTrigger;
  }

  else if(state == WaitForOptoTrigger) {
    if(s_opto_triggered(active_coil)) {
      state = WaitForOptoUntrigger;
    }
  }

  else if(state == WaitForOptoUntrigger) {
    if(!s_opto_triggered(active_coil)) {
      active_coil++;

      // Finished the shot
      if(active_coil >= NUM_OPTOS_COILS) {
        turn_coils_off();
        enable_firing = 0; // Finished the shot, disallow firing again
        state = ResetCoilgun;
      }
      // More coils to fire
      else {
        state = FireNextCoil;
      }
    }
  }

}

int coilgun_is_idle(void) {
  if(enable_firing == 0) { return 1; }
  return 0;
}

void enable_coilgun_firing(void) {
  // Allow coilgun to fire until the firing timeout is reached
  // After that time, firing is disabled in tick_coilgun()
  enable_firing = 1;
  firing_timeout_timer = millis();
}

void turn_coils_off(void) {
  for(int i = 0; i < NUM_OPTOS_COILS; i++) {
    digitalWriteFast(coil_pins[i], LOW);
  }
}

int coil_is_on(uint8_t coil) {
  if(coil >= NUM_OPTOS_COILS) { error("Coil out of range"); }

  return digitalReadFast(coil_pins[coil]);
}

int num_optos_triggered(void) {
  uint8_t tmp = 0;
  for(int i = 0; i < NUM_OPTOS_COILS; i++) {
    if(s_opto_triggered(i)) { tmp++; }
  }

  return tmp;
}


static int s_opto_triggered(uint8_t opto) {
  if(opto >= NUM_OPTOS_COILS) { error("Opto out of range"); }

  return !digitalReadFast(opto_pins[opto]); // Optos are active low
}

static void s_turn_coil_on(uint8_t coil) {
  if(coil >= NUM_OPTOS_COILS) { error("Coil out of range"); }

  digitalWriteFast(coil_pins[coil], HIGH);
}
