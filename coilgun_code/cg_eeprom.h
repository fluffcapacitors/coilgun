
#ifndef CG_EEPROM_H
#define CG_EEPROM_H

#include <Arduino.h>


typedef enum {
  LifetimeShots,
  TripAShots,
  TripBShots
} ShotCounterEnum;

typedef enum {
  InvalidInfo    = 0,
  ShotsTodayInfo = 1,
  TotalShotsInfo = 2,
} InfoDisplayEnum;


// void set_lifetime_shots(uint32_t);

uint32_t get_shot_counter(ShotCounterEnum);
void reset_shot_counter(ShotCounterEnum);
void increment_shots_fired(void);

void set_info_display(InfoDisplayEnum);
InfoDisplayEnum get_info_display(void);

uint8_t error_display_inverted(void);
void set_inverted_error_display(uint8_t);


#endif
