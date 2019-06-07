/*
 * Abstraction layer for the ATmega328p ADC
 * Non-blocking read the ADC output as an alternative
 * to the blocking analogRead() method.
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

#ifndef __ADC_H
#define __ADC_H

#include <Arduino.h>


#define ADC_NUM_PINS 8  /* Maximum number of ADC pins */

/*
 * ADC prescaler values
 */
enum AdcPrescaler_t {
  ADC_PRESCALER_2  = 1,
  ADC_PRESCALER_4  = 2,
  ADC_PRESCALER_8  = 3,
  ADC_PRESCALER_16 = 4,
  ADC_PRESCALER_32 = 5,
  ADC_PRESCALER_64 = 6,
  ADC_PRESCALER_128 = 7
};

/*
 * Analog pin numbers
 * Note:
 * Arduino macro A0 = ADC_PIN0 + 14
 * the same applies for A1, A2...
 */
enum AdcPin_t {
  ADC_PIN0 = 0,
  ADC_PIN1 = 1,
  ADC_PIN2 = 2,
  ADC_PIN3 = 3,
  ADC_PIN4 = 4,
  ADC_PIN5 = 5,
  ADC_PIN6 = 6,
  ADC_PIN7 = 7
};

/*
 * Analog reference sources
 */
enum AdcReference_t {
  ADC_EXTERNAL = 0,                       /* external input */
  ADC_DEFAULT  = _BV(REFS0),              /* AVcc */
  ADC_INTERNAL = _BV(REFS0) | _BV(REFS1)  /* internal 1.1V */
};

/*
 * ADC class definition
 */
class AdcClass {

  public:

    /*
     *  Initialize the ADC
     */
    void initialize (
      AdcPrescaler_t prescaler,  // ADC prescaler value
      AdcReference_t reference,  // ADC reference voltage
      uint8_t avgSamples,        // Number of averaged samples
      uint8_t numPins,           // Number of ADC pins to be read
      AdcPin_t *adcPins          // Array of ADC pin numbers of size numPins
      );

    /*
     * Start ADC conversion
     */
    void start (
      AdcPin_t adcPin  // ADC pin number (0..7 for ATmega328p)
      );

    /*
     * Read ADC result
     * Return value:
     *   -1      : no result available yet
     *   0..1023 : ADC result
     */
    int16_t readVal (void);

    /*
     * Read all preset ADC inputs
     * When called repeatedly it will cycle through the pre-configuraed ADC chanels defined by numPins,
     * get the ADC results, average them as defined in avgSamples and store them in the result array.
     * Return value:
     *  false : no result available yet
     *  true  : all the channels have been read and averaged
     */
    bool readAll (void);

    /*
     * ADC result can be read from this array
     */
    uint32_t result[ADC_NUM_PINS];

  private:
    bool working = false;
    AdcReference_t reference;
    AdcPin_t adcPins[ADC_NUM_PINS];
    uint8_t numPins = 0;
    uint8_t pinIdx = 0;
    uint8_t avgSamples = 1;
    uint8_t avgCount = 0;
};



/*
 * ADC class instantiated as a singleton
 */
extern AdcClass ADConv;

#endif // __ADC_H
