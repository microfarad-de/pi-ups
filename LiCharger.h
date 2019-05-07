/* 
 * Lithium-Ion Battery Charger Class
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
 * 
 */
#ifndef __LiCharger_H_
#define __LiCharger_H_

#include <Arduino.h>


/*
 * State machine states
 */
enum LiChargerState_t { 
  STATE_INIT_E, 
  STATE_INIT, 
  STATE_CHARGE_E, 
  STATE_CHARGE, 
  STATE_FULL_E, 
  STATE_FULL, 
  STATE_ERROR_E, 
  STATE_ERROR,  
};

/*
 * Lithium-Ion battery charger class
 */
class LiChargerClass
{
  public:
    /*
     * Initialization function
     */
    void initialize (
        uint16_t iChrg,    // I_chrg - Maximum charging current in mA 
        uint16_t iFull,    // I_full - End of charge current in mA
        uint8_t  nCells,   // N_cells - Number of Lithium-Ion cells
        void (*callback)(  // Callback function for controlling the PWM hardware
            uint8_t pwm,   // PWM duty cycle
            )
        );

    void loopHandler (
        uint32_t v, 
        uint32_t i
        );


  private:
    uint16_t iChrg;    // I_chrg - Maximum charging current in mA 
    uint16_t iFull;    // I_full - End of charge current in mA
    uint8_t  nCells;   // N_cells - Number of Lithium-Ion cells  
};
















#endif /* __LiCharger_H_ */
