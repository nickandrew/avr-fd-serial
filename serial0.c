/*
**  Tullnet Half Duplex UART using timer0
**  (C) 2010, Nick Andrew <nick@tull.net>
**
**  ATtiny85
**     This code uses Timer/Counter 0
**     RX connected to PB2, pin 7
**     TX connected to PB3, pin 2
**     Speed 9600 bps, half duplex
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

#include "serial0.h"

#ifndef CPU_FREQ
#define CPU_FREQ 8000000
#endif

#if SERIAL_RATE == 9600
// Prescaler CK/4
#define PRESCALER ( 1<<CS01 )
#define PRESCALER_DIVISOR 8
// 8000000 / PRESCALER / 9600 = 104.1666
#define SERIAL_TOP 103
#define SERIAL_HALFBIT 52
#else
#error "Serial rates other than 9600 are not presently supported"
#endif

/* Data structure used by this module */

static struct serial0_uart uart;

/*
**  Start the timer. The timer must be running while characters
**  are being received or sent.
*/

static void _starttimer(void)
{
	// Clear any pending timer interrupt
	TIFR |= 1<<OCF0B;
	// Start the timer counting
	TCCR0B |= PRESCALER;
}

/*
**  Stop the timer. This will not only save power but also stop
**  the regular timer interrupts on TIMER1_COMPA and TIMER1_COMPB.
*/

static void _stoptimer(void) {

	TCCR0B &= ~PRESCALER;
}

/*
**  Initialise the software UART.
**
**  Configure timer0 as follows:
**    1 interrupts per data bit
**    CTC mode (CTC1=1)
**    No output pin
**    Frequency = 8000000 / 8 / 104 = 9615 bits/sec
**    Prescaler = 8, Clock source = System clock, OCR0A = 103
*/

void serial0_init(void) {
	uint8_t com_mode = 0<<COM0A1 | 0<<COM0A0;
	uint8_t wgm1_mode = 1<<WGM01 | 0<<WGM00;
	uint8_t wgm2_mode = 0<<WGM02;

	uart.send_ready = 1;
	uart.state = 0;
	uart.available = 0;

	TCNT0 = 0;
	OCR0A = SERIAL_TOP;
	OCR0B = 0; // this will be used for send bit timing

	// Interrupt per rx or tx bit
	TIMSK |= 1<<OCIE0B;

	// Set output pin and raise it
	DDRB |= S0_TX_PIN;
	PORTB |= S0_TX_PIN;

	// Set input pin and enable pullup
	DDRB &= ~( S0_RX_PIN );
	PORTB |= S0_RX_PIN;

	_stoptimer();
	TCCR0A = com_mode | wgm1_mode;
	TCCR0B = wgm2_mode;
	_starttimer();
}

/*
**  serial0_available()
**   Return true if a character has been received.
*/

uint8_t serial0_available(void) {
	return uart.available;
}

/*
**  serial0_sendok()
**    Return true if the transmit interface is free to transmit a character
*/

uint8_t serial0_sendok(void) {
	return uart.send_ready;
}

/*
**  serial0_startbit()
**   If a start bit has been detected, start the RX state machine
**   and return true.
*/

uint8_t serial0_startbit(void) {
	if (PINB & S0_RX_PIN) {
		return 0;
	}

	// This will ensure an interrupt half a bit time hence
	uint8_t tcnt0 = TCNT0;
	if (tcnt0 >= SERIAL_HALFBIT) {
		OCR0B = tcnt0 - SERIAL_HALFBIT;
	} else {
		OCR0B = tcnt0 + SERIAL_HALFBIT;
	}

	uart.state = 6;
	_starttimer();

	return 1;
}

/*
**  serial0_send(c)
**    Send the character c
*/

void serial0_send(unsigned char send_arg) {
	// Wait until previous byte finished
	while (! uart.send_ready) { }

	OCR0B = TCNT0;
	uart.send_ready = 0;
	uart.send_byte = send_arg;
	uart.state = 1; // Send start bit
}

/*
**  c = serial0_recv()
**   Return the received character.
*/

unsigned char serial0_recv(void) {
	unsigned char c;

	if (! uart.available) {
		if (uart.state == 0) {
			// Wait for a start bit
			while (! serial0_startbit()) { }
		}

		// Wait for rx byte completion
		while (! uart.available) { }
	}

	c = uart.recv_byte;
	uart.recv_byte = 0;
	uart.available = 0;

	return c;
}

/*
**  serial0_alarm(uint32_t duration)
**
**  Keep the transmit system busy for the specified number of ms.
**
**  Wait until the transmit state is idle, then setup the
**  transmit bit comparator to count down.
*/

void serial0_alarm(uint32_t duration) {
	// Wait until available
	while (! uart.send_ready) { }

	uart.delay = duration;
	uart.send_ready = 0;
	uart.state = 5;
}

/*
**  serial0_delay(uint32_t duration)
**
**  Delay for the specified number of ms.
**
**  Setup an alarm for the specified duration, then
**  spin until the alarm has expired (transmit state
**  has returned to idle).
*/

void serial0_delay(uint32_t duration) {
	serial0_alarm(duration);

	// Wait until alarm expires
	while (! uart.send_ready) { }
}


// Interrupt routine for timer1, TCCR1A, tx bits

ISR(TIMER0_COMPB_vect)
{
	// Read the bit as early as possible, to try to hit the
	// center mark
	uint8_t read_bit = PINB & S0_RX_PIN;

	switch(uart.state) {
		case 0: // Idle
			break;

		case 1: // Send start bit
			PORTB &= ~( S0_TX_PIN );
			uart.state = 2;
			uart.bits = 8;
			break;

		case 2: // Send a bit
			if (uart.send_byte & 1) {
				PORTB |= S0_TX_PIN;
			} else {
				PORTB &= ~( S0_TX_PIN );
			}
			uart.send_byte >>= 1;

			if (! --uart.bits) {
				uart.state = 3;
			}
			break;

		case 3: // Send stop bit
			PORTB |= S0_TX_PIN;
			uart.state = 4;
			break;

		case 4: // Return to idle mode
			uart.send_ready = 1;
			uart.state = 0;
			break;

		case 5: // Timed delay
			if (! --uart.delay) {
				uart.send_ready = 1;
				uart.state = 0;
			}
			break;

		case 6: // Midpoint of start bit. Go on to first data bit.
			uart.state = 7;
			uart.bits = 8;
			break;

		case 7: // Reading a data bit
			uart.recv_shift >>= 1;
			if (read_bit) {
				uart.recv_shift |= 0x80;
			}

			if (! --uart.bits) {
				uart.state = 8;
			}

			break;

		case 8: // Reading the stop bit
			if (read_bit) {
				uart.recv_byte = uart.recv_shift;
				uart.available = 1;
				uart.state = 0;
				_stoptimer();
			} else {
				// Framing error
				// Would like to wait for next byte at this point (later)
				uart.recv_byte = 0;
				uart.available = 2;
				uart.state = 0;
				_stoptimer();
			}

			break;

	}
}
