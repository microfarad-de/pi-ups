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
#ifndef __Led_H_
#define __Led_H_

#include <Arduino.h>

/*
 * Led control class
 */
class LedClass {
  public:
    void initialize (uint8_t ledPin);
    void loopHandler (void);
    void turnOn (void);
    void turnOff (void);
    void toggle (void);
    void blink (int32_t count, uint32_t tOn, uint32_t tOff);
    void blinkBlocking (int32_t count, uint32_t tOn, uint32_t tOff);
    void blinkStop (void);

    bool blinking = false;
    bool powerOn = false;
    
  private:
    bool initialized = false;
    uint8_t ledPin;
    uint32_t blinkTs = 0;
    int32_t count;
    uint32_t tOn = 0;
    uint32_t tOff = 0;
    bool blinkOn;
};





#endif /* __Led_H_ */
