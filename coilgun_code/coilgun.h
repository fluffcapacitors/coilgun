
#ifndef COILGUN_H
#define COILGUN_H

#include <Arduino.h>


#define NUM_OPTOS_COILS 3


typedef enum {
  AutoLoading,
  ManualLoading
} LoadingTypeEnum;


void init_coilgun(void);
void tick_coilgun(void);

int  coilgun_is_idle(void);
void allow_coilgun_firing(LoadingTypeEnum);
int  first_opto_was_triggered(void);

void turn_coils_off(void);
int  coil_is_on(uint8_t);

int num_optos_triggered(void);


#endif
