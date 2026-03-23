
#include "pins.h"
#include "switches.h"


// Must match the enum in switches.h
static const int switch_pins[NUM_SWITCHES] = {
  SAFETY_SW_PIN,
  FIRE_BTN_PIN,
  NO_THWACKER_SW_PIN,
  IGNORE_LOADED_SW_PIN,
  THREE_SHOT_SW_PIN,
  DISABLE_COIL_0_SW_PIN,
  DISABLE_COIL_1_SW_PIN,
  DISABLE_COIL_2_SW_PIN
};

static int switch_states[NUM_SWITCHES] = {0};
static int fire_btn_pressed_flag = 0;


void init_switches(void) {
  // All switches here have pulldown resistors enabled.
  // If the control pendant is disconnected, the pins should keep a disabled state
  for(int i = 0; i < NUM_SWITCHES; i++) {
    pinMode(switch_pins[i], INPUT_PULLDOWN);
  }
}

void tick_switches(void) {
  static uint32_t check_timer = 0;
  static uint32_t switch_bits[NUM_SWITCHES] = {0}; // 32 bits, 32ms debounce time

  static int prev_fire_btn_state = 0;

  // Every millisecond
  if(millis() != check_timer) {
    check_timer = millis(); 

    // Read each switch and update its current state
    for(int i = 0; i < NUM_SWITCHES; i++) {
      switch_bits[i] <<= 1;
      if(digitalReadFast(switch_pins[i])) { switch_bits[i] |= 1; }

      if(switch_bits[i] == 0xFFFFFFFF) { switch_states[i] = 1; }
      if(switch_bits[i] == 0x00000000) { switch_states[i] = 0; }
    }

    // Check if fire button was pressed (pin had a rising edge)
    if(prev_fire_btn_state == 0 && switch_is_active(FireButton)) {
      fire_btn_pressed_flag = 1;
    }
    prev_fire_btn_state = switch_is_active(FireButton);
  }
}

int switch_is_active(SwitchesEnum sw) {
  if(sw >= NUM_SWITCHES) { return 0; }

  return switch_states[sw];
}

int safety_is_on(void) {
  return !switch_is_active(SafetySwitch); // Safety is "on" when the switch is off
}

int fire_button_pressed(void) {
  uint8_t tmp = fire_btn_pressed_flag;
  fire_btn_pressed_flag = 0;

  return tmp;
}
