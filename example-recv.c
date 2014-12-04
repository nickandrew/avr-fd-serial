/*
**  Demonstration of fd-serial module
**  (C) 2010, Nick Andrew <nick@tull.net>
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

#include "fd-serial.h"

/*  Set highest frequency CPU operation.
**  Startup frequency is assumed to be 1 MHz;
**  8 MHz for the internal clock.
*/

void set_cpu_8mhz(void) {
	// Prepare for clock change
	CLKPR = 1<<CLKPCE;
	// Set the internal clock
	CLKPR = 0<<CLKPS3 | 0<<CLKPS2 | 0<<CLKPS1 | 0<<CLKPS0;
	// System clock is now 8 MHz
}

void writeString(char const *cp) {
	while (*cp) {
		fdserial_send(*cp++);
	}
}

int main(void) {
	// Disable interrupts
	cli();

	// Setup the clock
	set_cpu_8mhz();

	// Enable the software UART
	fdserial_init();

	// Enable interrupts
	sei();

	uint32_t loops = 0;

	writeString("\r\nFull Duplex Serial example: receive and echo\r\n");

	while (1) {
		// Wait for a RX character and return it

		char c = fdserial_recv();

		// If a non-null character was received, initiate TX
		if (c) {
			// This does not wait for the character to be sent.
			// Instead, it waits for TX idle, which may mean
			// the end of the last TX character.
			fdserial_send(c);
		}

		loops ++;
	}
}
