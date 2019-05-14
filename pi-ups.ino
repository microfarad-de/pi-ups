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
#include "LiCharger.h"
#include "Cli.h"
#include "ADC.h"
#include "Led.h"


/*
 * Pin assignment
 */
#define LED_PIN        13      // LED pin



/* 
 * Objects
 */
LedClass Led;


/*
 * Arduino initalization routine
 */
void setup (void) {

  MCUSR = 0;      // clear MCU status register
  wdt_disable (); // and disable watchdog
  
  // Initialize the command-line interface
  Cli.init();
  Cli.xputs ("");
  Cli.xputs ("+ + +  P I  U P S  + + +");
  Cli.xputs ("");
  Cli.xprintf ("V %d.%d.%d\n", VERSION_MAJOR, VERSION_MINOR, VERSION_MAINT);
  Cli.xputs ("");
  Cli.newCmd ("test", "test routine", test);
  
  Cli.showHelp ();

  // Initialize LED
  Led.initialize (LED_PIN);
  Led.blink (-1, 500, 1500);

}


/*
 * Arduino main loop
 */
void loop (void) {

  // Command-line interpreter
  Cli.getCmd ();

  // Update the LED state
  Led.loopHandler ();

}

/*
 * CLI command
 */
int test (int argc, char **argv) {
  Cli.xprintf ("Testing CLI: %s\n", argv[1]);
  return 0;
}
