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
 * Version: 2.3.0
 * Date:    September 2020
 */
#define VERSION_MAJOR 2  // major version
#define VERSION_MINOR 3  // minor version
#define VERSION_MAINT 0  // maintenance version

#include <avr/wdt.h>
#include <avr/sleep.h>
#include <Arduino.h>
#include "src/Adc/Adc.h"
#include "src/Cli/Cli.h"
#include "src/Led/Led.h"
#include "src/LiCharger/LiCharger.h"
#include "src/MathMf/MathMf.h"
#include "src/Nvm/Nvm.h"



/*
 * Pin assignment
 */
#define NUM_APINS                3  // Number of analog pins in use
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
#define SERIAL_BAUD           19200   // Serial communication baud rate
#define I_CHRG                  500   // 500mA - Constant charging current in mA
#define ADC_AVG_SAMPLES          16   // Number of ADC samples to be averaged
#define V_IN_THR_BATTERY    4900000   // 4.85 V - V_in thershold in µV below which the UPS will switch to battery power
#define V_IN_THR_EXTERNAL   5000000   // 5.0 V - V_in threshold in µV above which the UPS will switch back to external power
#define V_BATT_THR_75       3800000   // 3.8 V - V_batt threshold in µV that roughly corresponds to 75% battery charge
#define V_BATT_THR_50       3600000   // 3.6 V - V_batt threshold in µV that roughly corresponds to 50% battery charge
#define V_BATT_THR_25       3400000   // 3.4 V - V_batt threshold in µV that roughly corresponds to 25% battery charge
#define V_BATT_THR_LOW      3200000   // 3.2 V - V_batt threshold in µV for initiating a system shutdown
#define V_BATT_THR_ERROR    2400000   // 2.4 V - V_batt threshold in µV for signalling a battery error
#define V_UPS_THR_ERROR     4900000   // 4.9 V - V_ups threshold in µV for signalling a DC-DC converter error
#define V_BATT_HYST_THR       15000   // 0.015 V - Hysteresis threshold in µV for the displayed V_batt via the 'stat' command
#define V_MEAS_HYST_THR       15000   // 0.015 V - Hysteresis threshold in µV for the output of the 'meas' command
#define INITIAL_DELAY           500   // Initial power on delay in ms
#define EXTERNAL_DELAY         1000   // Delay in ms prior to switching back to external power
#define SHUTDOWN_DELAY        60000   // Delay in ms prior to turning off power upon system shutdown
#define RESTART_DELAY          5000   // Delay in ms prior to restarting the system following a shutdown
#define DCDC_ERROR_DELAY        500   // Delay in milliseconds prior to registering a DC-DC converter error
#define CALIBRATE_EXIT_DELAY   3600   // Delay in seconds prior to automatically exiting the calibration mode


/*
 * Objects
 */
LedClass Led;
LiChargerClass LiCharger;


/*
 * State machine states
 */
enum State_t {
  STATE_INIT_E,
  STATE_INIT,
  STATE_EXTERNAL_E,
  STATE_EXTERNAL,
  STATE_BATTERY_E,
  STATE_BATTERY,
  STATE_CALIBRATE_E,
  STATE_CALIBRATE,
  STATE_ERROR_E,
  STATE_ERROR
};


/*
 * A list of error codes
 * Each code sets a different bit. Codes can be combined via addition.
 */
enum Error_t {
  ERROR_NONE    = 0,   // No errors
  ERROR_BATTERY = 1,   // Battery error
  ERROR_DCDC    = 2,   // DC-DC converter error
  ERROR_CRC     = 128  // CRC error
};


/*
 * Battery states
 */
enum BattState_t {
  BATT_STATE_0   = 0,    // Low battery
  BATT_STATE_25  = 25,   // 25%
  BATT_STATE_50  = 50,   // 50%
  BATT_STATE_75  = 75,   // 75%
  BATT_STATE_100 = 100   // 100%
};

/*
 * Raspberry Pi watchdog timer states
 */
enum WdState_t {
  WD_STATE_DISABLED  = 0,  // Watchdog timer disabled
  WD_STATE_ENABLED   = 1,  // Watchdog timer enabled
  WD_STATE_TRIGGERED = 2   // Watchdog timer has been triggered
};

/*
 * Global variables
 */
struct {
  State_t state     = STATE_INIT_E; // Current state machine state
  State_t lastState = STATE_INIT;   // Previous state
  uint32_t vIn;          // V_in - external power supply voltage in µV
  uint32_t vUps;         // V_ups - voltage at the output of the DC-DC converter in µV
  uint32_t vBatt;        // V_batt - Battery voltage in µV
  uint64_t iBatt;        // I_batt - Battery charging current in µA
  uint32_t vInAvg   = 0; // Average value of V_in in mV
  uint32_t vUpsAvg  = 0; // Average value of V_ups in mV
  uint32_t vBattAvg = 0; // Average value of V_batt in mV
  uint32_t iBattAvg = 0; // Average value if I_batt in mA
  uint32_t avgCount = 0; // Number of averaged values
  uint32_t watchdogTs;   // Time stamp for measuring the RPi watchdog timer duration
  uint16_t vInRaw;       // Raw ADC value of V_in
  uint16_t vUpsRaw;      // Raw ADC value of V_ups
  uint16_t vBattRaw;     // Raw ADC value of V_batt
  BattState_t battState; // Battery state
  uint8_t error = 0;     // Error code
  bool errorSeen = false; // Tells if errors have been seen and can be cleared
  bool shutdown = false; // System shutdown command flag
  bool testMode = false; // UPS test mode activation flag
  bool statRcvd = false; // Set to true when the "stat" command has been received
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
  WdState_t watchdog;    // RPi watchdog timer status - if enabled, the RPi will be rebooted if
                         // the stat command has not been received within wdDuration
  uint32_t wdDuration;   // RPi watchdog timer duration in hours
  uint32_t crc;          // CRC checksum
} Nvm;



/*
 * Strings to be reused for saving memory
 */
const struct {
  char *R_shunt     = (char *)"R_shunt    = %umΩ\n";
  char *V_diode     = (char *)"V_diode    = %umV\n";
  char *V_in_cal    = (char *)"V_in_cal   = %lu\n";
  char *V_ups_cal   = (char *)"V_ups_cal  = %lu\n";
  char *V_batt_cal  = (char *)"V_batt_cal = %lu\n";
  char *watchdog    = (char *)"watchdog   = %u (%uh)\n";
  char *CRC         = (char *)"CRC        = %lx\n";
} Str;



/*
 * Function prototypes
 */
void changeState (State_t);
void raiseError (Error_t error);
void clearError (Error_t error);
void shutdown (void);
void powerSave (void);
void liChargerCB (uint8_t pwm);
void adcRead (void);
void nvmValidate (void);
void nvmRead (void);
void nvmWrite (void);
void checkBattState (void);
void watchdogReset (void);
void printState (void);
void printBriefStatus (void);
int cmdStat (int argc, char **argv);
int cmdMeas (int argc, char **argv);
int cmdHalt (int argc, char **argv);
int cmdWatchdog (int argc, char **argv);
int cmdTest (int argc, char **argv);
int cmdStatus (int argc, char **argv);
int cmdEEPROM (int argc, char **argv);
int cmdRshunt (int argc, char **argv);
int cmdVdiode (int argc, char **argv);
int cmdCal (int argc, char **argv);
void calVin (uint32_t vRef);
void calVups (uint32_t vRef);
void calVbatt (uint32_t vRef);





/*
 * Arduino initalization routine
 */
void setup (void) {

  MCUSR = 0;      // clear MCU status register
  wdt_disable (); // and disable watchdog

  // Initialize the Timer 2 PWM frequency for pins 3 and 11
  // see https://etechnophiles.com/change-frequency-pwm-pins-arduino-uno/
  // see ATmega328P datasheet Section 20.11.2, Table 22-10
  TCCR2B = (TCCR2B & B11111000) | B00000001; // for PWM frequency of 31250 Hz

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
  Serial.println ("");
  Serial.println (F("+ + +  P I  U P S  + + +"));
  Serial.println ("");
  Cli.xprintf ("V %d.%d.%d\n", VERSION_MAJOR, VERSION_MINOR, VERSION_MAINT);
  Serial.println ("");

  // Read the settings from EEPROM
  nvmRead ();

  if (Nvm.watchdog != WD_STATE_DISABLED) {
    Serial.println (F("Watchdog timer enabled!"));
    Serial.print (F("Power cycle after "));
    Serial.print (Nvm.wdDuration, DEC);
    Serial.println (F(" hours"));
    Serial.println (F("if no 'stat' command received"));
    Serial.println ("");
  }
  Cli.xputs ("'h' for help\n");
  Cli.newCmd ("stat", "Brief status", cmdStat);
  Cli.newCmd ("s", "", cmdStat);
  Cli.newCmd ("status", "Detailed status", cmdStatus);
  Cli.newCmd (".", "", cmdStatus);
  Cli.newCmd ("rom", "EEPROM status", cmdEEPROM);
  Cli.newCmd ("r", "", cmdEEPROM);
  Cli.newCmd ("meas", "Measurement summary", cmdMeas);
  Cli.newCmd ("m", "", cmdMeas);
  Cli.newCmd ("halt", "Shutdown (arg: [abort])", cmdHalt);
  Cli.newCmd ("test", "Test (arg: [abort])", cmdTest);
  Cli.newCmd ("rshunt", "Set R_shunt (arg: <mΩ>)", cmdRshunt);
  Cli.newCmd ("vdiode", "Set V_diode (arg: <mV>)", cmdVdiode);
  Cli.newCmd ("cal", "Calibrate (arg: <start|stop|vin|vups|vbatt> [mV])", cmdCal);
  Cli.newCmd ("wd", "Watchdog (arg: <enable|disable> [hours])", cmdWatchdog);
  //Cli.showHelp ();

  // Initialize the ADC
  AdcPin_t adcPins[NUM_APINS] = {V_IN_APIN, V_BATT_APIN, V_UPS_APIN};
  Adc.initialize (ADC_PRESCALER_128, ADC_INTERNAL, ADC_AVG_SAMPLES, NUM_APINS, adcPins);

  // Initialize the battery charger
  LiCharger.initialize (1, I_CHRG, liChargerCB);

  // Initialize the LED
  Led.initialize (LED_PIN);

  // Initialize Raspberry Pi watchdog timer
  watchdogReset ();

  // Enable the watchdog
  wdt_enable (WDTO_1S);
}


/*
 * Arduino main loop
 */
void loop (void) {
  static uint32_t delayTs;  // Timestamp for measuring delays
  uint32_t ts = millis ();  // General purpose millisecond timestamp

  // Reset the watchdog timer
  wdt_reset ();

  // Command-line interpreter
  Cli.getCmd ();

  // Update the LED state
  Led.loopHandler ();

  // Read the ADC channels
  adcRead ();

  // Update the battery charger state
  LiCharger.loopHandler (G.vBatt, G.iBatt);

  // Check the battery state
  checkBattState ();

  // Handle shutdown command
  shutdown ();

  // Handle the RPi watchdog timer expiry
  if (Nvm.watchdog != WD_STATE_DISABLED && millis() - G.watchdogTs > Nvm.wdDuration * 3600000 ) {
    Nvm.watchdog = WD_STATE_TRIGGERED;
    nvmWrite ();
    G.shutdown = true;
    watchdogReset ();
  }

  // Send the CPU into sleep mode
  //powerSave ();

  // Main state machine
  switch (G.state) {

    // System initialization
    case STATE_INIT_E:
      G.shutdown = false;
      delayTs = ts;
      G.state = STATE_INIT;
    case STATE_INIT:
      // Wait for the ADC to stabilize before starting-up
      if (ts - delayTs > (uint32_t)INITIAL_DELAY) {
        digitalWrite (OUT_MOSFET_PIN, LOW);   // Activate output power
        changeState (STATE_EXTERNAL_E);
      }
      break;


    // Running on external power
    case STATE_EXTERNAL_E:
      LiCharger.start ();                   // Start battery charging
      digitalWrite (IN_MOSFET_PIN, LOW);    // Activate external power
      digitalWrite (BATT_MOSFET_PIN, HIGH); // Deactivate battery power
      G.statRcvd = false;
      G.state = STATE_EXTERNAL;
    case STATE_EXTERNAL:
      if (!G.shutdown) {
        if (G.statRcvd) {
          // Additional blink whenever the "stat" command is received
          Led.blink (1, 100, 100);
          G.statRcvd = false;
        }
        Led.blink (-1, 100, 9900);
      }
      // Switch to battery power if V_in is below the specified threshold
      if (G.vIn < (uint32_t)V_IN_THR_BATTERY) {
        changeState (STATE_BATTERY_E);
      }
      // Check for error conditions
      if (G.error != ERROR_NONE) {
        changeState (STATE_ERROR_E);
      }
      break;


    // Running on battery power
    case STATE_BATTERY_E:
      LiCharger.stop ();                   // Stop battery charging
      digitalWrite (BATT_MOSFET_PIN, LOW); // Activate battery power
      digitalWrite (IN_MOSFET_PIN, HIGH);  // Deactivate external power
      delayTs = ts;
      G.state = STATE_BATTERY;
    case STATE_BATTERY:
      if (!G.shutdown) {
        // Adapt the LED blinking duty cycle according to batter voltage
        if      (G.battState == BATT_STATE_0)  Led.blink (-1, 100, 100);
        else if (G.battState == BATT_STATE_25) Led.blink (-1, 250, 750);
        else if (G.battState == BATT_STATE_50) Led.blink (-1, 500, 500);
        else if (G.battState == BATT_STATE_75) Led.blink (-1, 750, 250);
        else                                   Led.blink (-1, 1000, 0);
      }
      // Switch back to external power if V_in is above the specified threshold during EXTERNAL_DELAY
      if (G.vIn < (uint32_t)V_IN_THR_EXTERNAL) delayTs = ts;
      if (ts - delayTs > EXTERNAL_DELAY) {
        changeState (STATE_EXTERNAL_E);
      }
      break;


    // Voltage calibration mode
    case STATE_CALIBRATE_E:
      Led.blink (-1, 500, 1500);
      LiCharger.stop ();                    // Stop battery charging
      digitalWrite (OUT_MOSFET_PIN, LOW);   // Activate output power
      digitalWrite (IN_MOSFET_PIN, LOW);    // Activate external power
      digitalWrite (BATT_MOSFET_PIN, HIGH); // Deactivate battery power
      G.shutdown = false;
      delayTs = ts;
      G.state = STATE_CALIBRATE;
    case STATE_CALIBRATE:
      // Exit the calibration state after some time to avoid being
      // inadvertently stuck in calibration mode
      if (ts - delayTs > (int32_t)CALIBRATE_EXIT_DELAY * 1000) {
        changeState (STATE_EXTERNAL_E);
      }
      break;


    // Failsafe mode for handling error conditions
    case STATE_ERROR_E:
      LiCharger.stop ();                    // Stop battery charging
      digitalWrite (IN_MOSFET_PIN, LOW);    // Activate external power
      digitalWrite (BATT_MOSFET_PIN, HIGH); // Deactivate battery power
      G.state = STATE_ERROR;
    case STATE_ERROR:
      if (!G.shutdown) {
        Led.blink (-1, 200, 200);
      }
      // Exit error state if no errors
      if (G.error == ERROR_NONE) {
        changeState (STATE_EXTERNAL_E);
      }
      break;

    default:
      break;

  }

}



/*
 * Perform a state machine state transition
 */
void changeState (State_t state) {
  G.lastState = G.state;
  G.state = state;
}



/*
 * Raise an error condition
 */
void raiseError (Error_t error) {
  if ((G.error & (uint8_t)error) == 0) {
    G.error    |= (uint8_t)error;
    G.errorSeen = false;
    if (G.state == STATE_INIT || G.state == STATE_EXTERNAL) {
      changeState (STATE_ERROR_E);
    }
  }
}



/*
 * Clear an error condition
 */
void clearError (Error_t error) {
  // Ensure that the error codes have been displayed before clearing
  if ((G.error & (uint8_t)error) != 0 && G.errorSeen) {
    G.error &= ~(uint8_t)error;
  }
}



/*
 * Shutdown routine
 * Initiates a Raspberry Pi power cycle
 */
void shutdown (void) {
  static uint32_t shutdownTs;
  uint32_t ts = millis ();

  if (G.shutdown) {
    Led.blink (-1, 50, 50);
    G.testMode = false;
    G.statRcvd = false;

    if (digitalRead (OUT_MOSFET_PIN) == LOW) {
      // Power down if SHUTDOWN_DELAY has elapsed
      if (ts - shutdownTs > SHUTDOWN_DELAY) {
        digitalWrite (OUT_MOSFET_PIN, HIGH);  // Deactivate output power
        shutdownTs = ts;
      }
    }
    else {
      // Power up if RESTART_DELAY has elapsed and not on battery power
      if (ts - shutdownTs > RESTART_DELAY && G.state != STATE_BATTERY) {
        digitalWrite (OUT_MOSFET_PIN, LOW);  // Activate output power
        G.shutdown = false;
      }
    }
  }
  else {
    if (digitalRead (OUT_MOSFET_PIN) == HIGH) {
      digitalWrite (OUT_MOSFET_PIN, LOW); // Activate output power
    }
    shutdownTs = ts;
  }
}


/*
 * Send the CPU Into Light Sleep Mode
 *
 * The CPU will be waken-up within 1 millisecond by the Timer 0 millis() interrupt.
 *
 * Please refer to ATmega328P datasheet Section 14.2. "Sleep Modes" for more
 * information about the different sleep modes.
 */
void powerSave (void) {
  set_sleep_mode (SLEEP_MODE_IDLE);  // Configure lowest sleep mode that keeps UART active
  cli ();                            // Disable interrupts
  sleep_enable ();                   // Prepare for sleep
  sei ();                            // Enable interrupts
  sleep_cpu ();                      // Send the CPU into sleep mode
  sleep_disable ();                  // CPU will wake-up here
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
  result = Adc.readAll ();


  if (result) {

    // Get the ADC results
    G.vInRaw   = (uint16_t)Adc.result[V_IN_APIN];
    G.vUpsRaw  = (uint16_t)Adc.result[V_UPS_APIN];
    G.vBattRaw = (uint16_t)Adc.result[V_BATT_APIN];

    // Calculate voltage and current
    G.vIn   = (uint32_t)G.vInRaw * Nvm.vInCal;
    G.vUps  = (uint32_t)G.vUpsRaw * Nvm.vUpsCal;
    G.vBatt = (uint32_t)G.vBattRaw * Nvm.vBattCal;
    G.iBatt = (( (uint64_t)G.vIn - (uint64_t)G.vBatt - (uint64_t)Nvm.vDiode*1000) * LiCharger.pwm * 1000) / 255 / Nvm.rShunt ;
    if (G.iBatt < 0) G.iBatt = 0;

    // Simulate low input voltage during test mode
    if (G.testMode) G.vIn = 0;

    // Accumulate avarage values
    if (G.avgCount < 10000) {
      G.vInAvg   += G.vIn / 1000;
      G.vUpsAvg  += G.vUps / 1000;
      G.vBattAvg += G.vBatt / 1000;
      G.iBattAvg += G.iBatt / 1000;
      G.avgCount++;
    }

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
  if (Nvm.rShunt < 100 || Nvm.rShunt > 5000) Nvm.rShunt = 100;
  if (Nvm.vDiode < 100 || Nvm.vDiode > 1000) Nvm.vDiode = 100;
  if ((uint32_t)Nvm.watchdog > WD_STATE_TRIGGERED) Nvm.watchdog = WD_STATE_DISABLED;
  if (Nvm.wdDuration < 1) Nvm.wdDuration = 36;
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
  Cli.xputs ("");

  if (crc != Nvm.crc) {
    Serial.println (F("CRC error"));
    Serial.println ("");
    raiseError (ERROR_CRC);
  }
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
 * Check the battery state
 */
void checkBattState (void) {
  static HysteresisClass Hyst;
  // Apply a hysteresis to the battery voltage
  uint32_t vBatt = Hyst.apply (G.vBatt, (int32_t)V_BATT_HYST_THR);

  // Check the battery voltage
  if      (vBatt < (uint32_t)V_BATT_THR_LOW) G.battState = BATT_STATE_0;
  else if (vBatt < (uint32_t)V_BATT_THR_25)  G.battState = BATT_STATE_25;
  else if (vBatt < (uint32_t)V_BATT_THR_50)  G.battState = BATT_STATE_50;
  else if (vBatt < (uint32_t)V_BATT_THR_75)  G.battState = BATT_STATE_75;
  else                                       G.battState = BATT_STATE_100;

  if (G.state != STATE_INIT_E && G.state != STATE_INIT) {

    // Check for battery error
    if  (G.vBatt < V_BATT_THR_ERROR) {
      raiseError (ERROR_BATTERY);
    }
    // Clear error condition if battery voltage becomes ok
    else {
      clearError (ERROR_BATTERY);
    }

    // Check for DC-DC converter error
    if (G.vUps < V_UPS_THR_ERROR) {
      raiseError (ERROR_DCDC);
    }
    // Clear error condition if the DC-DC converter voltage becomes ok
    else {
      clearError (ERROR_DCDC);
    }
  }
}


/*
 * Reset the Raspberry Pi watchdog timer
 */
void watchdogReset (void) {
  G.watchdogTs = millis ();
}


/*
 * Convert state to string and print it
 */
void printState (State_t state, bool printValues) {
  if (state == STATE_INIT) {
    Serial.print(F("INIT"));
  }
  else if (state == STATE_EXTERNAL) {
    Serial.print(F("EXTERNAL"));
  }
  else if (state == STATE_BATTERY) {
    Serial.print(F("BATTERY"));
    if (printValues) Cli.xprintf(" %u%%", G.battState);
  }
  else if (state == STATE_CALIBRATE) {
    Serial.print(F("CALIBRATE"));
  }
  else if (state == STATE_ERROR) {
    Serial.print (F("ERROR"));
    if (printValues) Cli.xprintf (" %u", G.error);
  }
}



/*
 * Print system state string
 */
void printBriefStatus (void) {
  static State_t lastState = STATE_INIT_E;
  static HysteresisClass Hyst;

  printState (G.state, true);
  // Print the last state upon state transition
  if (lastState != G.lastState) {
    Cli.xprintf (" (");
    printState (G.lastState, false);
    Cli.xprintf (")");
  }
  G.lastState = G.state;
  lastState = G.state;
  if (G.state == STATE_ERROR) {
    G.errorSeen = true;
  }
  if (G.shutdown) {
    Cli.xprintf (" SHUTDOWN %u", digitalRead (OUT_MOSFET_PIN));
  }
  if (G.testMode) {
    Serial.print (F(" TEST"));
  }
  if (LiCharger.state == LI_CHARGER_STATE_CHARGE) {
    Serial.print (F(" CHARGING"));
  }
  // Reduce voltage resolution and apply a hysteresis to avoid
  // frequent tracing upon voltage change
  uint32_t vBatt = Hyst.apply (G.vBatt, (int32_t)V_BATT_HYST_THR);
  uint8_t v1 = vBatt/1000000;
  uint8_t v10 = vBatt/100000 - v1*10;
  uint8_t v100 = vBatt/10000 - v1*100 - v10*10;
  //uint8_t v1000 = vBatt/1000 - v1*1000 - v10*100;
  //if      (v1000 < 25) v100 = 0;
  //else if (v1000 < 75) v100 = 5;
  //else    v100 = 0, v10++;
  //if      (v10 == 10) v10 = 0, v1++;
  Cli.xprintf (" %u.%u%uV\n", v1, v10, v100);
}


/*
 * CLI command reporting the brief system status
 */
int cmdStat (int argc, char **argv) {
  printBriefStatus ();
  G.statRcvd = true;
  // Reset the RPi watchdog timer
  watchdogReset ();
  return 0;
}



/*
 * CLI command reporting a brief voltage and current
 * measurement summary
 */
int cmdMeas (int argc, char **argv) {
  static HysteresisClass VInHyst, VUpsHyst, VBattHyst;

  Cli.xprintf ("%4umV ", VInHyst.apply   (G.vInAvg / G.avgCount,   (int32_t)V_MEAS_HYST_THR/1000) );
  Cli.xprintf ("%4umV ", VUpsHyst.apply  (G.vUpsAvg / G.avgCount,  (int32_t)V_MEAS_HYST_THR/1000) );
  Cli.xprintf ("%4umV ", VBattHyst.apply (G.vBattAvg / G.avgCount, (int32_t)V_MEAS_HYST_THR/1000) );
  Cli.xprintf ("%4umA ", G.iBattAvg / G.avgCount);
  Cli.xprintf ("%3u\n", LiCharger.pwm);
  G.vInAvg   = 0;
  G.vUpsAvg  = 0;
  G.vBattAvg = 0;
  G.iBattAvg = 0;
  G.avgCount = 0;
  return 0;
}



/*
 * CLI command for initiating a system shutdown
 * argv[1]:
 *   abort : abort the shutdown procedure
 */
int cmdHalt (int argc, char **argv) {
  if (strcmp(argv[1], "abort") == 0){
    Serial.println (F("Shutdown abort"));
    watchdogReset ();
    G.shutdown = false;
  }
  else {
   if (G.state != STATE_CALIBRATE_E && G.state != STATE_CALIBRATE) {
      Cli.xprintf ("SHUTDOWN %u\n", digitalRead (OUT_MOSFET_PIN));
      G.shutdown = true;
    }
  }
  return 0;
}

/*
 * CLI command for enabling the Raspberry Pi watchdog timer
 * The watchdog timer will trigger a power cycle if the stat command
 * has not been received during the wdDuration period.
 * argv[1]:
 *   enable <duration> : enable enable with duration
 *   disable           : disable the watchdog
 */
int cmdWatchdog (int argc, char **argv) {
  if (strcmp(argv[1], "enable") == 0) {
    if (argc == 3) {
      Nvm.wdDuration = (uint32_t)atoi(argv[2]);
    }
    Nvm.watchdog = WD_STATE_ENABLED;
    nvmWrite ();
    watchdogReset ();
    Cli.xprintf (Str.watchdog, Nvm.watchdog, Nvm.wdDuration);
  }
  else if (strcmp(argv[1], "disable") == 0) {
    Serial.println (F("Watchdog disabled"));
    Nvm.watchdog = WD_STATE_DISABLED;
    nvmWrite ();
  }
  return 0;
}

/*
 * CLI command for initiating the UPS test mode
 * argv[1]:
 *   abort : abort the UPS test mode
 */
int cmdTest (int argc, char **argv) {
  if (strcmp(argv[1], "abort") == 0) {
    Serial.println (F("Test abort"));
    G.testMode = false;
  }
  else if (!G.shutdown) {
    Serial.println (F("Test"));
    G.testMode = true;
  }
  return 0;
}

/*
 * CLI command for showing the detailed system status
 */
int cmdStatus (int argc, char **argv) {
  Cli.xprintf ("state      = ");
  printBriefStatus ();
  Cli.xprintf ("battery    = %u%%\n", G.battState);
  Cli.xprintf ("V_in       = %lumV\n", G.vIn / 1000);
  Cli.xprintf ("V_ups      = %lumV\n", G.vUps / 1000);
  Cli.xprintf ("V_batt     = %lumV\n", G.vBatt / 1000);
  Cli.xprintf ("I_batt     = %lumA\n", G.iBatt / 1000);
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
  Cli.xprintf (Str.watchdog,   Nvm.watchdog, Nvm.wdDuration);
  Cli.xprintf (Str.CRC,        Nvm.crc);
  Cli.xprintf ("V %d.%d.%d\n", VERSION_MAJOR, VERSION_MINOR, VERSION_MAINT);
  Cli.xputs ("");
  if (Nvm.watchdog == WD_STATE_TRIGGERED) {
    Nvm.watchdog = WD_STATE_ENABLED;
    nvmWrite ();
  }
  return 0;
}


/*
 * CLI command for setting the shunt resistor value
 * argv[1]: shunt resistance in mΩ
 */
int cmdRshunt (int argc, char **argv) {
  if (argc != 2) return 1;
  Nvm.rShunt = atoi (argv[1]);
  nvmWrite ();
  Cli.xprintf(Str.R_shunt, Nvm.rShunt);
  return 0;
}


/*
 * CLI command for setting the charger diode voltage drop
 * argv[1]: shunt voltage in mV
 */
int cmdVdiode (int argc, char **argv) {
  if (argc != 2) return 1;
  Nvm.vDiode = atoi (argv[1]);
  nvmWrite ();
  Cli.xprintf(Str.V_diode, Nvm.vDiode);
  return 0;
}



/*
 * CLI command for calibrating V_in, V_ups and V_batt
 * argv[1]:
 *   start : start calibration mode
 *   stop  : stop calibration mode
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
    if      (strcmp(argv[1], "vin"  ) == 0 && argc == 3) calVin (vRef);
    else if (strcmp(argv[1], "vups" ) == 0 && argc == 3) calVups (vRef);
    else if (strcmp(argv[1], "vbatt") == 0 && argc == 3) calVbatt (vRef);
    else if (strcmp(argv[1], "stop") == 0) {
      changeState (STATE_EXTERNAL_E);
      Serial.println (F("Calibration stop"));
    }
  }
  else if (strcmp(argv[1], "start") == 0 && (G.state == STATE_EXTERNAL || G.state == STATE_ERROR)) {
    changeState (STATE_CALIBRATE_E);
    Serial.println(F("Calibration start"));
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
}


/*
 * Calibrate V_ups
 */
void calVups (uint32_t vRef) {
  Nvm.vUpsCal = (uint32_t)vRef / (uint32_t)G.vUpsRaw;
  nvmWrite ();
  Cli.xprintf (Str.V_ups_cal, Nvm.vUpsCal);
}


/*
 * Calibrate V_batt
 */
void calVbatt (uint32_t vRef) {
  Nvm.vBattCal = (uint32_t)vRef / (uint32_t)G.vBattRaw;
  nvmWrite ();
  Cli.xprintf (Str.V_batt_cal, Nvm.vBattCal);
}
