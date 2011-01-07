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

int main(void)
{
	// Disable interrupts
	cli();

	//Setup the clock
	set_cpu_8mhz();

	fdserial_init();

	// Enable interrupts
	sei();

	uint32_t loops = 0;

	// Configure PORTB4 as an output
	DDRB |= 1<<PORTB4;

	while (1) {
		loops ++;

		if (loops % 50000 == 0) {
			PORTB |= 1<<PORTB4;
			writeString("ATtiny85 Serial Port Test 9600BPS UUUUU UUUUU\n");
			PORTB &= ~(1<<PORTB4);
		}
	}
}
