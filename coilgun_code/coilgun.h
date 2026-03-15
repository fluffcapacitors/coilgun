
#ifndef COILGUN_H
#define COILGUN_H

#include <Arduino.h>


#define NUM_OPTOS_COILS 3


void init_coilgun(void);
void tick_coilgun(void);

int  coilgun_is_idle(void);
void enable_coilgun_firing(void);

void turn_coils_off(void);
int  coil_is_on(uint8_t);
int  num_optos_triggered(void);


#endif
