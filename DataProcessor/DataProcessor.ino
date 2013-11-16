
/**
 * Acts as a filter for data.
 * 
 * Copyright 2013 Joseph Lewis <joehms22@gmail.com>
 **/

#include <Arduino.h>
#include <Wire.h>
#include <OBD.h>
#include <MultiLCD.h>
#include <EEPROM.h>
#include "EEPROMAnything.h"

//#define PRODUCTION

const float MAX_SAFE_DECEL_MPH = 3.5;
const float MAX_SAFE_ACCEL_MPH = 3.5;
const int SECONDS_BETWEEN_READS = 1;
const int REPORT_ALL_PIN = 13;

// This data must be stored for processing.
int lastVelocity;
int lastTime;

// This value holds the current event.
int eventId;


struct Trip{
  int tripTime;
  int speedBucket[4];
  int stopCount;
  float totalMileage;
  int over80Seconds;
  int hardBrakeCount;
  int hardAccelCount;
};

#ifdef PRODUCTION
// Our Hardware
COBD obd; // uart obd2 connector

#ifdef USELCD
LCD_SSD1306 lcd; // SSD1306 OLED module
#endif

#endif


Trip trip;

void setup() {
  // INIT all variables
  lastVelocity = 0.0;
  lastTime = 0;
  eventId = 0;

  clearTrip();
  //Initialize serial and wait for port to open:
  Serial.begin(9600); 
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }

  // INIT all connections

#ifdef PRODUCTION
  // start communication with OBD-II UART adapter
  obd.begin();
  // initiate OBD-II connection until success
  while (!obd.init());


#ifdef USELCD
  lcd.begin();
  lcd.setFont(FONT_SIZE_SMALL);
  lcd.println("ABCDEFGHIJK");
#endif
#endif


  // 
  pinMode(REPORT_ALL_PIN, INPUT);           // set pin to input
  if(digitalRead(REPORT_ALL_PIN) == LOW)
  {
    Serial.print("Attach pin ");
    Serial.print(REPORT_ALL_PIN);
    Serial.println(" to 3V3 to print results");
  }
  else
  {
     recoverTrips(); 
  }


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
  report("Speed < 30 Seconds", trip.speedBucket[0]); 
  report("Speed 30-45 Seconds", trip.speedBucket[1]); 
  report("Speed 45-55 Seconds", trip.speedBucket[2]); 
  report("Speed 55+ Seconds", trip.speedBucket[3]); 
  report("#Stops", trip.stopCount);
  report("Time (s)", trip.tripTime);
  report("Miles", trip.totalMileage);
  report("Seconds > 80mph", trip.over80Seconds);
  report("# Hard Brakes", trip.hardBrakeCount);
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
#ifdef PRODUCTION
  return obd.readSensor(PID_SPEED, mph);
#else
  Serial.println("Speed MPH?");
  while (Serial.available() == 0) {
    ; //wait for mph
  }

  mph = Serial.parseInt();
  return true;
#endif
}


void loop() 
{  
  static int loopCounter = 0;
  int currentTime = loopCounter * SECONDS_BETWEEN_READS;

#ifndef PRODUCTION
  report("Uptime", currentTime);
#endif


  int mph;
  if (getNextSpeed(mph)) {

    if(mph < 0)
    {
      saveTrip(); 

    }

    hardBrake(currentTime, mph);
    processOver80MPH(currentTime, mph);
    totalMileage(currentTime, mph);
    tripTime(currentTime, mph);
    stopCount(currentTime, mph);
    speedBucket(currentTime, mph);
    lastVelocity = mph;
    lastTime = currentTime;

    reportValues();
  }



  loopCounter++;
  // wait to log another item
  delay(SECONDS_BETWEEN_READS * 1000);
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
  if(velocity > 0)
  {
    trip.totalMileage += (((float) velocity) / (60 * 60)) * (time - lastTime);
  }
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
  if(velocity < 0)
  {
    return;
  }

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
    if(hadHardBrake == false && brakeSpeed > 3.5)
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

void recoverTrips()
{
  while(loadTrip())
  {
     reportValues(); 
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



