
#include "cg_eeprom.h"
#include "oled.h"
#include "pins.h"
#include <U8g2lib.h>
#include <Wire.h>


#define LARGE_FONT u8g2_font_helvR24_tr
#define SMALL_FONT u8g2_font_helvR14_tr

#define ALIGN_LEFT   0
#define ALIGN_CENTER 1
#define ALIGN_RIGHT  2

#define X_CENTER_COORD (u8g2.getDisplayWidth() / 2)
#define X_RIGHT_COORD  (u8g2.getDisplayWidth() - 1)


U8G2_SSD1309_128X64_NONAME0_F_HW_I2C u8g2(U8G2_R0, OLED_RST_PIN, OLED_SCL_PIN, OLED_SDA_PIN);


static void s_draw_str(uint16_t, uint16_t, uint8_t, const uint8_t *, const char *);
static void s_draw_num(uint16_t, uint16_t, uint8_t, const uint8_t *, int);


void init_oled(void) {
  u8g2.begin();
  u8g2.clear();
}

void refresh_oled(void) {
  u8g2.clear();

  s_draw_str(X_CENTER_COORD, 14, ALIGN_CENTER, SMALL_FONT, "SHOTS TODAY");
  s_draw_num(X_CENTER_COORD, 44, ALIGN_CENTER, LARGE_FONT, get_shot_odometer());

  // char str[20];
  // snprintf(str, 20, "Total shots: %u", get_total_shots());
  // s_draw_str(X_CENTER_COORD, 63, ALIGN_CENTER, SMALL_FONT, str);

  s_draw_str(0,             63, ALIGN_LEFT,  SMALL_FONT, "Total shots:");
  s_draw_num(X_RIGHT_COORD, 63, ALIGN_RIGHT, SMALL_FONT, get_total_shots());
}

// Thanks, LenShustek: https://github.com/olikraus/u8g2/discussions/1479
// display a string on multiple lines, keeping words intact where possible
void oled_show_error(const char *msg) {
  init_oled(); // Reset and clear screen
  u8g2.setDisplayRotation(U8G2_R2); // Rotate screen 180 so we can read the error from behind the coilgun
  u8g2.setFont(SMALL_FONT);
  int xloc = 0;
  int yloc = u8g2.getMaxCharHeight();

  int dspwidth = u8g2.getDisplayWidth(); // display width in pixels
  int strwidth = 0; // string width in pixels
  char glyph[2]; glyph[1] = 0;

  for (const char *ptr = msg, *lastblank = NULL; *ptr; ++ptr) {
    while (xloc == 0 && *msg == ' ')
      if (ptr == msg++) ++ptr; // skip blanks at the left edge

    glyph[0] = *ptr;
    strwidth += u8g2.getStrWidth(glyph); // accumulate the pixel width
    if (*ptr == ' ')  lastblank = ptr; // remember where the last blank was
    else ++strwidth; // non-blanks will be separated by one additional pixel

    if (xloc + strwidth > dspwidth) { // if we ran past the right edge of the display
      int starting_xloc = xloc;
      while (msg < (lastblank ? lastblank : ptr)) { // print to the last blank, or a full line
        glyph[0] = *msg++;
        xloc += u8g2.drawStr(xloc, yloc, glyph); 
      }

      strwidth -= xloc - starting_xloc; // account for what we printed
      yloc += u8g2.getMaxCharHeight(); // advance to the next line
      xloc = 0; lastblank = NULL;
    }
  }
  while (*msg) { // print any characters left over
    glyph[0] = *msg++;
    xloc += u8g2.drawStr(xloc, yloc, glyph);
  }
}


static void s_draw_str(uint16_t x_coord, uint16_t y_coord, uint8_t align, const uint8_t *font, const char *str) {
  u8g2.setFont(font);

  uint16_t text_w = u8g2.getStrWidth(str);

  // Center text on X coordinate
  if(align == ALIGN_CENTER) {
    x_coord -= text_w / 2;
  }
  // Align right side of text with the X coordinate
  else if(align == ALIGN_RIGHT) {
    x_coord -= text_w;
  }
  // If ALIGN_LEFT, don't need to do anything

  u8g2.drawStr(x_coord, y_coord, str);
  // u8g2.setCursor(x_coord, y_coord);
  // u8g2.print(str);
}

static void s_draw_num(uint16_t x_coord, uint16_t y_coord, uint8_t align, const uint8_t *font, int num) {
  char str[20];
  itoa(num, str, 10);

  s_draw_str(x_coord, y_coord, align, font, str);
}
