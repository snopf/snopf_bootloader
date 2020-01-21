// Copyright (c) 2018 Hajo Kortmann
// License: GNU GPL v2 (see License.txt)

#include "io_stuff.h"
#include <avr/interrupt.h>
#include <util/delay.h>

void io_btt_activate(void)
{
    // Deactivate internal pullup
    PORTB &= ~(1 << IO_BUTTON_PIN);
    // Set the Button pin as input
    DDRB &= ~(1 << IO_BUTTON_PIN);
}

void io_btt_deactivate(void)
{
    // Set the Button pin as output
    DDRB |= (1 << IO_BUTTON_PIN);
    // Set to GND to deactivate LED
    PORTB &= ~(1 << IO_BUTTON_PIN);
}

void io_failure_shutdown(void)
{
    cli();
    while (1) {
        // led on
        io_btt_deactivate();
        PORTB |= (1 << IO_BUTTON_PIN);
        _delay_ms(500);
        // led off
        PORTB &= ~(1 << IO_BUTTON_PIN);
        _delay_ms(500);
    }
}
