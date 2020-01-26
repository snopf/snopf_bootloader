// Copyright (c) 2019-2020 Hajo Kortmann <hajo.kortmann@gmx.de>
// License: GNU GPL v2 (see License.txt)

#include "io_stuff.h"
#include <avr/interrupt.h>
#include <util/delay.h>

void io_init(void)
{
    // Set the Button pin as input
    DDRB &= ~(1 << IO_BUTTON_PIN);
    // Set the LED pin as output
    DDRB |= (1 << IO_LED_PIN);
    
    IO_LED_OFF;
}
