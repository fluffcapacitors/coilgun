
#include "cg_eeprom.h"
#include <EEPROM.h>

#define TOTAL_SHOTS_ADDR  0 // Lifetime shots fired
#define SHOTS_TODAY_ADDR  4 // When reset, gets set equal to total shots. Then subtract the two to get shots since reset
#define INFO_DISPLAY_ADDR 8 // What data to show on the display during normal operation
#define INVERT_ERROR_ADDR 9 // Whether to invert the error display


static void     s_write_u8 (uint16_t, uint8_t);
static uint32_t s_read_u8  (uint16_t);
static void     s_write_u32(uint16_t, uint32_t);
static uint32_t s_read_u32 (uint16_t);


uint32_t get_total_shots(void) {
  return s_read_u32(TOTAL_SHOTS_ADDR);
}

void increment_total_shots(void) {
  uint32_t new_total_shots = get_total_shots() + 1;
  s_write_u32(TOTAL_SHOTS_ADDR, new_total_shots);
}

uint32_t get_shots_today(void) {
  uint32_t total_shots = get_total_shots();
  uint32_t odometer = s_read_u32(SHOTS_TODAY_ADDR);

  return total_shots - odometer;
}

void reset_shots_today(void) {
  s_write_u32(SHOTS_TODAY_ADDR, get_total_shots());
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
