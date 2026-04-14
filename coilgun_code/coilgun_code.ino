
// Teensy 3.2, 72MHz, USB Serial

// TODO:
// Why getting random "multiple optos triggered" errors?

#include "cg_eeprom.h"
#include "coilgun.h"
#include "loader.h"
#include "oled.h"
#include "switches.h"


// When firing multiple shots automatically, how long to wait for the coilgun and loader to be ready before abandoning
// future shots and resetting the state machine.
// Must be long enough for slow loaders to load the next shot, but should be kept short to reset when ammo has run out
#define MULTI_SHOT_TIMEOUT_MS 1000


static void s_tick_firing(void);
static int  s_ready_to_fire(void);


void setup() {
  init_switches();
  init_oled();

  // Before initializing anything else, check if we enter the menu (we might change settings that affect other stuff)
  if(switch_is_active(FireButton)) {
    enter_oled_menu(); // Stays here until it's done
  }
  refresh_oled();

  init_loader(); // Inits thwacker as well
  init_coilgun(); // Inits safety timer as well
}

void loop() {
  tick_switches();
  tick_loader(); // Ticks thwacker as well
  tick_coilgun();
  s_tick_firing();

  // Update EEPROM (and eventually OLED) directly after each coilgun shot
  // EEPROM write (and OLED) is blocking for a few ms, so we definitely don't want to do it while a shot is ongoing
  // Plus, if the safety interrupt errors out, then we'll never return to what we were doing before, possibly leaving a partial EEPROM write
  // This is probably the least likely moment for the safety errors to trigger
  if(coilgun_successfully_fired()) {
    increment_total_shots();
    refresh_oled(); // Takes ~40ms
  }
}


static void s_tick_firing(void) {
  typedef enum {
    WaitForButtonPress,
    WaitForReadiness,
    FireCoilGun,
  } FiringStateEnum;

  static FiringStateEnum state = WaitForButtonPress;
  static int num_shots = 0;
  static uint32_t multi_shot_timeout_timer = 0;

  /*======================SAFETY======================*/

  if(safety_is_on()) {
    fire_button_pressed(); // Must call this to make sure button-press events are cleared, else could press fire button, turn safety off, and it would fire
    state = WaitForButtonPress;

    return;
  }

  /*==================================================*/

  // Only even reach this point when the safety is off

  // If we're waiting too long for the coilgun or loader to be ready for the next shot, reset the state machine
  if(millis() - multi_shot_timeout_timer >= MULTI_SHOT_TIMEOUT_MS) {
    state = WaitForButtonPress;
  }

  if(state == WaitForButtonPress) {
    if(fire_button_pressed() && s_ready_to_fire()) {
      num_shots = 1;
      if(switch_is_active(ThreeShotSwitch)) { num_shots = 3; }
      multi_shot_timeout_timer = millis();

      state = WaitForReadiness;
    }
  }

  else if(state == WaitForReadiness) {
    if(s_ready_to_fire()) {
      state = FireCoilGun;
    }
  }

  else if(state == FireCoilGun) {
    fire_loader();
    multi_shot_timeout_timer = millis(); // Reset on each shot

    num_shots--;
    if(num_shots > 0) {
      state = WaitForReadiness;
    }
    else {
      state = WaitForButtonPress;
    }
  }
}

static int s_ready_to_fire(void) {
  if(loader_is_ready() && coilgun_is_idle()) { return 1; }
  return 0;
}
