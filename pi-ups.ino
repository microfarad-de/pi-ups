/* 
 * Uninterrruptible Power Supply (UPS) for a Raspberry Pi
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
 * Version: 1.0.0
 * Date:    May 2019
 */
#define VERSION_MAJOR 1  // major version
#define VERSION_MINOR 0  // minor version
#define VERSION_MAINT 0  // maintenance version

#include <avr/wdt.h>
#include <Arduino.h>
#include "Helper.h"
#include "LiCharger.h"
#include "Cli.h"
#include "ADC.h"
#include "Led.h"



/*
 * Pin assignment
 */
#define NUM_APINS                4  // Number of analog pins in use
#define V_IN_APIN         ADC_PIN0  // Analog pin for measuring V_in
#define V_BATT_APIN       ADC_PIN2  // Analog pin for measuring V_batt
#define V_UPS_APIN        ADC_PIN3  // Analog pin for measuring V_ups
#define CHG_MOSFET_PIN           3  // Digital pin with PWM support for controlling the MOSFET gate of the battery charging circuit
#define BATT_MOSFET_PIN          5  // Digital pin controlling the MOSFET gate for switching the battery power
#define IN_MOSFET_PIN            7  // Digital pin controlling the MOSFET gate for switching the external power
#define OUT_MOSFET_PIN           9  // Digital pin controlling the MOSFET gate for switching the output
#define LED_PIN                  2  // LED digital pin



/*
 * Configuration parameters
 */
#define SERIAL_BAUD      115200   // Serial communication baud rate
#define I_CHRG              600   // 600mA - Constant charging current in mA
#define ADC_AVG_SAMPLES      16   // Number of ADC samples to be averaged




/* 
 * Objects
 */
LedClass Led;
LiChargerClass LiCharger;

/*
 * State machine states
 */
enum State_t { STATE_INIT_E, STATE_INIT, STATE_EXTERNAL_E, STATE_EXTERNAL, STATE_BATTERY_E, STATE_BATTERY, 
                STATE_SHUTDOWN_E, STATE_SHUTDOWN, STATE_CALIBRATE_E, STATE_CALIBRATE, STATE_ERROR_E, STATE_ERROR };


/*
 * Global variables
 */
struct {
  State_t state = STATE_INIT_E; // Current state machine state
  uint32_t vIn;          // V_in - external power supply voltage in µV
  uint32_t vUps;         // V_ups - voltage at the output of the DC-DC converter in µV
  uint32_t vBatt;        // V_batt - Battery voltage in µV
  uint64_t iBatt;        // I_batt - Battery charging current in µA
  uint16_t vInRaw;       // Raw ADC value of V_in
  uint16_t vUpsRaw;      // Raw ADC value of V_ups
  uint16_t vBattRaw;     // Raw ADC value of V_batt
  char *stateStr = 0;    // State as human readable string
  bool crcOk = false;    // EEPROM CRC check was successful
} G;



/*
 * Parameters stored in EEPROM (non-volatile memory)
 */
struct {
  uint32_t vInCal;       // V_in_cal - Calibration constant for calculating V_in
  uint32_t vUpsCal;      // V_ups_cal - Calibration constant for calculating V_ups
  uint32_t vBattCal;     // V_batt_cal - Calibration constant for calculating V_batt
  uint16_t rShunt;       // R_shunt - Shunt resistor value in mΩ
  uint16_t vDiode;       // V_diode - charger diode voltage drop in mV
  uint32_t crc;          // CRC checksum
} Nvm;



/*
 * Strings to be reused for saving memory
 */
const struct {
  char *R_shunt    = (char *)"R_shunt    = %u mΩ\n";
  char *V_diode    = (char *)"V_diode    = %u mV\n";
  char *V_in_cal   = (char *)"V_in_cal   = %lu\n";
  char *V_ups_cal  = (char *)"V_ups_cal  = %lu\n";
  char *V_batt_cal = (char *)"V_batt_cal = %lu\n";
  char *CRC        = (char *)"CRC        = %lx\n";
  char *INIT       = (char *)"INIT";
  char *EXTERN     = (char *)"EXTERNAL";
  char *BATTERY    = (char *)"BATTERY";
  char *SHUTDOWN   = (char *)"SHUTDOWN";
  char *CALIBRATE  = (char *)"CALIBRATE";
  char *ERR        = (char *)"ERROR";
} Str;


/*
 * Arduino initalization routine
 */
void setup (void) {

  MCUSR = 0;      // clear MCU status register
  wdt_disable (); // and disable watchdog

  // Initialize pins
  pinMode (CHG_MOSFET_PIN, OUTPUT);
  pinMode (IN_MOSFET_PIN, OUTPUT);
  pinMode (OUT_MOSFET_PIN, OUTPUT);
  pinMode (BATT_MOSFET_PIN, OUTPUT);
  analogWrite (CHG_MOSFET_PIN, 255);     // Active low: max duty cycle means the MOSFET is off
  digitalWrite (IN_MOSFET_PIN, LOW);     // Active low: LOW means the MOSFET is on
  digitalWrite (OUT_MOSFET_PIN, LOW);    // Active low: LOW means the MOSFET is on
  digitalWrite (BATT_MOSFET_PIN, HIGH);  // Active low: HIGH means the MOSFET is off
  
  
  // Initialize the command-line interface
  Cli.init ( SERIAL_BAUD );
  Cli.xputs ("");
  Cli.xputs ("+ + +  P I  U P S  + + +");
  Cli.xputs ("");
  Cli.xprintf ("V %d.%d.%d\n", VERSION_MAJOR, VERSION_MINOR, VERSION_MAINT);
  Cli.xputs ("");
  Cli.newCmd ("poll", "Automated poll", cmdPoll);
  Cli.newCmd (".", "Show status", cmdStatus);
  Cli.newCmd ("e", "Show EEPROM", cmdEEPROM);
  Cli.newCmd ("cal", "Calibrate (cal [vin|vups|vbatt])", cmdCal);
  Cli.newCmd ("rshunt", "Set R_shunt in mΩ", cmdRshunt);
  Cli.newCmd ("vdiode", "Set V_diode in mV", cmdVdiode);
  Cli.showHelp ();

  // Initialize the ADC
  ADConv.initialize (ADC_PRESCALER_128, ADC_INTERNAL, NUM_APINS, ADC_AVG_SAMPLES);

  // Initialize the battery charger
  LiCharger.initialize (1, I_CHRG, liChargerCB);

  // Initialize the LED
  Led.initialize (LED_PIN);
  Led.blink (-1, 500, 1500);

  // Read the settings from EEPROM
  nvmRead ();


  // Enable the watchdog
  wdt_enable (WDTO_1S);
}


/*
 * Arduino main loop
 */
void loop (void) {

  // Reset the watchdog timer
  wdt_reset ();

  // Command-line interpreter
  Cli.getCmd ();

  // Update the LED state
  Led.loopHandler ();


  // Read the ADC channels
  adcRead ();

  // Update the battery charger state
  if (G.state == STATE_EXTERNAL) LiCharger.loopHandler (G.vBatt, G.iBatt );

  // Force the error STATE if CRC error occurred
  if (!G.crcOk && G.state != STATE_ERROR) Cli.xputs ("CRC error\n"), G.state = STATE_ERROR_E;

  // Main state machine
  switch (G.state) {

    case STATE_INIT_E:
      G.stateStr = Str.INIT;
      G.state = STATE_INIT;
    case STATE_INIT:
      G.state = STATE_EXTERNAL_E;
      break;
      
    case STATE_EXTERNAL_E:
      LiCharger.start ();
      G.stateStr = Str.EXTERN;
      G.state = STATE_EXTERNAL;
    case STATE_EXTERNAL:
      break;

    case STATE_BATTERY_E:
      G.stateStr = Str.BATTERY;
      G.state = STATE_BATTERY;
    case STATE_BATTERY:
      break;

    case STATE_SHUTDOWN_E:
      G.stateStr = Str.SHUTDOWN;
      G.state = STATE_SHUTDOWN;
    case STATE_SHUTDOWN:
      break;

    case STATE_CALIBRATE_E:
      LiCharger.stop ();
      G.stateStr = Str.CALIBRATE;
      G.state = STATE_CALIBRATE;
    case STATE_CALIBRATE:
      // Do nothing and wait for a CLI command
      break;

    case STATE_ERROR_E:
      LiCharger.stop ();
      G.stateStr = Str.ERR;
      G.state = STATE_ERROR;
    case STATE_ERROR:
      // Do nothing and wait for a CLI command
      break;

    default:
      break;
    
  }

}





/*
 * Callback function in use by the battery charger
 * for setting the PWM value
 */
void liChargerCB (uint8_t pwm) {

  // Adjust the PWM value. The pin is active low, thus, we need to invert the PWM value.
  analogWrite (CHG_MOSFET_PIN, 255 - pwm);
}



/*
 * Read the ADC channels
 */
void adcRead (void) {
  bool result;

  // Read the ADC channels
  result = ADConv.readAll ();

  
  if (result) {
    // Get the ADC results
    G.vInRaw   = (uint16_t)ADConv.result[V_IN_APIN];
    G.vUpsRaw  = (uint16_t)ADConv.result[V_UPS_APIN];
    G.vBattRaw = (uint16_t)ADConv.result[V_BATT_APIN];

    // Calculate voltage and current
    G.vIn   = (uint32_t)G.vInRaw * Nvm.vInCal;
    G.vUps  = (uint32_t)G.vUpsRaw * Nvm.vUpsCal;
    G.vBatt = (uint32_t)G.vBattRaw * Nvm.vBattCal;
    G.iBatt = (( (uint64_t)G.vIn - (uint64_t)G.vBatt - (uint64_t)Nvm.vDiode*1000) * LiCharger.pwm * 1000) / 255 / Nvm.rShunt ;
    if (G.iBatt < 0) G.iBatt = 0;

  }
}




/*
 * Validate the settings
 * Called after reading or before writing EEPROM
 * Always fall-back to the safest possible values
 */
void nvmValidate (void) {
  if (Nvm.vInCal < 4000 || Nvm.vInCal > 40000) Nvm.vInCal = 40000;
  if (Nvm.vUpsCal < 4000 || Nvm.vUpsCal > 40000) Nvm.vUpsCal = 40000;
  if (Nvm.vBattCal < 4000 || Nvm.vBattCal > 40000) Nvm.vBattCal = 40000;
  if (Nvm.rShunt < 100 || Nvm.rShunt > 1000) Nvm.rShunt = 1000;
  if (Nvm.vDiode < 100 || Nvm.vDiode > 1000) Nvm.vDiode = 100;
}


/*
 * Read and validate EEPROM data
 */
void nvmRead (void) {
  uint32_t crc;
  
  eepromRead (0x0, (uint8_t*)&Nvm, sizeof (Nvm)); 
  nvmValidate ();

  // Calculate and check CRC checksum
  crc = crcCalc ((uint8_t*)&Nvm, sizeof (Nvm) - sizeof (Nvm.crc) );
  Cli.xprintf (Str.CRC, crc);
  Cli.xputs("\n");
  
  if (crc != Nvm.crc) G.crcOk = false;
  else G.crcOk = true;
}


/*
 * Write and validate EEPROM data
 */
void nvmWrite (void) {
  nvmValidate (); 
  Nvm.crc = crcCalc ((uint8_t*)&Nvm, sizeof (Nvm) - sizeof (Nvm.crc) );
  eepromWrite (0x0, (uint8_t*)&Nvm, sizeof (Nvm));
}





/*
 * CLI command for periodically polling of the UPS status
 * by the Raspberry Pi
 */
int cmdPoll (int argc, char **argv) {

  return 0;
}



/*
 * CLI command for showing the overall status
 */
int cmdStatus (int argc, char **argv) {
  Cli.xprintf ("state      = %s\n", G.stateStr);
  Cli.xprintf ("V_in       = %lu mV\n", G.vIn / 1000);
  Cli.xprintf ("V_ups      = %lu mV\n", G.vUps / 1000);
  Cli.xprintf ("V_batt     = %lu mV\n", G.vBatt / 1000);
  Cli.xprintf ("I_batt     = %lu mA\n", G.iBatt / 1000);
  Cli.xprintf ("PWM        = %u\n", LiCharger.pwm);
  Cli.xprintf ("V_in_raw   = %u\n", G.vInRaw);
  Cli.xprintf ("V_ups_raw  = %u\n", G.vUpsRaw);
  Cli.xprintf ("V_batt_raw = %u\n", G.vBattRaw);
  Cli.xputs ("");
  return 0;
}



/*
 * CLI command for displaying the EEPROM settings
 */
int cmdEEPROM (int argc, char **argv) {
  Cli.xprintf (Str.V_in_cal,   Nvm.vInCal);
  Cli.xprintf (Str.V_ups_cal,  Nvm.vUpsCal);
  Cli.xprintf (Str.V_batt_cal, Nvm.vBattCal);
  Cli.xprintf (Str.R_shunt,    Nvm.rShunt);
  Cli.xprintf (Str.V_diode,    Nvm.vDiode);
  Cli.xprintf (Str.CRC,        Nvm.crc);
  Cli.xputs ("");
  return 0;
}


/*
 * CLI command for setting the shunt resistor value
 * argv[1]: shunt resistance in mΩ
 */
int cmdRshunt (int argc, char **argv) {
  Nvm.rShunt = atoi (argv[1]);
  nvmWrite ();
  Cli.xprintf(Str.R_shunt, Nvm.rShunt);
  Cli.xputs("");
  return 0;
}


/*
 * CLI command for setting the charger diode voltage drop
 * argv[1]: shunt voltage in mV
 */
int cmdVdiode (int argc, char **argv) {
  Nvm.vDiode = atoi (argv[1]);
  nvmWrite ();
  Cli.xprintf(Str.V_diode, Nvm.vDiode);
  Cli.xputs("");
  return 0;
}



/*
 * CLI command for calibrating V_in, V_ups and V_batt
 * argv[1]:
 *   vin   : calibrate V_in
 *   vups  : calibrate V_ups
 *   vbatt : calibrate V_batt
 * 
 * argv[2]: 
 *   Measured reference voltage in mV
 */
int cmdCal (int argc, char **argv) {
  if (G.state == STATE_CALIBRATE) {
    uint32_t vRef = (uint32_t)atoi(argv[2]) * 1000; 
    if      (strcmp(argv[1], "vin"  ) == 0) calVin (vRef);
    else if (strcmp(argv[1], "vups" ) == 0) calVups (vRef);
    else if (strcmp(argv[1], "vbatt") == 0) calVbatt (vRef);
    else    G.state = STATE_INIT_E, Cli.xputs ("Cal. mode end\n");
  }
  else {
    G.state = STATE_CALIBRATE_E;
    Cli.xputs ("Cal. mode begin\n");        
    //Cli.xprintf ("V_in_ref   = %lu mV\n", V_IN_REF / 1000);
    //Cli.xprintf ("V_ups_ref  = %lu mV\n", V_UPS_REF / 1000);
    //Cli.xprintf ("V_batt_ref = %lu mV\n\n", V_BATT_REF / 1000);
  }
  return 0;
}


/*
 * Calibrate V_in
 */
void calVin (uint32_t vRef) {
  Nvm.vInCal = (uint32_t)vRef / (uint32_t)G.vInRaw;
  nvmWrite ();
  Cli.xprintf (Str.V_in_cal, Nvm.vInCal);
  Cli.xputs ("");
}


/*
 * Calibrate V_ups
 */
void calVups (uint32_t vRef) {
  Nvm.vUpsCal = (uint32_t)vRef / (uint32_t)G.vUpsRaw;
  nvmWrite ();
  Cli.xprintf (Str.V_ups_cal, Nvm.vUpsCal);
  Cli.xputs ("");
}


/*
 * Calibrate V_batt
 */
void calVbatt (uint32_t vRef) {
  Nvm.vBattCal = (uint32_t)vRef / (uint32_t)G.vBattRaw;
  nvmWrite ();
  Cli.xprintf (Str.V_batt_cal, Nvm.vBattCal);
  Cli.xputs ("");
}
