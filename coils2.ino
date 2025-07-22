#include <stdio.h>

//currently one sensor in front of each coil
#define FIRST_SENSOR_PIN 0
#define SENSOR_COUNT 3

//COIL_COUNT is assumed to match SENSOR_COUNT
#define FIRST_COIL_PIN 3
#define COIL_COUNT 3

#define FIRE_PIN 12

#define POLL_TIME 31
//#define MAXIMUM_SAFE_ON_MILLISECONDS 5000
//#define MAXIMUM_ON_COUNT (MAXIMUM_SAFE_ON_MILLISECONDS*1000 / POLL_TIME)

//MAX_COIL_ON_TIME / POLL_TIME = number of polling cycles 
#define MAX_COIL_ON_TIME 10000 //time in usec


IntervalTimer myTimer;

volatile bool errorFlag = true; //global to tell gun to turn off
volatile bool activeShot = false; //global to communicate if trigger is currently pressed
volatile bool softwareEnable = false;
volatile long activeShotTimer = 0;
volatile int coilIndex = 0; //to tell what coil in the sequence we're on (used to pre-charge next coil in line)
int shotComplete = 0;

char errorMessage[255] = "";
char msg[255] = "";

int debounce;

//this is intended to count how many cycles the coil has been on for
volatile long coilTimes[COIL_COUNT];
//this is intended to be a boolean array
volatile int activeCoils[COIL_COUNT];
long shotTimer = 0;


void setup()
{
  
  //setup sensors
  for(int i = 0; i < SENSOR_COUNT; i++)
  {
    pinMode(i+FIRST_SENSOR_PIN, INPUT);
  }
  //setup coils
  for(int i = 0; i < SENSOR_COUNT; i++)
  {
    pinMode(i+FIRST_COIL_PIN, OUTPUT);
  }
  pinMode(FIRE_PIN, INPUT);
  pinMode(10, INPUT);
  pinMode(11, OUTPUT);
  pinMode(13, OUTPUT);
  //1,000,000 = interrupt every second
  //31.25 microseconds per cycle = ~32khz
  myTimer.begin(timerCallback, POLL_TIME);
  errorFlag = false;

  for(int i = 0; i < COIL_COUNT; i++)
  {
    coilTimes[i] = 0;
    activeCoils[i] = 0;
    
  }

  for(int i =0; i < COIL_COUNT; i++)
  {
    activeCoils[i] = 0;
    coilTimes[i] = 0;
  }
}

void loop()
{
  //don't even bother checking the fire button if our software enable isn't on
  softwareEnable = digitalReadFast(10);

  if(softwareEnable) {
    debounceFireButton();
  }
  
  //error indicator light, built in LED on teensy 3.2/arduino
  if(errorFlag == true)
  {
    //error indicator light, built in LED on teensy 3.2/arduino
    digitalWrite(13, HIGH);
	  //Serial.println(errorMessage);
  }
  if(shotComplete == 1)
  {
    
    //this bit not currently accurate
    //Serial.print("shot complete! Duration: ");
    //Serial.println(activeShotTimer*POLL_TIME/1000);
    //~125mm traveled

    
    double totalShotTime = 0;
    for(int i = 0; i < COIL_COUNT; i++)
    {
      totalShotTime += ((coilTimes[i] * POLL_TIME) / 1000);
      sprintf(msg, "coil %i: %i milliseconds\n", i, ((coilTimes[i] * POLL_TIME) / 1000) );
      Serial.print(msg);
      sprintf(msg, "coil %i: %f meters/second\n", i, (61/((coilTimes[i] * POLL_TIME) / 1000)));
      Serial.print(msg);
      coilTimes[i] = 0;
    }
    
    //distance between sensors = 61mm
    sprintf(msg, "Approximate shot speed (m/sec): %f", (61*3)/totalShotTime);
    Serial.print(msg);
   
	  shotComplete = 0;
  }
}

void debounceFireButton()
{
  if(digitalReadFast(FIRE_PIN) && !activeShot)
  {
    if(debounce > 30000) {
      if(!errorFlag) {
        activeShot = true;
        activeShotTimer = 0;
        Serial.println("active shot!");
      }
      debounce = 0;
    }
    else {
      debounce++;
    }
  }
  else {
    debounce = 0;
  }
}

/* --------------------------------------------------------------------------------
 * returns 0 indexed sensor number for single active sensor
 * returns -1 for zero or multiple active sensors
 *  
 * Idea is to scan through each sensor input looking for a coil to activate on the
 * first active sensor found. If activeSensor is set to -1, we know it's the first.
 * If there's a second active sensor (there never should be) set an error flag and 
 * return the error value (-1)
 */
int getActiveSensor()
{
  int activeSensor = -1;

  //this for loop scans all sensors that are in use each time for two reasons:
  //1) so the function takes relativly constant time
  //2) as a safety measure to ensure that no sensor is broken
  //
  //TODO: better/faster way of doing this? (direct read 4 chunks of memory to get inputs?)
  //i = 0; i < 0+3; i++
  for(int i = FIRST_SENSOR_PIN; i < SENSOR_COUNT+FIRST_SENSOR_PIN; i++)
  {
	  //administrivia, track how long coil has been active
	  //TODO: check time and call emergency shutdown here?
	  //TODO: evaluate performance hit if this is made into
	  //a separate loop inside the 31khz callback
	  if(activeCoils[i] == 1)
	  {
	    //safety critical
	    if(coilTimes[i]++ * POLL_TIME > MAX_COIL_ON_TIME)
	    {
        //this if (but not contents of if) is for debug 
        if(!errorFlag)
        {
		      errorFlag = true;
		      sprintf(errorMessage, "coil %i reached %i cycles", i, coilTimes[i]);
          Serial.println(errorMessage);
        }
	    }
	  }
    //low means beam broken by projectile <-- double checked this, it is correct
    //high means sensor is clear
    //this if statement is supposed to trigger if beam is broken
    if(!digitalReadFast(i))
    { 
      if(activeSensor == -1) {
        //zero-indexing the active sensor
        activeSensor = i-FIRST_SENSOR_PIN;
      }
      else {
        errorFlag = true;
        return -1;
      }
    }
  }
  return activeSensor;
}


//note: this assumes the projectile is moving forwards.
//killing coil+1 to prevent thermal runaway if projectile moves forwards
void actuateCoils(int coil)
{
  //turn off one coil ahead (in case the projectile is moving backwards or something)
  turnCoilOff(coil+1);
  //turn on current coil
  turnCoilOn(coil);
  //turn off previous coil
  turnCoilOff(coil-1);
  //TODO: possibly make some masking bits to write to memory for
  //all coil outputs? That way no more than one coil can be on.
  //not sure if we want that or not though.
}

//turn coil on and write to active coil array so we know which is currently on
void turnCoilOn(int coil)
{
  //writing to the coil drops the output for a split microsecond, so only write if it's not on
  //if(coil >= 0 && coil <= COIL_COUNT && activeCoils[coil] != 1)
  //{
    activeCoils[coil] = 1;
    digitalWriteFast(coil+FIRST_COIL_PIN, HIGH);
  //}
}

//turn coil off, write to active coil array, reset coil on timer
void turnCoilOff(int coil)
{
  if(coil >= 0 && coil < COIL_COUNT)
  {
    digitalWriteFast(coil+FIRST_COIL_PIN, LOW);
    activeCoils[coil] = 0;
    //don't reset this here, leave it for the main loop so we can dump stats
    //coilTimes[coil] = 0;
  }
}

//shut down everything. Is called in emergency, but not exclusivly 
void coilShutdown()
{
  activeShot = false;
  coilIndex = 0;
  for(int i = 0; i < COIL_COUNT; i++) {
    turnCoilOff(i);
	//leave this resetting bit for the shotFinished cleanup in main loop so we can dump debug stats
	//coilTimes[i] = 0;
  }
}

void timerCallback(void)
{
  digitalWriteFast(11, HIGH);

  int sensor = getActiveSensor(); //getActiveSensor() also sets error bit
  //error case, overrides everything
  if(errorFlag == true || !softwareEnable)
  {
    coilShutdown();
    activeShot = false;
	//keeping slow calls like serial.print inside the main loop
  }
  //no error and activly shooting
  else if(activeShot)
  {
    //projectile is breaking a beam, activate coil, count up coil on timer, and advance coil index
    //TODO: maybe move coil timer to the actuateCoil function?
    if(sensor != -1)
    {
      //TODO: what about case where last coil in string?
      //in theory that's caught elsewhere, but should be case for it here to be safe.
      //activeShot,sensor,coilIndex
      sprintf(msg, "1,%i,%i\n", sensor,coilIndex);
      Serial.print(msg);
      
      actuateCoils(sensor);
      coilIndex = sensor;
    }
    //projectile is in between coils
    //(error case would have been caught higher up)
    //add && coilIndex != 0 to catch case in which first sensor not broken initially (only happens when hand fireing)
    else if(sensor == -1)
    {
      //this bit pre-charges the next coil
      //big potential to be slightly dangerous if forgotten about
      //coil_count -1 because we don't pre-charge on the last coil (no coil after that to charge)
      //TODO: could actuateCoils handle this logic and just shutdown if commanded to actuate a non-existent coil?
      if(coilIndex < COIL_COUNT-1) {
        //activeShot,sensor,coilIndex
        //sprintf(msg, "1,-1,%i\n", coilIndex);
        //Serial.print(msg);
        actuateCoils(coilIndex+1);
      }
      //if we're on the last coil, shutdown instead of pre-charging
      //TODO: handle this case in actuateCoil() and remove this logic from here?
      else if (coilIndex >= COIL_COUNT-1){
        //sprintf(msg, "0,-1,%i\n", coilIndex);
        //Serial.print(msg);
        Serial.print("last coil?");
        activeShot = false;
        coilShutdown();
		    shotComplete = 1;
      }
      else {
        //sprintf(msg, "not sure what happened here, coilindex = %i\n", coilIndex);
        //Serial.print(msg);
		    //this used to reset activeCoilTimer?
      }
    }
    else {
      //This case should never be reached. If it is reached,it's
      //because an error flag magically appeared between instructions.
    }
  }
  else
  {
    //do nothing further since there's no active trigger command
  }
  digitalWriteFast(11, LOW);
}

