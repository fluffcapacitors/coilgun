
#include "cg_eeprom.h"
#include <EEPROM.h>

#define LIFETIME_SHOTS_ADDR 0 // Can't be reset
#define TRIP_A_SHOTS_ADDR   4 
#define TRIP_B_SHOTS_ADDR   8 
#define INFO_DISPLAY_ADDR   12 // What data to show on the display during normal operation
#define INVERT_ERROR_ADDR   13 // Whether to invert the error display
#define INVALID_ADDR        100

static uint16_t s_get_counter_addr(ShotCounterEnum);
static void     s_write_u8 (uint16_t, uint8_t);
static uint32_t s_read_u8  (uint16_t);
static void     s_write_u32(uint16_t, uint32_t);
static uint32_t s_read_u32 (uint16_t);


// void set_lifetime_shots(uint32_t shots) {
//   s_write_u32(s_get_counter_addr(LifetimeShots), shots);
// }

uint32_t get_shot_counter(ShotCounterEnum counter) {
  uint32_t lifetime_shots = s_read_u32(s_get_counter_addr(LifetimeShots));
  if(counter == LifetimeShots) { return lifetime_shots; }

  uint32_t trip_val = s_read_u32(s_get_counter_addr(counter));
  return lifetime_shots - trip_val;
}

void reset_shot_counter(ShotCounterEnum counter) {
  if(counter == LifetimeShots) { return; } // Can't reset lifetime

  // Reset a trip by setting it equal to the current lifetime shots
  uint32_t lifetime_shots = get_shot_counter(LifetimeShots);
  s_write_u32(s_get_counter_addr(counter), lifetime_shots);
}

void increment_shots_fired(void) {
  uint32_t new_lifetime_shots = get_shot_counter(LifetimeShots) + 1;
  s_write_u32(s_get_counter_addr(LifetimeShots), new_lifetime_shots);
}

void set_info_display(InfoDisplayEnum info_type) {
  s_write_u8(INFO_DISPLAY_ADDR, (uint8_t)info_type);
}

InfoDisplayEnum get_info_display(void) {
  uint8_t tmp = s_read_u8(INFO_DISPLAY_ADDR);
  switch(tmp) {
    case 1: return ShotsTodayInfo;
    case 2: return TotalShotsInfo;

    default: return InvalidInfo;
  }
}

uint8_t error_display_inverted(void) {
  return s_read_u8(INVERT_ERROR_ADDR);
}

void set_inverted_error_display(uint8_t invert) {
  s_write_u8(INVERT_ERROR_ADDR, invert);
}


static uint16_t s_get_counter_addr(ShotCounterEnum counter) {
    switch(counter) {
      case LifetimeShots: return LIFETIME_SHOTS_ADDR;
      case TripAShots:    return TRIP_A_SHOTS_ADDR;
      case TripBShots:    return TRIP_B_SHOTS_ADDR;
  }
  return INVALID_ADDR;
}

static void s_write_u8(uint16_t addr, uint8_t data) {
  EEPROM.update(addr, data);
}

static uint32_t s_read_u8(uint16_t addr) {
  return EEPROM.read(addr);
}

static void s_write_u32(uint16_t addr, uint32_t data) {
  EEPROM.update(addr + 0, data >> 24);
  EEPROM.update(addr + 1, data >> 16);
  EEPROM.update(addr + 2, data >> 8 );
  EEPROM.update(addr + 3, data >> 0 );
}

static uint32_t s_read_u32(uint16_t addr) {
  uint32_t tmp = 0;

  tmp |= EEPROM.read(addr + 0); tmp <<= 8;
  tmp |= EEPROM.read(addr + 1); tmp <<= 8;
  tmp |= EEPROM.read(addr + 2); tmp <<= 8;
  tmp |= EEPROM.read(addr + 3);

  return tmp;
}
