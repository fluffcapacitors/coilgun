
#ifndef OLED_H
#define OLED_H

#include <Arduino.h>


void init_oled(void);

void refresh_oled(void);
void oled_show_error(const char *);

void oled_show_startup_screen(void);
void enter_oled_menu(void);


#endif
