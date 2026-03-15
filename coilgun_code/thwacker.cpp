
#include "pins.h"
#include "switches.h"
#include "thwacker.h"


// Needs to be long enough to guarantee projectile gets hit and launched
// Saw ~32ms from thwacker activate to projectile triggering first opto
#define THWACKER_TIMEOUT_MS 100

#define THWACKER_ON    digitalWriteFast(THWACKER_PIN, HIGH)
#define THWACKER_OFF   digitalWriteFast(THWACKER_PIN, LOW )
#define THWACKER_IS_ON digitalReadFast (THWACKER_PIN)


static uint32_t thwacker_timeout_timer = 0;
static uint32_t last_turned_off_timestamp = 0;


void init_thwacker(void) {
  pinMode(THWACKER_PIN, OUTPUT);
  turn_thwacker_off();
}

void tick_thwacker(void) {
  // Make sure thwacker always turns off after some time
  if(safety_is_on() || (millis() - thwacker_timeout_timer >= THWACKER_TIMEOUT_MS)) {
    turn_thwacker_off();
  }
}

void fire_thwacker(void) {
  if(safety_is_on() || switch_is_active(NoThwackerSwitch)) { return; }

  thwacker_timeout_timer = millis();
  THWACKER_ON; // Thwacker is normally turned off in tick_coilgun()
}

void turn_thwacker_off(void) {
  THWACKER_OFF;
  last_turned_off_timestamp = millis();
}

uint32_t thwacker_off_time(void) {
  if(thwacker_is_on()) { return 0; }
  return millis() - last_turned_off_timestamp;
}

int thwacker_is_on(void) {
  return THWACKER_IS_ON;
}
