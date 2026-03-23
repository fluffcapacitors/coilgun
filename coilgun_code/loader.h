
#ifndef LOADER_H
#define LOADER_H

#include <Arduino.h>


void init_loader(void);
void tick_loader(void);

int loader_is_ready(void);
void fire_loader(void);


#endif
