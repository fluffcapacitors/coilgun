
#ifndef LOADER_H
#define LOADER_H

#include <Arduino.h>


typedef enum {
  NoneLoader,
  MagLoader,
  ChainLoader
} LoaderTypeEnum;


void init_loader(void);
void tick_loader(void);

int loader_is_ready(void);
void fire_loader(void);

LoaderTypeEnum current_loader(void);


#endif
