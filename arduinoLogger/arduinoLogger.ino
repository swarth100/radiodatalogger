#include <SD.h>
#include <Wire.h>
#include "RTClib.h"

#define rfReceivePin A0     /* RF Receiver pin = Analog pin 0 */
#define cardInsertPin A1     /* RF Receiver pin = Analog pin 0 */

#define WaveLED 9
#define SDLED 8
#define DisconnectLED 7
#define WriteLED 6

#define BtnPIN 11

const int chipSelect = 10;  /* COnstant reference SD card Pin */
  
RTC_DS1307 rtc;             /* Time references */

File logfile;               /* Filesystem used for logging */

DateTime now;

#define SYNC_INTERVAL 5000  /* Intervals between calls to writing output */
uint32_t syncTime = 0;      /* Time of last sync */ 
 
unsigned int data = 0;      /* Stores received analog raw data from rfReceivePin */
unsigned int avg = 0;       /* Stores the sum of 5 data instances */
unsigned int avgCnt = 0;    /* Keeps count of the data instances summed into avg */
unsigned int cnt = 0;       /* Holds the avg value divided by the avgCnt */

int type = 0;               /* Stores the "type" of data received (i.e. 300 or 600) */
int typeCounter = 0;        /* Stores the number of consecutive identical types */

int points = 0;             /* Stores the consecutive number of wave hits */

int pointBuffer = 0;        /* Keeps track of possible wave misses due to errors */

int pressCounter = 0;       /* Keeps track of consecutive number of wave hits */

bool prevLow = false;       /* Keeps track of consecutive wave oscillations */
bool prevMed = false;
bool prevHigh = false;

int lowFrequency = 0;       /* Holds the "parity" of the wave */
int medFrequency = 0;
int highFrequency = 0;

unsigned long checkSum = 0; /* Holds the checksum value of the wave */

long sdCardVal = 0;
int sdCardCnt = 0;

bool isCardInserted = true;

void (*Reboot)() = 0;

void printStr(char* data) {
  logfile.print(data);    
  Serial.print(data);
}

void printInt(long data) {
  logfile.print(data, DEC);    
  Serial.print(data, DEC);
}

void printLn() {
  logfile.println();    
  Serial.println();
}

/* Resets the global logged values.
   Takes into account the point buffer avoiding reset until exhaustion */
void resetValues() {
  if (pointBuffer > 0) {
    pointBuffer --;
  } else {
    pointBuffer = 20;
    points = 0;
    checkSum = 0;
    lowFrequency = 0;
    medFrequency = 0;
    highFrequency = 0;
    typeCounter = 0;
  }
}

/* Updates the type and typeCounter values
   Resets the typeCounter should the type have changed*/
void checkType (int refValue) {
  if (type != refValue) {
      typeCounter = 0;
    }

    type = refValue;
    typeCounter ++;
}

/* Updates the frequency counters (ints) on consecutive refBool hits */
void checkFreq (int threshold, bool* refBool, int* refInt) {
  if (avg >= threshold) {
    if (!*refBool) {
      (*refInt) ++;
    }
    *refBool = false;
  } else {
    if (*refBool) {
      (*refInt) ++;
    }
    *refBool = true;
  }
}

void error(char *str)
{
  Serial.print("Error: ");
  Serial.println(str);

  while(1);
}

void printTime() {

  /* Fetch the milliseconds since the start */
  printInt(millis());
  printStr("; ");    

  // fetch the time
  now = rtc.now();
  // log time
  printInt(now.unixtime()); // seconds since 1/1/1970
  printStr(";");
  printInt(now.day());
  printStr("/");
  printInt(now.month());
  printStr("/");
  printInt(now.year());
  printStr(" ");
  printInt(now.hour());
  printStr(":");
  printInt(now.minute());
  printStr(":");
  printInt(now.second());
  printStr("; ");
}

/* Sets up the main arduino components */
void setup() {
  Serial.begin(9600);

  pinMode(WriteLED, OUTPUT);
  pinMode(DisconnectLED, OUTPUT);
  pinMode(SDLED, OUTPUT);
  pinMode(WaveLED, OUTPUT);

  pinMode(BtnPIN, INPUT);
  
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  if (! rtc.isrunning()) {
    Serial.println("RTC is NOT running!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  Serial.println("Initializing SD card...");

  /* Must be set to output even if unused */
  pinMode(10, OUTPUT);           
  
  /* Check the card's status/presence */
  if (!SD.begin(chipSelect)) {
    error("Card failed, or not present");
  }
  Serial.println("card initialized.");

  /* Attepts to create a new logger file
     Will create up to 100 possible files. Will not open already created files*/
  char filename[] = "LOGGER00.CSV";
  for (uint8_t i = 0; i < 100; i++) {
    filename[6] = i/10 + '0';
    filename[7] = i%10 + '0';
    if (! SD.exists(filename)) {
      /* Only opens unexistant files */
      logfile = SD.open(filename, FILE_WRITE); 
      break;
    }
  }

  /* Logfile failure */
  if (! logfile) {
    error("couldnt create file");
  }

  Serial.print("Logging to: ");
  Serial.println(filename);

  Wire.begin();

  if (!rtc.begin()) {
    printStr("RTC failed");
  }
}

void loop() {
  /* Retrieves an instance of the current date and time */
  now = rtc.now();

  /* Listens for data on analog port A0 */
  avg += analogRead(rfReceivePin);

  /* Increments avgCnt at every read */
  avgCnt ++;

  /* Whenever there are more than 5 pieces of data calculations to determine
     wether it is the right wave or not are fired. */
  if (avgCnt >= 5) {
    cnt = avg / avgCnt;
    if (cnt >= 0 && cnt <= 20) {
      checkType(0);
    } else if (cnt >= 145 && cnt <= 175) {
      checkType(1);
    } else if (cnt >= 295 && cnt <= 315) {
      checkType(3);
    } else if (cnt >= 450 && cnt <= 470) {
      checkType(4);
    } else if (cnt >= 605 && cnt <= 625) {
      checkType(6);
    } else {
      typeCounter = 0;
      resetValues();
    }

    /* When a valid item in the wave is found we can enter the validation area */
    if (typeCounter != 0) {
      points ++;

      checkSum += avg;

      /* Update the consecutive frequency counters where necessary */
      checkFreq(1000, &prevLow, &lowFrequency);
      checkFreq(1750, &prevMed, &medFrequency);
      checkFreq(2500, &prevHigh, &highFrequency);

      /* Reset values should the type counter have exceeded the allowed thresholds */
      if (typeCounter >= 3 || (typeCounter >= 2 && type == 0)) {
        resetValues();
      }

      /* Should the consecutive point hits be greater than 100 consider the wave */
      if (points >= 100) {

        /* Perform value threshold checks and accordingly consider or remove the wave */
        if ((checkSum >= 100000 && checkSum <= 140000) &&
            (lowFrequency >= 20 && lowFrequency <= 50) && 
            (medFrequency >= 50 && medFrequency <= 80) &&
            (highFrequency >= 80)) {
              pressCounter ++;
              Serial.print("PRESS #");
              Serial.println(pressCounter);

              digitalWrite(WaveLED, HIGH);
            }
        else {
          Serial.println("- IGNORED WAVE -");
        }

        Serial.print("---------- CHECKSUM -------------: ");
        Serial.println(checkSum);

        Serial.print("---------- LOW FR -------------: ");
        Serial.println(lowFrequency);

        Serial.print("---------- MED FR -------------: ");
        Serial.println(medFrequency);

        Serial.print("---------- HIGH FR -------------: ");
        Serial.println(highFrequency);

        /* Force a pointBuffer of 0 and reset all the values */
        pointBuffer = 0;
        resetValues();
      }
    }

    /* Reset the avg counters */
    avgCnt = 0;
    avg = 0; 

    digitalWrite(WaveLED, LOW);
  }

  sdCardVal += analogRead(cardInsertPin);
  sdCardCnt ++;

  if (sdCardCnt >= 100) {
    if (sdCardVal <= 10) {
      digitalWrite(SDLED, HIGH);
    } else {
      digitalWrite(SDLED, LOW);
    }

    sdCardVal = 0;
    sdCardCnt = 0;
  }

  if (digitalRead(BtnPIN) == HIGH) {  
    if (isCardInserted) {
      digitalWrite(DisconnectLED, LOW); 
    }
  } else {
    if (isCardInserted) {
      digitalWrite(DisconnectLED, HIGH);
      isCardInserted = false;
    }
  }

  /* Write data to SD card */
  if ((millis() - syncTime) < SYNC_INTERVAL) return;
  syncTime = millis();

  printTime();

  printInt(pressCounter);
  printStr(";");
  printLn();

  if (isCardInserted) {
    digitalWrite(WriteLED, HIGH);
    
    logfile.flush();
    
    digitalWrite(WriteLED, LOW);
  }

  /* Resets permanent interval press counter counts */
  pressCounter = 0;
}
