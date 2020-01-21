// Copyright (c) 2018 Hajo Kortmann
// License: GNU GPL v2 (see License.txt)

#ifndef __io_stuff_h__
#define __io_stuff_h__

#include <avr/io.h>

// Some routines for using the Button / LED IO pin

// Only IO pin left
#define IO_BUTTON_PIN PB0

// Evaluates to 1 if button is pressed (set to GND)
#define IO_BUTTON_PRESSED (!(PINB & (1 << IO_BUTTON_PIN)))

// Activate the button
void io_btt_activate(void);

// Deactivate the button
void io_btt_deactivate(void);

// Shut down the device and just keep blinking the LED slowly
void io_failure_shutdown(void);

#endif
