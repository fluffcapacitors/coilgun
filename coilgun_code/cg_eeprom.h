
#ifndef CG_EEPROM_H
#define CG_EEPROM_H

#include <Arduino.h>


uint32_t get_total_shots(void);
void increment_total_shots(void);

uint32_t get_shot_odometer(void);
void reset_shot_odometer(void);


#endif
