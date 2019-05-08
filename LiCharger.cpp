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

 #include "LiCharger.h"


/*
 * Configuration parameters
 */
#define V_MAX           4190000 // 4.19 V - Maximum allowed battery voltage per cell in µV
#define V_MIN           2500000 // 2.50 V - Minimum allowed battery voltage per cell in µV
#define V_START_MAX     4100000 // 4.10 V - Start charging below this voltage in µV
#define V_START_MIN     2500000 // 2.50 V - Start charging above this voltage in µV
#define V_SAFE          2800000 // 2.80 V - Chare with reduced current I_safe below this voltage per cell in µV
#define V_WINDOW           2000 // 0.002 V - Do not regulate voltage when within +/- this window (per cell) in µV
#define I_WINDOW          15000 // 0.015 A - Do not regulate current when within +/- this window in µA
#define I_SAFE_DIVIDER       10 // Divide I_chrg by this value to calculate I_safe, which is the reduced safety charging current
#define TIMEOUT_CHARGE     2000 // Time duration in ms during which V shall be between V_MIN and V_START before starting to charge
#define TIMEOUT_FULL      20000 // Time duration in ms during which I_full shall not be exceeded in order to assume that battery is full
#define TIMEOUT_UPDATE       50 // Time interval in ms for updating the output by one increment





void LiChargerClass::initialize ( uint8_t  nCells, uint16_t iChrg, uint16_t iFull, void (*callbackFct)(uint8_t pwm) ) {

  this->nCells = nCells;
  this->iChrg = iChrg;
  this->iFull = iFull;
  this->callbackFct = callbackFct;
  this->state = LI_CHARGER_STATE_STANDBY_E;
}



void LiChargerClass::loopHandler (uint32_t v, uint32_t i) {

  uint32_t ts = millis ();

  // Main state machine
  switch (state) {
    
    case LI_CHARGER_STATE_STANDBY_E:
      dutyCycle = 0;    
      state = LI_CHARGER_STATE_STANDBY;
      callbackFct (dutyCycle);
    case LI_CHARGER_STATE_STANDBY:
    
      // Start charging if V stays within bounds during TIMEOUT_CHARGE
      if ( v < (uint32_t)V_START_MIN * nCells || v > (uint32_t)V_START_MAX * nCells) chargeTs = ts;
      if (ts - chargeTs > TIMEOUT_CHARGE) {
        state = LI_CHARGER_STATE_CHARGE_E;
      }
      break;

    case LI_CHARGER_STATE_CHARGE_E:
      updateTs = ts;
      fullTs = ts;
      iMax = (uint32_t)iChrg * 1000 / I_SAFE_DIVIDER;
      safeCharge = true;
      state = LI_CHARGER_STATE_CHARGE;
    case LI_CHARGER_STATE_CHARGE:

      // CC-CV Regulation:
      // Run the regulation routine at the preset interval
      if (ts - updateTs > TIMEOUT_UPDATE) {
        updateTs = ts;

        // Regulate voltage and current with the CC-CV algorithm
        if ( ( v > (uint32_t)V_MAX * nCells + (uint32_t)V_WINDOW * nCells ) ||
             ( i > iMax + (uint32_t)I_WINDOW ) ) {
          if (dutyCycle > 0) dutyCycle--;
        }
        else if ( ( v < (uint32_t)V_MAX * nCells - (uint32_t)V_WINDOW * nCells ) &&
                  ( i < iMax - (uint32_t)I_WINDOW ) ) {
          if (dutyCycle < 255) dutyCycle++;
        }

        // Update the PWM duty cycle
        callbackFct (dutyCycle);
      }

      // Terminate safety charging if voltage is higher than V_SAFE
      if (v > (uint32_t)V_SAFE * nCells && safeCharge) {
        safeCharge = false;
        iMax = (uint32_t)iChrg * 1000;
      }

      // End of Charge Detection:
      // Report battery full if I_full has not been exceeded during TIMEOUT_FULL (ignore during safety charging)
      if ( i > (uint32_t)iFull * 1000 || safeCharge ) fullTs = ts;
      if (ts - fullTs > TIMEOUT_FULL) {
        state = LI_CHARGER_STATE_STANDBY_E; 
      }
      break;

    default:
      break;
    
  }
  
}
