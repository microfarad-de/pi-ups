/* 
 * LED Control Class
 * 
 * This source file is part of the Raspberry Pi UPS Arduino firmware
 * found under http://www.github.com/microfarad-de/pi-ups
 * 
 * Please visit:
 *   http://www.microfarad.de
 *   http://www.github.com/microfarad-de
 * 
 * Copyright (C) 2019 Karim Hraibi (khraibi at gmail.com)
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>. 
 */

 #include "Led.h"


 void LedClass::initialize (uint8_t ledPin) {
  this->ledPin = ledPin;
  pinMode (ledPin, OUTPUT);
  digitalWrite (ledPin, LOW);
  powerOn = false;
  blinking = false;
  initialized = true;
}

void LedClass::loopHandler (void) {
  uint32_t ts;
  
  if (!initialized || !blinking ) return;

  ts = millis ();
  
  if ( (blinkOn && ts - blinkTs > tOn) || (!blinkOn && ts - blinkTs > tOff) ) {
    blinkOn = !blinkOn;
    digitalWrite (ledPin, blinkOn);
    blinkTs = ts;
    if (count > 0 ) count--;
    else if (count == 0) {
      blinkStop ();
    }
  }
}

void LedClass::turnOn (void) {
  if (!initialized) return;
  blinking = false;
  powerOn = true;
  digitalWrite (ledPin, powerOn);
}

void LedClass::turnOff (void) {
  if (!initialized) return;
  blinking = false;
  powerOn = false;
  digitalWrite (ledPin, powerOn);
}

void LedClass::toggle (void) {
  if (!initialized) return;
  blinking = false;
  powerOn = !powerOn;
  digitalWrite (ledPin, powerOn);
}

void LedClass::blink (int32_t count, uint32_t tOn, uint32_t tOff) {
  if (!initialized || count == 0) return;
  this->blinking = true;
  this->count = 2 * count;
  this->tOn = tOn;
  this->tOff = tOff;
  this->blinkOn = !powerOn;
  digitalWrite (ledPin, blinkOn);  
  blinkTs = millis ();
}

void LedClass::blinkStop (void) {
  if (!initialized) return;
  blinking = false;
  digitalWrite (ledPin, powerOn);  
}

void LedClass::blinkBlocking (int32_t count, uint32_t tOn, uint32_t tOff) {
  blink (count, tOn, tOff);
  while (blinking) loopHandler ();
}
