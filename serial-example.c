/*
**  Demonstration of fd-serial module
**  (C) 2010, Nick Andrew <nick@tull.net>
*/

#include <avr/io.h>
#include <avr/interrupt.h>

#include "fd-serial.h"

/* Set highest frequency CPU operation
**  Startup frequency is assumed to be 1 MHz
**  8 MHz for the internal clock
*/

void set_cpu_8mhz(void) {
	// Let's wind this sucker up to 8 MHz
	CLKPR = 1<<CLKPCE;
	CLKPR = 0<<CLKPS3 | 0<<CLKPS2 | 0<<CLKPS1 || 0<<CLKPS0;
	// System clock is now 8 MHz
}

void writeString(char const *cp) {
	while (*cp) {
		fdserial_send(*cp++);
	}
}

int main(void)
{
	// Disable interrupts
	cli();

	//Setup the clock
	set_cpu_8mhz();

	fdserial_init();

	// Enable interrupts
	sei();

	while (1) {
		char line[128];
		char *cp = line;
		unsigned char c;
		unsigned int i;

		for (i = 0; i < 126; ++i) {
			c = fdserial_recv();
			if (c == '\n') {
				break;
			}
			*cp++ = c;
			i ++;
		}

		*cp++ = '\n';
		*cp++ = '\0';

		writeString("You sent: ");
		writeString(line);
	}
}
