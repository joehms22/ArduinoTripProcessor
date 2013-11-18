
/**
 * Acts as a filter for data.
 * 
 * Copyright 2013 Joseph Lewis <joehms22@gmail.com>
 **/
 
// DEFINES

#include <Arduino.h>
#include <Wire.h>
#include <OBD.h>
#include <MultiLCD.h>
#include <EEPROM.h>
#include "EEPROMAnything.h"


// 1 - obdii monitor, 2 - Serial input, 3 - random()
#define VELOCITY_SOURCE 3

#if VELOCITY_SOURCE == 1
#define VELOCITY_OBDII
#elif VELOCITY_SOURCE == 2
#define VELOCITY_SERIAL
#else
#define VELOCITY_RANDOM
#endif

// STRUCTURES

struct Trip{
  int tripTime;
  int speedBucket[4];
  int stopCount;
  float totalMileage;
  int over80Seconds;
  int hardBrakeCount;
  int hardAccelCount;
};


// CONSTANTS

const float MAX_SAFE_DECEL_MPH = 3.5;
const float MAX_SAFE_ACCEL_MPH = 3.5;
const int SECONDS_BETWEEN_READS = 1;
const int REPORT_ALL_PIN = 13;
const float KPH_TO_MPH = 0.621371;
const int DISPLAY_REFRESH_TIME = 5; // the number of SECONDS_BETWEEN_READS intervals to wait for a display update


// VARIABLES

int lastVelocity;
int lastTime;
Trip trip;

#ifdef VELOCITY_OBDII
COBD obd; // uart obd2 connector
#endif

#ifdef USELCD
LCD_SSD1306 lcd; // SSD1306 OLED module
#endif




void setup() {
  // INIT all variables
  lastVelocity = 0.0;
  lastTime = 0;
  clearTrip();


  //Initialize serial and wait for port to open:
  Serial.begin(9600); 
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }

#ifdef VELOCITY_OBDII
  // start communication with OBD-II UART adapter
  obd.begin();
  while (!obd.init()){
    ; // initiate OBD-II connection until success
  }
#endif


#ifdef USELCD
  lcd.begin();
  lcd.setFont(FONT_SIZE_SMALL);
#endif


  // print out the trips to serial if needed.
  recoverTrips();


}

void report(String reportName, float reportValue)
{
  Serial.print(reportName);
  Serial.print(" -> ");
  Serial.println(reportValue);
}


void reportValues()
{
  Serial.println("============================================================");
  report("Speed < 30 (s)", trip.speedBucket[0]); 
  report("Speed 30-45 (s)", trip.speedBucket[1]); 
  report("Speed 45-55 (s)", trip.speedBucket[2]); 
  report("Speed 55+ (s)", trip.speedBucket[3]); 
  report("Speed 80+ (s)", trip.over80Seconds);
  report("Trip Time (s)", trip.tripTime);
  report("Miles", trip.totalMileage);
  report("# Stops", trip.stopCount);
  report("# Hard Brake", trip.hardBrakeCount);
  report("# Hard Accel", trip.hardAccelCount);  
}

void clearTrip()
{
    memset((char *) &trip, 0, sizeof(trip));
}

/**
 * Returns the next speed from whatever source we are using
 **/
boolean getNextSpeed(int& mph)
{
#if defined(VELOCITY_OBDII)
  
  int kph;
  boolean retval = obd.readSensor(PID_SPEED, kph);
  mph = kph * KPH_TO_MPH;
  return retval;
  
#elif defined(VELOCITY_SERIAL)
  
  Serial.println("Speed MPH?");
  while (Serial.available() == 0) {
    ; //wait for mph
  }

  mph = Serial.parseInt();
  return true;
  
#else
  mph = random(0,100);
  return true;
#endif
}


void loop() 
{  
  static int loopCounter = 0;
  int currentTime = loopCounter * SECONDS_BETWEEN_READS;
  
  int mph;
  if (getNextSpeed(mph)) {

    if(mph < 0)
    {
      saveTrip(); 
      return;
    }

    hardBrake(currentTime, mph);
    processOver80MPH(currentTime, mph);
    totalMileage(currentTime, mph);
    tripTime(currentTime, mph);
    stopCount(currentTime, mph);
    speedBucket(currentTime, mph);
    lastVelocity = mph;
    lastTime = currentTime;
  }

  
  if(loopCounter % DISPLAY_REFRESH_TIME == 0)
  {
    reportValues();
  }
  
  // wait to log another item
  delay(SECONDS_BETWEEN_READS * 1000);
  loopCounter++;
}

/**
 * Below this line are all of the processors, each takes in a float and a 
 **/


void processOver80MPH(int time, int velocity)
{
  if(velocity > 80)
  {
    trip.over80Seconds += SECONDS_BETWEEN_READS;
  }
}

void totalMileage(int time, int velocity)
{
  trip.totalMileage += (((float) velocity) / (60 * 60)) * (time - lastTime);
}

void tripTime(int time, int velocity)
{
  trip.tripTime = time;
}

void stopCount(int time, int velocity)
{
  if(velocity == 0 && lastVelocity != 0)
  {
    trip.stopCount++; 
  }
}

void speedBucket(int time, int velocity)
{
  if(velocity < 30){
    trip.speedBucket[0] += SECONDS_BETWEEN_READS;
  }
  else if(velocity < 45){
    trip.speedBucket[1] += SECONDS_BETWEEN_READS;
  }
  else if(velocity < 55){
    trip.speedBucket[2] += SECONDS_BETWEEN_READS;
  }
  else{
    trip.speedBucket[3] += SECONDS_BETWEEN_READS;
  }
}

void hardBrake(int time, int velocity)
{
  static boolean hadHardBrake = false;

  if(velocity < lastVelocity)
  {
    float brakeSpeed = ((float)lastVelocity - velocity) / (time - lastTime);
    if(hadHardBrake == false && brakeSpeed > MAX_SAFE_DECEL_MPH)
    {
      hadHardBrake = true;
      trip.hardBrakeCount++;
    }
  }
  else
  {
    hadHardBrake = false;
  }
}

/**
Clears the filesystem if it looks like junk.
**/
boolean clearFs()
{
  byte two = EEPROM.read(E2END - 2);

  if(two != 0)
  {
    Serial.println("no filesystem!"); 

    EEPROM.write(E2END - 1, 1);
    EEPROM.write(E2END - 2, 0);
  }

}

/**
Prints out and clears the EEPROM of all existing trips.
**/
void recoverTrips()
{
  pinMode(REPORT_ALL_PIN, INPUT);           // set pin to input
  if(digitalRead(REPORT_ALL_PIN) == LOW)
  {
    Serial.print("Attach pin ");
    Serial.print(REPORT_ALL_PIN);
    Serial.println(" to 3V3 to print results");
  }
  else
  {
    while(loadTrip())
    {
       reportValues(); 
    } 
  }
}

boolean saveTrip()
{ 
  clearFs();

  byte numSaves = EEPROM.read(E2END -1);
  Serial.print("saves");
  Serial.println(numSaves);
  int savePos = numSaves * sizeof(trip);
  Serial.print("Saving to: ");
  Serial.println(savePos);

  if(savePos + sizeof(trip) > E2END -1 )
  {
    return false; 
  }

  EEPROM_writeAnything(savePos, trip);
  EEPROM.write(E2END - 1, numSaves+1);
  
  clearTrip();
  
  return true;
}

// beware of the following code, I have only tried it, not proven it correct!
// I suspect there is a bug
boolean loadTrip()
{
  clearFs();

  byte numSaves = EEPROM.read(E2END - 1) - 1;
  if(numSaves < 1)
  {
    return false;
  }
  EEPROM_writeAnything(numSaves * sizeof(trip), trip);
  EEPROM.write(E2END - 1, numSaves);

  return true;
}



