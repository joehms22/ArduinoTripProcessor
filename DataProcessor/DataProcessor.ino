/**
 * Acts as a filter for data.
 * 
 * Copyright 2013 Joseph Lewis <joehms22@gmail.com>
 **/

#define USELCD
#define USESD


#include <Arduino.h>
#include <Wire.h>
#include <OBD.h>


// Optional headers for extra data processing.

#ifdef USESD
#include <SD.h>
#endif

#ifdef USELCD
#include <MultiLCD.h>
#else
#include <LiquidCrystal.h>
#endif


const float MAX_SAFE_DECEL_MPH = 3.5;
const float MAX_SAFE_ACCEL_MPH = 3.5;
const int SECONDS_BETWEEN_READS = 1;

// This data must be stored for processing.
int lastVelocity;
int lastTime;

// This value holds the current event.
int eventId;


// Our Hardware
COBD obd; // uart obd2 connector
#ifdef USELCD
LCD_SSD1306 lcd; // SSD1306 OLED module
#else
// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
#endif



void setup() {
  // INIT all variables
  lastVelocity = 0.0;
  lastTime = 0;
  eventId = 0;


  // INIT all connections

  // start communication with OBD-II UART adapter
  obd.begin();
  // initiate OBD-II connection until success
  while (!obd.init());

#ifdef USELCD
  lcd.begin();
  lcd.setFont(FONT_SIZE_SMALL);
  lcd.println("ABCDEFGHIJK");
#else
  // set up the LCD's number of columns and rows: 
  lcd.begin(16, 2);
  // Print a message to the LCD.
  lcd.print("hello, world!");
#endif

}

/**
 * Returns a new event id, logging the event name it belongs to
 **/
int getEvent(String eventName)
{
  int toRet = eventId;
  logString("BEGIN EVENT," + eventName + "," + toRet + "\n");
  eventId++;
  return toRet;
}

void logString(String s)
{
#ifdef USESD
  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  File dataFile = SD.open("speedreport.dat", FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(s);
    dataFile.close();
  }  
  // if the file isn't open, pop up an error:
  else {
    //Serial.println("error opening datalog.txt");
  } 
#endif
}

void report(String reportName, float reportValue)
{
  logString(reportName + ",");//(reportValue));
}


void loop() 
{  
  static int loopCounter = 0;

  int currentTime = loopCounter * SECONDS_BETWEEN_READS;
  int mph;
  if (obd.readSensor(PID_SPEED, mph)) {
    // process sensor
    report("MILES PER HOUR", mph);

    processOver80MPH(currentTime, mph);
    totalMileage(currentTime, mph);
    tripTime(currentTime, mph);
    stopCount(currentTime, mph);
    speedBucket(currentTime, mph);

    lastVelocity = mph;
    lastTime = currentTime;
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
  static int pointsOver80 = 0;

  if(velocity < 0)
    report("Time Over 80 MPH", pointsOver80);

  if(velocity > 80)
    pointsOver80 += SECONDS_BETWEEN_READS;
}

void totalMileage(int time, int velocity)
{
  static float totalMileage = 0.0;

  if(velocity < 0)
  {
    report("Total Mileage", totalMileage);
  }
  else
  {
    totalMileage += (((float) velocity) / (60 * 60)) * (time - lastTime);
  }
}

void tripTime(int time, int velocity)
{
  if(velocity < 0)
  {
    report("Trip Time (seconds)", lastTime);
  } 
}

void stopCount(int time, int velocity)
{
  static int stops = 0;
  if(velocity < 0)
  {
    report("Stop Count", stops);
    stops = 0;
    return;
  } 

  if(velocity == 0 && lastVelocity != 0)
  {
    stops++; 
  }
}

void speedBucket(int time, int velocity)
{
  static int zeroToThirty = 0;
  static int thirtyToFortyFive = 0;
  static int fortyFiveToFiftyFive = 0;
  static int fiftyFivePlus = 0;

  if(velocity < 0)
  {
    int totalPoints = zeroToThirty + thirtyToFortyFive + fortyFiveToFiftyFive + fiftyFivePlus;

    report("% of trip 0-30MPH", ((float)zeroToThirty)/totalPoints);
    report("% of trip 30-45MPH", ((float)thirtyToFortyFive)/totalPoints);
    report("% of trip 45-55MPH", ((float)fortyFiveToFiftyFive)/totalPoints);
    report("% of trip 55+MPH", ((float)fiftyFivePlus)/totalPoints);

    zeroToThirty = thirtyToFortyFive = fortyFiveToFiftyFive = fiftyFivePlus = 0;
    return;
  }

  if(velocity < 30){
    zeroToThirty++;
  }
  else if(velocity < 45){
    thirtyToFortyFive++;
  }
  else if(velocity < 55){
    fortyFiveToFiftyFive++;
  }
  else{
    fiftyFivePlus++; 
  }
}

