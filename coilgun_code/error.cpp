
#include "coilgun.h"
#include "error.h"
#include "pins.h"
#include "thwacker.h"


void error(const char *err_msg) {
  turn_coils_off();
  turn_thwacker_off();

  pinMode(ORG_LED_PIN, OUTPUT);
  digitalWriteFast(ORG_LED_PIN, HIGH);

  if(err_msg != NULL) {
    Serial.begin(115200);
    Serial.print("ERROR: ");
    Serial.println(err_msg);
  }

  while(1);
}
