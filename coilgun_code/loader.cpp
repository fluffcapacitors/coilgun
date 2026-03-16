
#include "error.h"
#include "loader.h"
#include "switches.h"
#include "pins.h"
#include "thwacker.h"


// ID resistor pullup should be 10k. If 48V shorted to it, it would only inject 5mA into the 3.3V line (no problem)
// Voltages with pulldown:
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

#define MAG_MIN_THWACKER_OFF_TIME_MS 250 // Essentially, minimum time between shots for magazine loader


static LoaderTypeEnum loader = NoneLoader;


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
    pinMode(LOADER_IO_0_PIN, INPUT_PULLUP); // Digital output of IR sensor (active low)
    // pinMode(LOADER_IO_1_PIN, INPUT); // Analog output of IR sensor
    // IO 1 currently unused
  }

  else if(loader == ChainLoader) {

  }
}

void tick_loader(void) {
  if(loader == NoneLoader) {

  }

  else if(loader == MagLoader) {
    
  }

  else if(loader == ChainLoader) {

  }
}

int loader_is_ready(void) {
  if(loader == NoneLoader) {
    return 1; // Always loaded (no sensing circuitry)
  }

  else if(loader == MagLoader) {
    // Once the thwacker turns off, it takes time for it to retract, and for the next dowel to drop into place
    // We're definitely not ready unless that minimum time has passed
    if(thwacker_off_time() < MAG_MIN_THWACKER_OFF_TIME_MS) { return 0; }
    // Otherwise we check the IR sensor, unless the override switch is on
    if(switch_is_active(IgnoreLoadedSwitch)) { return 1; }
    // IR proximity sensor, outputs low when something detected
    if(digitalReadFast(LOADER_IO_0_PIN) == LOW) { return 1; }

    return 0;
  }

  else if(loader == ChainLoader) {
    return 0;
  }

  return 0;
}

void fire_loader(void) {
  if(safety_is_on()) { return; }

  if(loader == NoneLoader) {

  }

  else if(loader == MagLoader) {
    fire_thwacker();
  }

  else if(loader == ChainLoader) {

  }
}

LoaderTypeEnum current_loader(void) {
  return loader;
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
