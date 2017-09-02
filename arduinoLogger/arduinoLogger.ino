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

unsigned long checkSum = 0; /* Holds the checksum value of the wave */
int highFrequency = 0;      /* Holds the "parity" of the wave */

void resetValues() {
  if (pointBuffer > 0) {
    pointBuffer --;
  } else {
    pointBuffer = 20;
    points = 0;
    checkSum = 0;
    highFrequency = 0;
    typeCounter = 0;
  }
}

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
  DateTime now = rtc.now();
  
  data=analogRead(rfReceivePin);    //listen for data on Analog pin 0

  avg += data;
  avgCnt ++;

  if (avgCnt >= 5) {
    cnt = avg / avgCnt;

    if (cnt >= 450 && cnt <= 470) {
      if (type != 4) {
        typeCounter = 0;
      }

      type = 4;
      typeCounter ++;
    } else if (cnt >= 145 && cnt <= 175) {
      if (type != 1) {
        typeCounter = 0;
      }

      type = 1;
      typeCounter ++;
    } else if (cnt >= 295 && cnt <= 315) {
      if (type != 3) {
        typeCounter = 0;
      }

      type = 3;
      typeCounter ++;
    } else if (cnt >= 605 && cnt <= 625) {
      if (type != 6) {
        typeCounter = 0;
      }

      type = 6;
      typeCounter ++;
    } else if (cnt >= 0 && cnt <= 20) {
      if (type != 0) {
        typeCounter = 0;
      }

      type = 0;
      typeCounter ++;
    } else {
      typeCounter = 0;

      resetValues();
    }

    if (typeCounter != 0) {
      points ++;

      checkSum += avg;

      if (avg >= 1750) {
        if (!prevLow) {
          highFrequency ++;
        }
    
        prevLow = false;
      } else {
        if (prevLow) {
          highFrequency ++;
        }
    
        prevLow = true;
      }

      if (typeCounter >= 3 || (typeCounter >= 2 && type == 0)) {
        resetValues();
      }

      if (points >= 100) {

        if ((checkSum >= 100000 && checkSum <= 140000) &&
            (highFrequency >= 50 && highFrequency <= 70)) {
              pressCounter ++;
              Serial.print("PRESS #");
              Serial.println(pressCounter);
            }
        else {
          Serial.println("- IGNORED WAVE -");
        }

        Serial.print("---------- CHECKSUM -------------: ");
        Serial.println(checkSum);

        Serial.print("---------- CLARITY -------------: ");
        Serial.println(highFrequency);

        points = 0;
        checkSum = 0;
        highFrequency = 0;
      }
    }
    
    avgCnt = 0;
    avg = 0; 
  }
}
