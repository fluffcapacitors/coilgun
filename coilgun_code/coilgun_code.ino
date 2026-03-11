// Teensy 3.2, 72MHz, USB Serial

// TODO:
// More opto checks prior to firing (make sure no unexpected optos are triggered)
// Maximum overall shot time safety check? (might be redundant because coil on-time will get triggered if shot somehow doesn't finish)

#define NUM_OPTOS_COILS 3

#define SAFETY_PIN 10
#define SAFETY_ENABLED digitalReadFast(SAFETY_PIN)

#define FIRE_BTN_PIN 12
#define FIRE_BTN_PRESSED digitalReadFast(FIRE_BTN_PIN)

#define GRN_LED_PIN 11
#define ORG_LED_PIN 13
#define GRN_LED_ON  digitalWrite(GRN_LED_PIN, HIGH)
#define GRN_LED_OFF digitalWrite(GRN_LED_PIN, LOW )
#define ORG_LED_ON  digitalWrite(ORG_LED_PIN, HIGH)
#define ORG_LED_OFF digitalWrite(ORG_LED_PIN, LOW )

#define SAFETY_TIMER_INTERVAL_US 100
#define MAX_COIL_ON_TIME_US 10000UL


typedef enum {
  RESET_COILS,
  WAIT_FOR_FIRST_OPTO,
  FIRE_NEXT_COIL,
  WAIT_FOR_OPTO_TRIGGER,
  WAIT_FOR_OPTO_UNTRIGGER
} CoilStateEnum;


// Must be in firing order (0th index is the first opto/coil)
static const int opto_pins[NUM_OPTOS_COILS] = {0, 1, 2};
static const int coil_pins[NUM_OPTOS_COILS] = {3, 4, 5};

static IntervalTimer safety_timer;
static uint8_t fire_btn_state = 0;


static void tick_coilgun_fire(void);
static void tick_fire_btn_read(void);
static int  fire_btn_held(void);
static int  opto_triggered(uint8_t);
static void turn_coil_on(uint8_t);
static void turn_coils_off(void);
static int  coil_is_on(void);
static void error(const char *);
static void safety_callback(void);


void setup() {
  for(uint8_t i = 0; i < NUM_OPTOS_COILS; i++) {
    pinMode(opto_pins[i], INPUT);
    pinMode(coil_pins[i], OUTPUT);
  }
  turn_coils_off();

  pinMode(SAFETY_PIN,   INPUT);
  pinMode(FIRE_BTN_PIN, INPUT);
  pinMode(GRN_LED_PIN, OUTPUT);
  pinMode(ORG_LED_PIN, OUTPUT);
  
  safety_timer.begin(safety_callback, SAFETY_TIMER_INTERVAL_US);
}

void loop() {
  tick_fire_btn_read();
  tick_coilgun_fire();
}

static void tick_coilgun_fire(void) {
  static CoilStateEnum state = RESET_COILS;
  static uint8_t active_coil = 0;

  if(fire_btn_held() == 0 || SAFETY_ENABLED == 0) {
    state = RESET_COILS;
  }

  if(state == RESET_COILS) {
    turn_coils_off();
    active_coil = 0;

    state = WAIT_FOR_FIRST_OPTO;
  }

  else if(state == WAIT_FOR_FIRST_OPTO) {
    if(opto_triggered(0)) {
      state = FIRE_NEXT_COIL;
    }
  }

  else if(state == FIRE_NEXT_COIL) {
    if(active_coil >= NUM_OPTOS_COILS) { error("State machine tried to fire too many coils"); }

    turn_coils_off();
    //delayMicroseconds(100); // Make sure MOSFET has at least started to turned off
    turn_coil_on(active_coil);

    state = WAIT_FOR_OPTO_TRIGGER;
  }

  else if(state == WAIT_FOR_OPTO_TRIGGER) {
    if(opto_triggered(active_coil)) {
      state = WAIT_FOR_OPTO_UNTRIGGER;
    }
  }

  else if(state == WAIT_FOR_OPTO_UNTRIGGER) {
    if(!opto_triggered(active_coil)) {
      active_coil++;
      if(active_coil >= NUM_OPTOS_COILS) {
        state = RESET_COILS;
      }
      else {
        state = FIRE_NEXT_COIL;
      }
    }
  }
}

static void tick_fire_btn_read(void) {
  static uint32_t timer = 0;
  static uint32_t btn_bits = 0; // 32 bits, 32ms debounce time

  if(millis() != timer) { // Every millisecond
    timer = millis(); 

    btn_bits <<= 1;
    if(FIRE_BTN_PRESSED) { btn_bits |= 1; }

    if(btn_bits == 0xFFFFFFFF) { fire_btn_state = 1; }
    if(btn_bits == 0x00000000) { fire_btn_state = 0; }
  }
}

static int fire_btn_held(void) {
  return fire_btn_state;
}

static int opto_triggered(uint8_t opto) {
  if(opto >= NUM_OPTOS_COILS) { error("Opto out of range"); }

  return !digitalReadFast(opto_pins[opto]); // Optos are active low
}

static void turn_coil_on(uint8_t coil) {
  if(coil >= NUM_OPTOS_COILS) { error("Coil out of range"); }

  digitalWriteFast(coil_pins[coil], HIGH);
}

static void turn_coils_off(void) {
  for(uint8_t i = 0; i < NUM_OPTOS_COILS; i++) {
    digitalWriteFast(coil_pins[i], LOW);
  }
}

static int coil_is_on(uint8_t coil) {
  if(coil >= NUM_OPTOS_COILS) { error("Coil out of range"); }

  return digitalReadFast(coil_pins[coil]);
}

static void error(const char *err_msg) {
  turn_coils_off();
  ORG_LED_ON;
  GRN_LED_OFF;

  if(err_msg != NULL) {
    Serial.print("ERROR: ");
    Serial.println(err_msg);
  }

  while(1);
}

// Timer interrupt callback, gets called constantly to independently check stuff and shut down if things go wrong
static void safety_callback(void) {
  static uint32_t coil_on_time_us[NUM_OPTOS_COILS] = {0};

  GRN_LED_ON;

  uint8_t num_triggered_optos = 0;

  for(uint8_t i = 0; i < NUM_OPTOS_COILS; i++) {
    if(coil_is_on(i)) {
      coil_on_time_us[i] += SAFETY_TIMER_INTERVAL_US;
      if(coil_on_time_us[i] >= MAX_COIL_ON_TIME_US) {
        error("Max coil on time reached");
      }
    }
    else {
      coil_on_time_us[i] = 0;
    }

    if(opto_triggered(i)) {
      num_triggered_optos++;
    }
  }

  if(num_triggered_optos > 1) { error("Multiple optos triggered"); }

  GRN_LED_OFF;
}
