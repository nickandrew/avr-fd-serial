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

volatile uint8_t cycle_64 = 0;
volatile uint8_t cycle_74 = 0;
volatile uint8_t cycle_94 = 0;
volatile uint8_t cycle_10 = 0;
volatile uint8_t tcnt_64 = 0;
volatile uint8_t tcnt_74 = 0;
volatile uint8_t tcnt_94 = 0;
volatile uint8_t tcnt_10 = 0;
uint8_t done = 0;

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
	uint8_t pwm_mode = 0<<PWM1A;

	TCNT1 = 0;
	OCR1A = 32;
	OCR1B = 64;
	OCR1C = 207;
	TIMSK |= 1<<OCIE1A | 1<<OCIE1B;
	DDRB |= 1<<PORTB3;
	PORTB |= 1<<PORTB3;
	// TCCR1 |= 1<<PWM1A;
	// GTCCR |= 1<<PWM1B;
	TCCR1 = ctc_mode | com_mode | prescaler;
	GIMSK |= 1<<INT0;

	// Enable interrupts
	sei();

	uint32_t loops = 0;
	uint8_t old_tcnt1 = 0;

	writeString("Starting\n");
	writeString("Really starting\n");

//	while (1) {
//		serial0_send('<');
//		serial0_send('<');
//		serial0_send('<');
//		serial0_delay(1000);
//		serial0_send('>');
//	}

	while (1) {
		loops ++;
		tcnt1 = TCNT1;

		if (tcnt1 < old_tcnt1) {
			cycle_count ++;
		}

		old_tcnt1 = tcnt1;

		if (cycle_10 && !done) {
			writeString("Done: <");
			write8(cycle_64);
			write8(tcnt_64);
			write8(cycle_74);
			write8(tcnt_74);
			write8(cycle_94);
			write8(tcnt_94);
			write8(cycle_10);
			write8(tcnt_10);
			serial0_send('>');
			serial0_send('\n');
			done = 1;
			writeString("Finito\n");
		}
	}
}

// Interrupt routine for timer1, TCCR1A, tx bits

ISR(TIMER1_COMPA_vect)
{
	// Do nothing
}

// Interrupt routine for timer1, TCCR1A, rx bits

ISR(TIMER1_COMPB_vect)
{
	uint8_t tcnt = TCNT1;
	uint8_t cycles = cycle_count;
	uint8_t ocr1b = OCR1B;

	if (ocr1b == 64) {
		cycle_64 = cycles;
		tcnt_64 = tcnt;
		OCR1B = 74;
	}
	else if (ocr1b == 74) {
		cycle_74 = cycles;
		tcnt_74 = tcnt;
		OCR1B = 94;
	}
	else if (ocr1b == 94) {
		cycle_94 = cycles;
		tcnt_94 = tcnt;
		OCR1B = 10;
	}
	else if (ocr1b == 10) {
		cycle_10 = cycles;
		tcnt_10 = tcnt;
		OCR1B = 22;
	}
}

// This is called on the falling edge of INT0 (pin 7)

ISR(INT0_vect) {
	// This will cause a timer interrupt half a bit time later
	uint8_t tcnt1 = TCNT1;
	OCR1B = (tcnt1 + 90) % 208;
	count_int0 ++;
}
