
#ifndef THWACKER_H
#define THWACKER_H

#include <Arduino.h>


void init_thwacker(void);
void tick_thwacker(void);

void fire_thwacker(void);
void turn_thwacker_off(void);
uint32_t thwacker_off_time(void);
int thwacker_is_on(void);


#endif
