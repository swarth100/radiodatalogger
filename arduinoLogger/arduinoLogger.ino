#include <SD.h>
#include "RTClib.h"

#define rfReceivePin A0  /* RF Receiver pin = Analog pin 0 */

RTC_DS1307 rtc;

unsigned int data = 0;      /* Stores received analog raw data from rfReceivePin */
unsigned int avg = 0;       /* Stores the sum of 5 data instances */
unsigned int avgCnt = 0;    /* Keeps count of the data instances summed into avg */
unsigned int cnt = 0;       /* Holds the avg value divided by the avgCnt */

int type = 0;               /* Stores the "type" of data received (i.e. 300 or 600) */
int typeCounter = 0;        /* Stores the number of consecutive identical types */

int points = 0;             /* Stores the consecutive number of wave hits */

int pointBuffer = 0;        /* Keeps track of possible wave misses due to errors */

int pressCounter = 0;       /* Keeps track of consecutive number of w<ve hits */

bool prevLow = false;       /* Keeps track of consecutive wave oscillations */
bool prevMed = false;
bool prevHigh = false;

int lowFrequency = 0;       /* Holds the "parity" of the wave */
int medFrequency = 0;
int highFrequency = 0;

unsigned long checkSum = 0; /* Holds the checksum value of the wave */

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

/* Sets up the main arduino components */
void setup() {
  Serial.begin(9600);
  
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  if (! rtc.isrunning()) {
    Serial.println("RTC is NOT running!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

void loop() {
  /* Retrieves an instance of the current date and time */
  DateTime now = rtc.now();

  /* Listens for data on analog port A0 */
  data=analogRead(rfReceivePin);

  /* Increments avg and avgCnt at every read */
  avg += data;
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
            (lowFrequency >= 25 && lowFrequency <= 45) && 
            (medFrequency >= 50 && medFrequency <= 70) &&
            (highFrequency >= 90)) {
              pressCounter ++;
              Serial.print("PRESS #");
              Serial.println(pressCounter);
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
  }
}
