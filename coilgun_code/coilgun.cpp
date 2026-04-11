
#include "coilgun.h"
#include "error.h"
#include "loader.h"
#include "pins.h"
#include "safety.h"
#include "switches.h"


// Max allowed time from firing enabled to the first opto being triggered
// Thwacker was observed to take ~32ms from activation to projectile reaching first opto
#define AUTO_LOADING_FIRING_TIMEOUT_MS 150

// If we disable the loader/thwacker for manual loading of other projectiles, this timeout needs to be rather long.
// With a loader attached (like the magazine loader), it's untenable to position the projectile to be blocking the first opto.
// Much easier is to just shove it in the end with enough speed to guarantee it makes it.
// So have the timeout extra long, so you can press the fire button then immediately shove the projectile in, and it will fire.
#define MANUAL_LOADING_FIRING_TIMEOUT_MS 1500

// Max allowed time from first opto triggering to the firing sequence completing
// After first opto trigger, firing takes ~15ms to finish for a normal dowel
// But this can take much longer with lighter projectiles.
#define SHOT_COMPLETE_TIMEOUT_MS 100

#define FIRING_EN_LED_ON  digitalWriteFast(FIRING_EN_LED_PIN, HIGH)
#define FIRING_EN_LED_OFF digitalWriteFast(FIRING_EN_LED_PIN, LOW )


// Must be in firing order (0th index is the first opto/coil)
static const int opto_pins[NUM_OPTOS_COILS] = {OPTO_0_PIN, OPTO_1_PIN, OPTO_2_PIN};
static const int coil_pins[NUM_OPTOS_COILS] = {COIL_0_PIN, COIL_1_PIN, COIL_2_PIN};

static int enable_firing = 0;
static LoadingTypeEnum this_shot_loading_type = AutoLoading;
static uint8_t first_opto_triggered_flag = 0; // Automatically set and reset in tick_coilgun()
static uint8_t coilgun_fired_flag = 0;


static int s_opto_triggered(uint8_t);
static void s_turn_coil_on(uint8_t);
static uint32_t s_get_firing_timeout(LoadingTypeEnum);


void init_coilgun(void) {
  for(int i = 0; i < NUM_OPTOS_COILS; i++) {
    pinMode(opto_pins[i], INPUT); // Do not set pull resistor, need these pins to be 5V-tolerant
    pinMode(coil_pins[i], OUTPUT);
  }
  turn_coils_off();

  /* ===== /!\ VERY IMPORTANT /!\ ===== */
  init_safety_checking();
  /* ================================== */

  pinMode(FIRING_EN_LED_PIN, OUTPUT);
  FIRING_EN_LED_OFF;
}

void tick_coilgun(void) {
  typedef enum {
    ResetCoilgun,
    WaitForFirstOpto,
    FireNextCoil,
    WaitForOptoTrigger,
    WaitForOptoUntrigger,
  } CoilgunStateEnum;

  static CoilgunStateEnum state = ResetCoilgun;
  static uint8_t active_coil = 0;
  static uint32_t firing_enabled_timeout_timer = 0;
  static uint32_t shot_complete_timeout_timer = 0;

  if(enable_firing) { FIRING_EN_LED_ON;  }
  else              { FIRING_EN_LED_OFF; }

  /*======================SAFETY======================*/

  if(safety_is_on() || enable_firing == 0) {
    turn_coils_off();
    enable_firing = 0; // Needed else you could fire, then quickly turn safety off and it would fire
    state = ResetCoilgun;

    // Don't even run the state machine if firing is disallowed
    return;
  }

  /*==================================================*/

  // Only even reach this point when the safety is off and firing has been allowed

  if(state == ResetCoilgun) {
    // Check for any triggered optos
    for(int i = 0; i < NUM_OPTOS_COILS; i++) {
      // First opto may be triggered initially only if we're doing a manual loading (skip checking it)
      if((i == 0) && (this_shot_loading_type == ManualLoading)) { continue; }
      // Otherwise, no opto may be triggered when starting a firing sequence
      if(s_opto_triggered(i)) {
        error("Unexpected opto triggered before firing");
      }
    }

    // Reset state machine variables and continue
    active_coil = 0;
    firing_enabled_timeout_timer = millis(); // Start tracking how long until first opto is triggered (if it ever is)
    state = WaitForFirstOpto;
  }

  else if(state == WaitForFirstOpto) {
    // Check if we're waiting too long for the first opto to trigger
    uint32_t firing_timeout = s_get_firing_timeout(this_shot_loading_type);
    if(millis() - firing_enabled_timeout_timer > firing_timeout) {
      enable_firing = 0;
      state = ResetCoilgun; // Needed else firing could be disabled, then something else could enable it before reaching this state machine again
      return;
    }

    if(s_opto_triggered(0)) {
      first_opto_triggered_flag = 1;
      shot_complete_timeout_timer = millis(); // Start tracking how long the shot is taking
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
    if(switch_is_active(disable_switch) == 0) {
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
        first_opto_triggered_flag = 0;
        coilgun_fired_flag = 1;
        state = ResetCoilgun;
      }
      // More coils to fire
      else {
        state = FireNextCoil;
      }
    }
  }

  /*======================SAFETY======================*/

  // If the first opto triggered but the shot didn't complete, error out
  if(first_opto_triggered_flag && (millis() - shot_complete_timeout_timer > SHOT_COMPLETE_TIMEOUT_MS)) {
    error("First opto triggered, but shot did not complete in time");
  }

  /*==================================================*/

}

int coilgun_is_idle(void) {
  if(enable_firing == 0) { return 1; }
  return 0;
}

void allow_coilgun_firing(LoadingTypeEnum loading_type) {
  if(safety_is_on()) { return; }

  this_shot_loading_type = loading_type;
  enable_firing = 1;
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

int first_opto_was_triggered(void) {
  return first_opto_triggered_flag;
}

int coilgun_successfully_fired(void) {
  int tmp = coilgun_fired_flag;
  coilgun_fired_flag = 0;
  
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

static uint32_t s_get_firing_timeout(LoadingTypeEnum loading_type) {
  switch(loading_type) {
    case AutoLoading:   return AUTO_LOADING_FIRING_TIMEOUT_MS;
    case ManualLoading: return MANUAL_LOADING_FIRING_TIMEOUT_MS;
  }
  
  return 0;
}
