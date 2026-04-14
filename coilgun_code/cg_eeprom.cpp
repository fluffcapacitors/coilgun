
#include "cg_eeprom.h"
#include <EEPROM.h>


#define TOTAL_SHOTS_ADDR    0 // Lifetime shots fired
#define SHOTS_ODOMETER_ADDR 4 // When reset, gets set equal to total shots. Then subtract the two to get shots since reset
#define INFO_DISPLAY_ADDR   8 // What data to show on the display during normal operation


static void     s_write_u32(uint16_t, uint32_t);
static uint32_t s_read_u32 (uint16_t);


uint32_t get_total_shots(void) {
  return s_read_u32(TOTAL_SHOTS_ADDR);
}

void increment_total_shots(void) {
  uint32_t new_total_shots = get_total_shots() + 1;
  s_write_u32(TOTAL_SHOTS_ADDR, new_total_shots);
}

uint32_t get_shot_odometer(void) {
  uint32_t total_shots = get_total_shots();
  uint32_t odometer = s_read_u32(SHOTS_ODOMETER_ADDR);

  return total_shots - odometer;
}

void reset_shot_odometer(void) {
  s_write_u32(SHOTS_ODOMETER_ADDR, get_total_shots());
}

void set_info_display(InfoDisplayEnum info_type) {
  EEPROM.update(INFO_DISPLAY_ADDR, (uint8_t)info_type);
}

InfoDisplayEnum get_info_display(void) {
  uint8_t tmp = EEPROM.read(INFO_DISPLAY_ADDR);
  switch(tmp) {
    case 1: return ShotsTodayInfo;
    case 2: return TotalShotsInfo;

    default: return InvalidInfo;
  }
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
