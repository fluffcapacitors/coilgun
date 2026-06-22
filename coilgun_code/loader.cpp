
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
// 7.5k = 1414mV
// 15k  = 1980mV
// 39k  = 2627mV

#define AREF_MV 3300
#define ADC_MAX_VAL 1023
#define ID_MV_TOLERANCE 200

#define NONE_LOADER_ID_MV  3200
#define MAG_LOADER_ID_MV   595
#define CHAIN_LOADER_ID_MV 1414 // Using 7.5k pulldown resistor since 6.8k wasn't available

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
#define CHAIN_LOADED_PIN   LOADER_IO_0_PIN
#define CHAIN_LOADED_LEVEL HIGH // Outputs high when the endstop is pressed
#define CHAIN_IS_LOADED    (digitalReadFast(CHAIN_LOADED_PIN) == CHAIN_LOADED_LEVEL)
#define CHAIN_DRIVE_PIN    LOADER_IO_1_PIN
#define CHAIN_PWM_FREQ     8789.062F // Supposedly the precise frequency needed to get a perfect 12 bits resolution (Teensy 3.2, 72MHz)
#define CHAIN_PWM_BITS     12
#define CHAIN_PWM_MAX_VAL  4095L // L instead of UL to prevent compilation warnings about comparing signed and unsigned integers (target_chain_speed is signed)
// =============== Tweak these values ===============
#define CHAIN_SLOW_SPEED_PERCENT 45 // Duty cycle, speed for manual movement
#define CHAIN_FAST_SPEED_PERCENT 60 // Duty cycle, speed for normal advance
#define CHAIN_ADVANCE_TIME_MS    1000UL // Max amount of time the chain will move (at high speed) to hit the switch
#define CHAIN_OVERDRIVE_TIME_MS  0 // After the switch is hit, keep going at low speed a bit to make sure it's in the right spot
#define CHAIN_SETTLE_TIME_MS     100 // After stopping the chain, wait for things to settle before thwacking again
// ==================================================
#define CHAIN_SLOW_DC_VAL ((CHAIN_PWM_MAX_VAL * CHAIN_SLOW_SPEED_PERCENT) / 100) // Duty cycle timer value for low speed
#define CHAIN_FAST_DC_VAL ((CHAIN_PWM_MAX_VAL * CHAIN_FAST_SPEED_PERCENT) / 100) // Duty cycle timer value for high speed
// Time to wait after turning off the thwacker before starting to move the chain again
// Unlike the mag loader, we're not waiting for dowels to drop via gravity, we just need to guarantee the thwacker is retracted before moving the chain
// We also want to make sure the shot completed and OLED updated, and the endstop untriggers
#define CHAIN_MIN_THWACKER_OFF_TIME_MS 100


typedef enum {
  ManualChainAdvance,
  FastChainAdvance
} ChainAdvanceTypeEnum;

typedef enum {
  ChainIsReady,
  InitChainAdvance,
  WaitForThwacker,
  WaitAdvanceTime,
  WaitOverdriveTime,
  WaitSettleTime,
  ManualMove
} ChainAdvanceStateEnum;


static LoaderTypeEnum loader = NoneLoader;

static ChainAdvanceTypeEnum chain_advance_type = FastChainAdvance;
static ChainAdvanceStateEnum chain_advance_state = ChainIsReady;
static uint32_t current_chain_speed = 0;

static void s_trigger_chain_advance(ChainAdvanceTypeEnum);
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
    // "Ignore Loaded" switch is used to manually force-advance the motor, regardless if the loaded switch is hit
    // Stays active as long as the fire button is held down
    if(switch_is_active(IgnoreLoadedSwitch)) {
      s_trigger_chain_advance(ManualChainAdvance);
    }
    // Normal mode
    else {
      if((switch_is_active(NoThwackerSwitch) == 0) && CHAIN_IS_LOADED) {
        allow_coilgun_firing(AutoLoading);
        fire_thwacker();
      }
      s_trigger_chain_advance(FastChainAdvance);
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


static void s_trigger_chain_advance(ChainAdvanceTypeEnum advance_type) {
  chain_advance_type = advance_type;
  chain_advance_state = InitChainAdvance;
  s_tick_chain_advance();
}

static void s_set_chain_speed(uint32_t speed) {
  analogWrite(CHAIN_DRIVE_PIN, speed);
  current_chain_speed = speed;
}

static void s_tick_chain_advance(void) {
  static uint32_t timer = 0;

  if(chain_advance_state == ChainIsReady) {
    s_set_chain_speed(0);
    return;
    // Endstop may or may not be triggered
  }

  if(chain_advance_state == InitChainAdvance) {
    chain_advance_state = WaitForThwacker;
  }

  else if(chain_advance_state == WaitForThwacker) {
    // Once thwacker has been off for long enough, we can start the motor
    if(thwacker_off_time() >= CHAIN_MIN_THWACKER_OFF_TIME_MS) {
      // Force move motor
      if(chain_advance_type == ManualChainAdvance) {
        s_set_chain_speed(CHAIN_SLOW_DC_VAL);
        chain_advance_state = ManualMove;
      }
      // Otherwise we need to be checking the loaded switch
      else {
        // This shouldn't happen unless there's a jam (switch shouldn't be pressed after firing the dowel)
        if(CHAIN_IS_LOADED) {
          chain_advance_state = ChainIsReady;
        }
        else {
          s_set_chain_speed(CHAIN_FAST_DC_VAL);
          timer = millis();
          chain_advance_state = WaitAdvanceTime;
        }
      }
    }
  }

  else if(chain_advance_state == WaitAdvanceTime) {
    // Switch was hit, move to waiting for the overdrive time
    if(CHAIN_IS_LOADED) {
      chain_advance_state = WaitOverdriveTime;
      timer = millis();
    }
    // Moved the max amount of time without hitting the switch, turn off motor and reset
    else if(millis() - timer >= CHAIN_ADVANCE_TIME_MS) {
      s_set_chain_speed(0);
      chain_advance_state = ChainIsReady;
    }
  }

  else if(chain_advance_state == WaitOverdriveTime) {
    if(millis() - timer >= CHAIN_OVERDRIVE_TIME_MS) {
      s_set_chain_speed(0);
      chain_advance_state = WaitSettleTime;
    }
  }

  else if(chain_advance_state == WaitSettleTime) {
    if(millis() - timer > CHAIN_SETTLE_TIME_MS) {
      chain_advance_state = ChainIsReady;
    }
  }

  else if(chain_advance_state == ManualMove) {
    // Motor was turned on already, keep it going until the fire button is released
    if(switch_is_active(FireButton) == 0) {
      s_set_chain_speed(0);
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
