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
	CLKPR = 0<<CLKPS3 | 0<<CLKPS2 | 0<<CLKPS1 | 0<<CLKPS0;
	// System clock is now 8 MHz
}

void writeString(char const *cp) {
	while (*cp) {
		fdserial_send(*cp++);
	}
}

void write8(unsigned char c) {
	unsigned char b = (c >> 4) + 0x30;
	fdserial_send(b > 0x39 ? b + 7 : b);
	b = (c & 0x0f) + 0x30;
	fdserial_send(b > 0x39 ? b + 7 : b);
}

void write16(uint16_t i) {
	write8(i >> 8);
	write8(i & 0xff);
}

void write32(uint32_t i) {
	write16(i >> 16);
	write16(i & 0xffff);
}

volatile uint8_t count_int0 = 0;
volatile uint8_t start_cycles = 0;
volatile uint8_t start_counter = 0;
volatile uint8_t stop_cycles = 0;
volatile uint8_t stop_counter = 0;
volatile uint8_t cycle_count = 0;
#define NR_MARKERS 14
volatile uint8_t a_state[NR_MARKERS];
volatile uint8_t a_cycle[NR_MARKERS];
volatile uint8_t a_counter[NR_MARKERS];
volatile uint8_t a_index = 0;
volatile uint8_t a_max = NR_MARKERS;

char *newline = "\n";

int main(void)
{
	// Disable interrupts
	cli();

	//Setup the clock
	set_cpu_8mhz();

	fdserial_init();

	// Enable interrupts
	sei();

#define MAXSIZE 20
	unsigned char buf[MAXSIZE];
	unsigned char *cp = buf;
	uint32_t loops = 0;
	uint8_t tcnt1;

	while (1) {
		loops ++;
		tcnt1 = TCNT1;
		if (tcnt1 >= 208) {
			fdserial_send('O');
			write8(tcnt1);
		}

		if (loops % 100000 == 0) {
			if (a_index > 0) {
				uint8_t i;
				fdserial_send('W');
				for (i = 0; i < a_index; ++i) {
					write8(a_cycle[i]);
					fdserial_send(':');
					write8(a_counter[i]);
					fdserial_send(' ');
				}
				a_index = 0;
				fdserial_send('\n');
			}
			if (cp > buf) {
				unsigned char *cp2 = buf;
				fdserial_send('<');
				while (cp2 < cp) {
					write8(*cp2++);
					fdserial_send(' ');
				}
				fdserial_send('>');
				cp = buf;
			}

			fdserial_send('C');
			fdserial_send(' ');
			write8(OCR1A);
			fdserial_send(' ');
			write8(OCR1B);
			fdserial_send('\n');

		}

		if (fdserial_available()) {
			unsigned char c = fdserial_recv();
			if (cp < (buf + MAXSIZE)) {
				*cp++ = c;
			}
		}
	}

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

		// writeString("U sent: ");
		writeString(line);
	}
}
