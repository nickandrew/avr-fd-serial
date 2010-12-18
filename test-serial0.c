/*
**  Demonstration of fd-serial module
**  (C) 2010, Nick Andrew <nick@tull.net>
*/

#include <avr/io.h>
#include <avr/interrupt.h>

#include "serial0.h"

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
		serial0_send(*cp++);
	}
}

void write8(unsigned char c) {
	unsigned char b = (c >> 4) + 0x30;
	serial0_send(b > 0x39 ? b + 7 : b);
	b = (c & 0x0f) + 0x30;
	serial0_send(b > 0x39 ? b + 7 : b);
}

void write16(uint16_t i) {
	write8(i >> 8);
	write8(i & 0xff);
}

void write32(uint32_t i) {
	write16(i >> 16);
	write16(i & 0xffff);
}

volatile uint8_t cycle_count = 0;
volatile uint8_t count_int0 = 0;
volatile uint8_t tcnt1 = 0;
#define NR_MARKERS 10
volatile uint8_t a_state[NR_MARKERS];
volatile uint8_t a_cycle[NR_MARKERS];
volatile uint8_t a_ocr1b[NR_MARKERS];
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

	serial0_init();

	// Setup timer1
	uint8_t ctc_mode = 1<<CTC1;
	uint8_t com_mode = 0<<COM1A1 | 0<<COM1A0;
	uint8_t prescaler = 1<<CS13 | 1<<CS12 | 1<<CS11 | 1<<CS10;

	TCNT1 = 0;
	OCR1A = 64;
	OCR1B = 128;
	OCR1C = 207;
	TIMSK |= 1<<OCIE1A | 1<<OCIE1B;
	DDRB |= 1<<PORTB3;
	PORTB |= 1<<PORTB3;
	TCCR1 = ctc_mode | com_mode | prescaler;
	GIMSK |= 1<<INT0;

	// Enable interrupts
	sei();

	uint32_t loops = 0;
	uint8_t old_tcnt1 = 0;

	writeString("Starting\n");

	while (1) {
		loops ++;
		tcnt1 = TCNT1;

		if (tcnt1 < old_tcnt1) {
			cycle_count ++;
			if (a_index < a_max) {
				a_state[a_index] = 4;
				a_cycle[a_index] = cycle_count;
				a_ocr1b[a_index] = 0;
				a_counter[a_index] = tcnt1;
				a_index ++;
			}
		}

		old_tcnt1 = tcnt1;

		if (a_index > 0) {
			writeString("T1:");
			uint8_t i;
			for (i = 0; i < a_index; ++i) {
				write8(i);
				serial0_send(':');
				write8(a_state[i]);
				serial0_send(':');
				write8(a_cycle[i]);
				serial0_send(':');
				write8(a_ocr1b[i]);
				serial0_send(':');
				write8(a_counter[i]);
				serial0_send('>');
			}
			a_index = 0;
			serial0_send('\n');

			// TCCR1 &= ~prescaler;
			// while (1) { }
		}
	}
}

// Interrupt routine for timer1, TCCR1A, tx bits

ISR(TIMER1_COMPA_vect)
{
	uint8_t tcnt = TCNT1;
	uint8_t cycles = cycle_count;

	if (a_index < a_max) {
		a_state[a_index] = 1;
		a_cycle[a_index] = cycles;
		a_ocr1b[a_index] = OCR1A;
		a_counter[a_index] = tcnt;
		a_index ++;
	}
}

// Interrupt routine for timer1, TCCR1A, rx bits

ISR(TIMER1_COMPB_vect)
{
	uint8_t tcnt = TCNT1;
	uint8_t cycles = cycle_count;

	if (a_index < a_max) {
		a_state[a_index] = 2;
		a_cycle[a_index] = cycles;
		a_ocr1b[a_index] = OCR1B;
		a_counter[a_index] = tcnt;
		a_index ++;
	}
}

// This is called on the falling edge of INT0 (pin 7)

ISR(INT0_vect) {
	// This will cause a timer interrupt half a bit time later
	uint8_t tcnt1 = TCNT1;
	OCR1B = (tcnt1 + 90) % 208;

	if (a_index < a_max) {
		a_state[a_index] = 3;
		a_cycle[a_index] = cycle_count;
		a_ocr1b[a_index] = OCR1B;
		a_counter[a_index] = tcnt1;
		a_index ++;
	}
}
