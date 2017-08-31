/* 
  RF Blink - Receiver sketch 
     Written by ScottC 17 Jun 2014
     Arduino IDE version 1.0.5
     Website: http://arduinobasics.blogspot.com
     Receiver: XY-MK-5V
     Description: A simple sketch used to test RF transmission/receiver.          
 ------------------------------------------------------------- */
#include "RTClib.h"

#define rfReceivePin A0  //RF Receiver pin = Analog pin 0
#define rfReceivePinDigital 2

RTC_DS1307 rtc;

unsigned int data = 0;   // variable used to store received data
int cnt = 0;

unsigned int avg = 0;
unsigned int avgCnt = 0;

int type = 0;
int typeCounter = 0;
int type0Counter = 0;

int points = 0;

int pointBuffer = 0;

int pressCounter = 0;

int valueArray[550];

void setup () {
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

void loop(){
  DateTime now = rtc.now();
  
  data=analogRead(rfReceivePin);    //listen for data on Analog pin 0

  avg += data;
  avgCnt ++;

  valueArray[(points * 5) + avgCnt] = data;

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
        type0Counter = 0;
      }

      type = 0;
      type0Counter ++;
    } else {
      typeCounter = 0;

      if (pointBuffer > 0) {
        pointBuffer --;
      } else {
        pointBuffer = 20;
        points = 0;
      }
    }

    if (typeCounter != 0) {
      points ++;

      if (typeCounter >= 3 || type0Counter >= 2) {
        if (pointBuffer > 0) {
          pointBuffer --;
        } else {
          pointBuffer = 20;
          points = 0;
          typeCounter = 0;
          type0Counter = 0;
        }
      }

      if (points >= 100) {
        unsigned long checkSum = 0;
        int highFrequency = 0;

        bool prevLow = false;
        int value = 0;

        for (int i = 0; i <= points * 5; i ++) {
          value = valueArray[i];

          checkSum += value;
          // Serial.println(value);

          if (value >= 350) {
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
        }
        
        points = 0;

        if ((checkSum >= 115000 && checkSum <= 135000) &&
            (highFrequency >= 330 && highFrequency <= 360)) {
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
      }
    }
    
    // Serial.println(avg / avgCnt);
    avgCnt = 0;
    avg = 0; 
  }
}
