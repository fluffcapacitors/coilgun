
#ifndef CG_EEPROM_H
#define CG_EEPROM_H

#include <Arduino.h>


typedef enum {
  InvalidInfo    = 0,
  ShotsTodayInfo = 1,
  TotalShotsInfo = 2,
} InfoDisplayEnum;


uint32_t get_total_shots(void);
void increment_total_shots(void);

uint32_t get_shots_today(void);
void reset_shots_today(void);

void set_info_display(InfoDisplayEnum);
InfoDisplayEnum get_info_display(void);

uint8_t error_display_inverted(void);
void set_inverted_error_display(uint8_t);


#endif
