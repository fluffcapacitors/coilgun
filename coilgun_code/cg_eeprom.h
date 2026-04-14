
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

uint32_t get_shot_odometer(void);
void reset_shot_odometer(void);

void set_info_display(InfoDisplayEnum);
InfoDisplayEnum get_info_display(void);


#endif
