
// Teensy 3.2, 72MHz, USB Serial

#include "coilgun.h"
#include "loader.h"
#include "switches.h"
#include "thwacker.h"


// When firing multiple shots automatically, how long to wait for the coilgun and loader to be ready before abandoning
// future shots and resetting the state machine.
// Must be long enough for slow loaders to load the next shot, but should be kept short to reset when ammo has run out
#define MULTI_SHOT_TIMEOUT_MS 1000


static void s_tick_firing(void);
static int  s_ready_to_fire(void);
static void s_fire_coilgun(void);


void setup() {
  init_switches();
  init_loader(); // Inits thwacker as well
  init_coilgun(); // Inits safety timer as well
}

void loop() {
  tick_switches();
  tick_loader();
  s_tick_firing();
  tick_thwacker();
  tick_coilgun();
}


typedef enum {
  WaitForButtonPress,
  WaitForReadiness,
  FireCoilGun,
} FiringStateEnum;

static void s_tick_firing(void) {
  static FiringStateEnum state = WaitForButtonPress;
  static int num_shots = 0;
  static uint32_t wait_for_readiness_timer = 0;

  if(safety_is_on()) {
    fire_button_pressed(); // Call this to make sure button-press events are cleared
    state = WaitForButtonPress;

    return;
  }

  // If we're waiting too long for the coilgun or loader to be ready for the next shot, reset the state machine
  if(millis() - wait_for_readiness_timer >= MULTI_SHOT_TIMEOUT_MS) {
    state = WaitForButtonPress;
  }

  if(state == WaitForButtonPress) {
    if(fire_button_pressed() && s_ready_to_fire()) {
      num_shots = 1;
      if(switch_is_active(ThreeShotSwitch)) { num_shots = 3; }
      wait_for_readiness_timer = millis();

      state = WaitForReadiness;
    }
  }

  else if(state == WaitForReadiness) {
    if(s_ready_to_fire()) {
      state = FireCoilGun;
    }
  }

  else if(state == FireCoilGun) {
    s_fire_coilgun();

    num_shots--;
    if(num_shots <= 0) {
      state = WaitForButtonPress;
    }
    else {
      state = WaitForReadiness;
    }
  }
}

static int s_ready_to_fire(void) {
  if(loader_is_ready() && coilgun_is_idle()) { return 1; }
  return 0;
}

static void s_fire_coilgun(void) {
  enable_coilgun_firing();
  fire_loader();
}
