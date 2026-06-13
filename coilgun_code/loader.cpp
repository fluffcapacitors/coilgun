
#include "error.h"
#include "coilgun.h"
#include "loader.h"
#include "switches.h"
#include "pins.h"
#include "thwacker.h"


// ID resistor pullup should be 10k. If 48V shorted to it, it would only inject 5mA into the 3.3V line (should be fine)
// Voltages with pulldown (roughly equally-spaced voltages, for up to 4 different loaders (as well as no loader)):
// 2.2k = 595mV
// 6.8k = 1336mV
// 15k  = 1980mV
// 39k  = 2627mV

#define AREF_MV 3300
#define ADC_MAX_VAL 1023
#define ID_MV_TOLERANCE 200

#define NONE_LOADER_ID_MV  3200
#define MAG_LOADER_ID_MV   595
#define CHAIN_LOADER_ID_MV 1336

// Magaine loader parameters
#define MAG_LOADED_PIN   LOADER_IO_0_PIN // IR proximity sensor digital output
#define MAG_LOADED_LEVEL LOW // Outputs low when something is detected
#define MAG_IS_LOADED    (digitalReadFast(MAG_LOADED_PIN) == MAG_LOADED_LEVEL)
// Enforced minimum time between shots for the magazine loader (empirically derived)
// Tested down to 150ms with high-speed footage. Looked good, would be an absolute minimum. 200ms is safer
#define MAG_MIN_THWACKER_OFF_TIME_MS 200

// Chain loader parameters
// Chain drive motor is being run off 20V
// https://www.pjrc.com/teensy/td_pulse.html
#define CHAIN_LOADED_PIN      LOADER_IO_0_PIN
#define CHAIN_LOADED_LEVEL    HIGH // Outputs high when the endstop is pressed
#define CHAIN_IS_LOADED       (digitalReadFast(CHAIN_LOADED_PIN) == CHAIN_LOADED_LEVEL)
#define CHAIN_DRIVE_PIN       LOADER_IO_1_PIN
#define CHAIN_PWM_FREQ        8789.062F // Supposedly the precise frequency needed to get a perfect 12 bits resolution (Teensy 3.2, 72MHz)
#define CHAIN_PWM_BITS        12
#define CHAIN_PWM_MAX_VAL     4095L // L instead of UL to prevent compilation warnings about comparing signed and unsigned integers (target_chain_speed is signed)
// =============== Tweak these values ===============
#define CHAIN_MIN_DC_PERCENT  20 // Minimum duty cycle, should be at least like 10 (else loading the chain will be extremely slow)
#define CHAIN_MAX_DC_PERCENT  75 // Maximum duty cycle
// Amount of time the loader should advance per shot, before hitting the endstop
// Ideally this value should advance the chain ~90% of the way, then the sustain takes it the rest of the way
#define CHAIN_ADVANCE_TIME_MS 1000UL
#define CHAIN_SUSTAIN_TIME_MS 1000 // After advancing, keep going at minimum speed for this long to ensure we hit the endstop (or stop if we don't)
#define CHAIN_SETTLE_TIME_MS  100 // After stopping the chain, wait for things to settle before thwacking again
// What percent of the advance time to spend accelerating/decelerating
// So 30% here means take 15% of CHAIN_ADVANCE_TIME_MS to accelerate, and 15% to decelerate
// Choose this value (along with min/max speed) to give smooth accel, then tune CHAIN_ADVANCE_TIME_MS appropriately
#define CHAIN_ACCEL_PERCENT   30
// ==================================================
#define CHAIN_MIN_DC_VAL      ((CHAIN_PWM_MAX_VAL * CHAIN_MIN_DC_PERCENT) / 100) // Duty cycle timer value for min speed
#define CHAIN_MAX_DC_VAL      ((CHAIN_PWM_MAX_VAL * CHAIN_MAX_DC_PERCENT) / 100) // Duty cycle timer value for max speed
#define CHAIN_ACCEL_TIME_MS   ((CHAIN_ADVANCE_TIME_MS * CHAIN_ACCEL_PERCENT) / 200) // How long the accel or decel takes. The 200 is /100 /2, simplified
#define CHAIN_ACCEL_STEP_MS   5 // How frequently to adjust duty cycle during acceleration
#define CHAIN_ACCEL_NUM_STEPS (CHAIN_ACCEL_TIME_MS / CHAIN_ACCEL_STEP_MS) // How many steps we'll take when going from min speed to max speed
#define CHAIN_ACCEL_STEP_VAL  ((CHAIN_MAX_DC_VAL - CHAIN_MIN_DC_VAL) / CHAIN_ACCEL_NUM_STEPS) // How much to change PWM on each step
#define CHAIN_CRUISE_TIME_MS  (CHAIN_ADVANCE_TIME_MS - (CHAIN_ACCEL_TIME_MS * 2)) // Total advance time minus the accelerations
// Time to wait after turning off the thwacker before starting to move the chain again
// Unlike the mag loader, we're not waiting for dowels to drop via gravity, we just need to guarantee the thwacker is retracted before moving the chain
// We also want to make sure the shot completed and OLED updated, and the endstop untriggers
#define CHAIN_MIN_THWACKER_OFF_TIME_MS 100


typedef enum {
  SlowChainAdvance,
  FastChainAdvance
} ChainAdvanceSpeedEnum;

typedef enum {
  ChainIsReady,
  InitChainAdvance,
  WaitForThwacker,
  AccelerateChain,
  WaitCruiseTime,
  DecelerateChain,
  WaitSustainTime,
  WaitSettleTime
} ChainAdvanceStateEnum;


static LoaderTypeEnum loader = NoneLoader;

static ChainAdvanceSpeedEnum chain_advance_speed = SlowChainAdvance;
static ChainAdvanceStateEnum chain_advance_state = ChainIsReady;
static uint32_t current_chain_speed = 0;

static void s_trigger_chain_advance(ChainAdvanceSpeedEnum);
static void s_set_chain_speed(uint32_t);
static void s_tick_chain_advance(void);

static LoaderTypeEnum s_get_attached_loader(void);
static int s_mv_matches_id_val(int, int);


void init_loader(void) {
  pinMode(LOADER_ID_PIN, INPUT);
  analogReadResolution(10);
  loader = s_get_attached_loader();

  // Thwacker is always init (set up its output pin), even if unused, because the safety checking looks at it
  init_thwacker();

  // Init the loader IO for its specific use case

  if(loader == NoneLoader) {
    // Nothing connected of course
  }

  else if(loader == MagLoader) {
    pinMode(MAG_LOADED_PIN, INPUT_PULLUP); // Digital output of IR sensor (active low)
    // pinMode(LOADER_IO_1_PIN, INPUT); // Analog output of IR sensor, currently unused
  }

  else if(loader == ChainLoader) {
    pinMode(CHAIN_LOADED_PIN, INPUT_PULLUP); // Endstop switch (active high)
    pinMode(CHAIN_DRIVE_PIN, OUTPUT);
    s_set_chain_speed(0);

    analogWriteFrequency(CHAIN_DRIVE_PIN, CHAIN_PWM_FREQ);
    analogWriteResolution(CHAIN_PWM_BITS);
  }
}

// Called continuously to update state
void tick_loader(void) {
  if(loader == NoneLoader) {
    // Nothing to do
  }

  else if(loader == MagLoader || loader == ChainLoader) {
    // The thwacker is turned on in fire_loader(), and turned off here once the projectile makes it to the first coilgun opto
    if(first_opto_was_triggered()) {
      turn_thwacker_off();
    }
    tick_thwacker();

    if(loader == ChainLoader) {
      s_tick_chain_advance();
    }
  }
}

// Check to see if it's okay to fire the loader
int loader_is_ready(void) {
  if(loader == NoneLoader) {
    return 1; // Always ready (no sensing circuitry)
  }

  else if(loader == MagLoader) {
    // Once the thwacker turns off, it takes time for it to retract, and for the next dowel to drop into place
    // We're definitely not ready unless that minimum time has passed
    if(thwacker_off_time() < MAG_MIN_THWACKER_OFF_TIME_MS) { return 0; }
    // Otherwise we check the IR sensor, unless the override switch is on
    if(switch_is_active(IgnoreLoadedSwitch)) { return 1; }
    if(MAG_IS_LOADED) { return 1; }

    return 0;
  }

  else if(loader == ChainLoader) {
    // If the state machine is controlling the chain, we're never ready
    if(chain_advance_state != ChainIsReady) { return 0; }

    // Else we're always ready to fire. With the chain loader, "firing" means either:
    // - Fire the thwacker and advance the chain, or
    // - Only advance the chain
    // The firing function checks whether the endstop (loaded) switch is triggered and whether the thwacker is disabled,
    // and decides whether to fire and/or advance the chain
    return 1;
  }

  return 0;
}

// FIRE
void fire_loader(void) {
  if(safety_is_on()) { return; }

  if(loader == NoneLoader) {
    allow_coilgun_firing(ManualLoading);
  }

  else if(loader == MagLoader) {
    // If thwacker is disabled, enable the longer firing timeout for manually loading stuff
    LoadingTypeEnum loading_type = AutoLoading;
    if(switch_is_active(NoThwackerSwitch)) { loading_type = ManualLoading; }

    allow_coilgun_firing(loading_type);
    if(loading_type == AutoLoading) {
      fire_thwacker();
    }
  }

  else if(loader == ChainLoader) {
    if(CHAIN_IS_LOADED) {
      // Thwacker is enabled, fire!
      if(switch_is_active(NoThwackerSwitch) == 0) {
        allow_coilgun_firing(AutoLoading);
        fire_thwacker();
        s_trigger_chain_advance(FastChainAdvance);
      }
      // Else thwacker is disabled and chain is fully advanced, do nothing
    }
    // In any case, if the endstop isn't triggered, slowly advance the chain and don't fire the thwacker
    else {
      s_trigger_chain_advance(SlowChainAdvance);
    }
  }
}

void turn_loader_off(void) {
  if(loader == NoneLoader) {

  }

  else if(loader == MagLoader) {

  }

  else if(loader == ChainLoader) {
    s_set_chain_speed(0);
  }
}

LoaderTypeEnum get_attached_loader(void) {
  return loader;
}


static void s_trigger_chain_advance(ChainAdvanceSpeedEnum speed) {
  chain_advance_speed = speed;
  chain_advance_state = InitChainAdvance;
  s_tick_chain_advance();
}

static void s_set_chain_speed(uint32_t speed) {
  analogWrite(CHAIN_DRIVE_PIN, speed);
  current_chain_speed = speed;
}

static void s_tick_chain_advance(void) {
  static uint32_t timer = 0;
  static uint32_t sustain_time = 0;
  static int32_t target_chain_speed = 0;

  if(chain_advance_state == ChainIsReady) {
    s_set_chain_speed(0);
    return;
    // Endstop may or may not be triggered
  }
  // If the endstop triggers (while the chain is moving), we stop the motor and wait for the settling time (which then goes to the ready state)
  else if(CHAIN_IS_LOADED && (current_chain_speed != 0)) {
    s_set_chain_speed(0);
    timer = millis();
    chain_advance_state = WaitSettleTime;
    return;
  }

  if(chain_advance_state == InitChainAdvance) {
    target_chain_speed = CHAIN_MIN_DC_VAL;
    sustain_time = CHAIN_SUSTAIN_TIME_MS;
    chain_advance_state = WaitForThwacker;
  }

  else if(chain_advance_state == WaitForThwacker) {
    // Once thwacker has been off for long enough, we can start the motor
    if(thwacker_off_time() >= CHAIN_MIN_THWACKER_OFF_TIME_MS) {
      if(chain_advance_speed == FastChainAdvance) {
        chain_advance_state = AccelerateChain;
      }
      else {
        // When going slow, we still want to move the chain about the same amount
        // Move the chain for longer, relative to the speed difference between the high speed and low speed
        sustain_time = (CHAIN_ADVANCE_TIME_MS * CHAIN_MAX_DC_PERCENT) / (CHAIN_MIN_DC_PERCENT + 1); // Simple don't divide by 0
        if(sustain_time > 5000) { sustain_time = 5000; } // But keep it reasonable
        chain_advance_state = WaitSustainTime;
      }

      timer = millis();
      s_set_chain_speed(target_chain_speed);
    }
  }

  else if(chain_advance_state == AccelerateChain) {
    if(millis() - timer >= CHAIN_ACCEL_STEP_MS) {
      // Add the wait time rather than setting timer equal to millis()
      // This keeps the overall acceleration time pretty constant, even if we miss a cycle
      timer += CHAIN_ACCEL_STEP_MS;

      target_chain_speed += CHAIN_ACCEL_STEP_VAL;
      if(target_chain_speed >= CHAIN_MAX_DC_VAL) {
        target_chain_speed = CHAIN_MAX_DC_VAL;

        timer = millis();
        chain_advance_state = WaitCruiseTime;
      }
      s_set_chain_speed(target_chain_speed);
    }
  }

  else if(chain_advance_state == WaitCruiseTime) {
    // Technically we should subtract one accel step time because when we go to decelerate, we wait a step time before changing the motor speed
    // But eh. Step time should be small compared to the cruise time
    if(millis() - timer >= CHAIN_CRUISE_TIME_MS) {
      timer = millis();
      chain_advance_state = DecelerateChain;
    }
  }

  else if(chain_advance_state == DecelerateChain) {
    if(millis() - timer >= CHAIN_ACCEL_STEP_MS) {
      timer += CHAIN_ACCEL_STEP_MS;

      target_chain_speed -= CHAIN_ACCEL_STEP_VAL;
      if(target_chain_speed <= CHAIN_MIN_DC_VAL) {
        target_chain_speed = CHAIN_MIN_DC_VAL;

        timer = millis();
        chain_advance_state = WaitSustainTime;
      }
      s_set_chain_speed(target_chain_speed);
    }
  }

  else if(chain_advance_state == WaitSustainTime) {
    // In the normal case, the endstop gets triggered sometime while we're waiting in this state
    // That's handled near the top of this state machine, and it stops the motor and jumps to WaitSettleTime
    // If we time out here, that indicates the endstop didn't trigger, so we just reset things anyway

    if(millis() - timer >= sustain_time) {
      s_set_chain_speed(0);
      chain_advance_state = ChainIsReady;
    }
  }

  else if(chain_advance_state == WaitSettleTime) {
    if(millis() - timer > CHAIN_SETTLE_TIME_MS) {
      chain_advance_state = ChainIsReady;
    }
  }
}

static LoaderTypeEnum s_get_attached_loader(void) {
  delay(50); // Make sure ID voltage has stabilized
  uint32_t id_pin_mv = analogRead(LOADER_ID_PIN);
  id_pin_mv *= AREF_MV;
  id_pin_mv /= ADC_MAX_VAL;

       if(s_mv_matches_id_val(id_pin_mv, NONE_LOADER_ID_MV )) { return NoneLoader;  }
  else if(s_mv_matches_id_val(id_pin_mv, MAG_LOADER_ID_MV  )) { return MagLoader;   }
  else if(s_mv_matches_id_val(id_pin_mv, CHAIN_LOADER_ID_MV)) { return ChainLoader; }
  else {
    error("Could not detect loader type");
    return NoneLoader; // Needed to prevent compilation errors, even though we never return from error()
  }
}

static int s_mv_matches_id_val(int measured_mv, int id_mv) {
  int max_val = id_mv + ID_MV_TOLERANCE;
  int min_val = id_mv - ID_MV_TOLERANCE;

  if(min_val < measured_mv && measured_mv < max_val) { return 1; }
  return 0;
}
