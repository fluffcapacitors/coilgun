
#include "coilgun.h"
#include "error.h"
#include "pins.h"
#include "safety.h"
#include "thwacker.h"


#define SAFETY_TIMER_INTERVAL_US 100
#define MAX_COIL_ON_TIME_US      (20  * 1000) // 20ms
#define MAX_THWACKER_ON_TIME_US  (150 * 1000) // 150ms, this should be longer than THWACKER_TIMEOUT_MS (thwacker.cpp) to avoid erroring if we fire with no projectile loaded

#define GRN_LED_ON  digitalWriteFast(GRN_LED_PIN, HIGH)
#define GRN_LED_OFF digitalWriteFast(GRN_LED_PIN, LOW )


static IntervalTimer safety_timer;


static void s_safety_callback(void);


void init_safety_checking(void) {
  pinMode(GRN_LED_PIN, OUTPUT);
  GRN_LED_OFF;

  safety_timer.begin(s_safety_callback, SAFETY_TIMER_INTERVAL_US);
}


// Timer interrupt callback, gets called constantly to independently check stuff and shut down if things go wrong
static void s_safety_callback(void) {
  static uint32_t coil_on_time_us[NUM_OPTOS_COILS] = {0};
  static uint32_t thwacker_on_time_us = 0;

  GRN_LED_ON;

  // Check coilgun coil on-times
  for(int i = 0; i < NUM_OPTOS_COILS; i++) {
    if(coil_is_on(i)) {
      coil_on_time_us[i] += SAFETY_TIMER_INTERVAL_US;
      if(coil_on_time_us[i] >= MAX_COIL_ON_TIME_US) {
        error("Max coil on time reached");
      }
    }
    else {
      coil_on_time_us[i] = 0;
    }
  }

  // Check thwacker on-time
  if(thwacker_is_on()) {
    thwacker_on_time_us += SAFETY_TIMER_INTERVAL_US;
    if(thwacker_on_time_us >= MAX_THWACKER_ON_TIME_US) {
      error("Max thwacker on time reached");
    }
  }
  else {
    thwacker_on_time_us = 0;
  }

  if(num_optos_triggered() > 1) { error("Multiple optos triggered"); }

  GRN_LED_OFF;
}
