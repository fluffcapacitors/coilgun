
#include "cg_eeprom.h"
#include "oled.h"
#include "pins.h"
#include "switches.h"
#include <U8g2lib.h>
#include <Wire.h>


// Updating the display takes about 40ms


#define LARGE_FONT u8g2_font_helvR24_tr
#define MED_FONT   u8g2_font_helvB12_tr
#define SMALL_FONT u8g2_font_helvR10_tr
#define MENU_FONT  u8g2_font_helvR08_tr

#define ALIGN_LEFT   0
#define ALIGN_CENTER 1
#define ALIGN_RIGHT  2

#define X_CENTER_COORD (u8g2.getDisplayWidth() / 2)
#define X_RIGHT_COORD  (u8g2.getDisplayWidth() - 1)
#define Y_BOTTOM_COORD (u8g2.getDisplayHeight() - 1)

#define ICON_W(X)         X[0]
#define ICON_H(X)         X[1]
#define ICON_DATA_PTR(X) &X[2]


U8G2_SSD1309_128X64_NONAME0_F_HW_I2C u8g2(U8G2_R0, OLED_RST_PIN, OLED_SCL_PIN, OLED_SDA_PIN);


static const uint8_t empty_circle_icon[] = {
  8, 8,
  0b00111100,
  0b01000010,
  0b10000001,
  0b10000001,
  0b10000001,
  0b10000001,
  0b01000010,
  0b00111100
};

static const uint8_t filled_circle_icon[] = {
  8, 8,
  0b00111100,
  0b01111110,
  0b11111111,
  0b11111111,
  0b11111111,
  0b11111111,
  0b01111110,
  0b00111100
};


static void s_draw_str(uint16_t, uint16_t, uint8_t, const uint8_t *, const char *);
static void s_draw_wrapped_str(int, int, const uint8_t *, const char *);
static void s_draw_num(uint16_t, uint16_t, uint8_t, const uint8_t *, int);
static void s_draw_icon(uint16_t, uint16_t, const uint8_t *);
static void s_draw_oled_menu_item(uint16_t, uint8_t, const char *);


void init_oled(void) {
  u8g2.setBusClock(400000);
  u8g2.begin();
}

void refresh_oled(void) {
  u8g2.clearBuffer();

  s_draw_str(X_CENTER_COORD, 14, ALIGN_CENTER, MED_FONT,   "SHOTS TODAY");
  s_draw_num(X_CENTER_COORD, 45, ALIGN_CENTER, LARGE_FONT, get_shot_odometer());

  char str[20];
  snprintf(str, 20, "TOTAL  %u", get_total_shots());
  s_draw_str(X_CENTER_COORD, Y_BOTTOM_COORD, ALIGN_CENTER, SMALL_FONT, str);

  // s_draw_str(0,             Y_BOTTOM_COORD, ALIGN_LEFT,  SMALL_FONT, "TOTAL:");
  // s_draw_num(X_RIGHT_COORD, Y_BOTTOM_COORD, ALIGN_RIGHT, SMALL_FONT, get_total_shots());
  
  u8g2.updateDisplay();
}

void oled_show_error(const char *err_msg) {
  init_oled(); // Reset and clear screen
  u8g2.setDisplayRotation(U8G2_R2); // Rotate screen 180 so we can read the error from behind the coilgun
  s_draw_wrapped_str(0, u8g2.getMaxCharHeight(), MENU_FONT, err_msg);
  u8g2.updateDisplay();
}

void enter_oled_menu(void) {
  static uint32_t timer = 0;

  while(1) {
    tick_switches(); // Need this or the switch states won't update

    if(fire_button_pressed()) { break; } // Save selections
    // Other way to exit is simply reset the MCU (discard selections)

    // Skip the rest of the code until it's time to update the display
    // We can't update the screen too often or else the switch updates get unresponsive because this takes so long
    if(millis() - timer < 100) { continue; } // 10 fps
    timer = millis();

    // Update the display to show the switch selections

    u8g2.clearBuffer();

    s_draw_oled_menu_item( 8, switch_is_active(DisableCoil0Switch), "Reset shot odometer");
    s_draw_oled_menu_item(19, switch_is_active(DisableCoil1Switch), "");
    s_draw_oled_menu_item(30, switch_is_active(DisableCoil2Switch), "");
    s_draw_oled_menu_item(41, switch_is_active(NoThwackerSwitch  ), "");
    s_draw_oled_menu_item(52, switch_is_active(IgnoreLoadedSwitch), "");

    s_draw_str(0,             Y_BOTTOM_COORD, ALIGN_LEFT,  MENU_FONT, "RESET:Cancel");
    s_draw_str(X_RIGHT_COORD, Y_BOTTOM_COORD, ALIGN_RIGHT, MENU_FONT, "FIRE:Save");

    u8g2.updateDisplay();
  }

  if(switch_is_active(DisableCoil0Switch)) { reset_shot_odometer(); }
  if(switch_is_active(DisableCoil1Switch)) { }
  if(switch_is_active(DisableCoil2Switch)) { }
  if(switch_is_active(NoThwackerSwitch  )) { }
  if(switch_is_active(IgnoreLoadedSwitch)) { }
  
  u8g2.clearBuffer();
  u8g2.updateDisplay();
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
}

// Thanks, LenShustek: https://github.com/olikraus/u8g2/discussions/1479
// display a string on multiple lines, keeping words intact where possible
static void s_draw_wrapped_str(int xloc, int yloc, const uint8_t *font, const char *msg) {
  u8g2.setFont(font);

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

static void s_draw_num(uint16_t x_coord, uint16_t y_coord, uint8_t align, const uint8_t *font, int num) {
  char str[20];
  itoa(num, str, 10);

  s_draw_str(x_coord, y_coord, align, font, str);
}

static void s_draw_icon(uint16_t x_coord, uint16_t y_coord, const uint8_t *icon) {
  // Bitmaps are drawn from top-left but I want their position defined by bottom-left corner,
  // so subtract the icon height from the Y coordinate
  u8g2.drawXBM(x_coord, y_coord - ICON_H(icon), ICON_W(icon), ICON_H(icon), ICON_DATA_PTR(icon));
}

static void s_draw_oled_menu_item(uint16_t y_coord, uint8_t state, const char *str) {
  const uint8_t *icon = empty_circle_icon;
  if(state) { icon = filled_circle_icon; }

  s_draw_icon(0, y_coord, icon);
  s_draw_str(ICON_W(icon) + 2, y_coord, ALIGN_LEFT, MENU_FONT, str);
}
