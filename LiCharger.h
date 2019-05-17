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
  LI_CHARGER_STATE_STANDBY_E,    // Standby (state entry)
  LI_CHARGER_STATE_STANDBY,      // Standby
  LI_CHARGER_STATE_CHARGE_E,     // Charging (state entry)
  LI_CHARGER_STATE_CHARGE,       // Charging
};

/*
 * Lithium-Ion battery charger class
 */
class LiChargerClass {
  
  public:
    /*
     * Initialization function
     */
    void initialize (
        uint8_t  nCells,      // N_cells - Number of Lithium-Ion cells
        uint16_t iChrg,       // I_chrg - Constant charging current in mA 
        uint16_t iFull,       // I_full - End of charge current in mA
        void (*callbackFct)(  // Callback function for controlling the PWM hardware
            uint8_t pwm       // PWM duty cycle
            )
        );

    /*
     * This function must be called within the Arduino main loop
     * it provides the Li-Ion charger with the latest voltage and 
     * current readings
     */
    void loopHandler (
        uint32_t v,        // V - battery voltage in µV
        uint32_t i         // I - battery current in µA
        );


    uint8_t  nCells = 1;      // N_cells - Number of Lithium-Ion cells
    uint16_t iChrg = 0;       // I_chrg - Maximum charging current in mA 
    uint16_t iFull = 150;     // I_full - End of charge current in mA
    uint8_t  pwm = 0;         // PWM duty cycle (0..255)
    LiChargerState_t state = LI_CHARGER_STATE_STANDBY_E;  // Charger state

  private:
    
    void (*callbackFct)(uint8_t pwm);
    uint32_t updateTs = 0;
    uint32_t chargeTs = 0;
    uint32_t fullTs   = 0;
    uint32_t iMax = 0;
    bool safeCharge = true;
};







#endif /* __LiCharger_H_ */
