/*
**  Tullnet Full Duplex Serial UART
**  (C) 2010, Nick Andrew <nick@tull.net>
**
**  ATtiny85
**     This code uses Timer/Counter 1
**     RX connected to PB2 (INT0), pin 7
**     TX connected to PB3, pin 2
**     Speed 9600 bps, full duplex
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

extern volatile uint8_t count_int0;
extern volatile uint8_t start_cycles;
extern volatile uint8_t start_counter;
extern volatile uint8_t stop_cycles;
extern volatile uint8_t stop_counter;
extern volatile uint8_t cycle_count;
extern volatile uint8_t a_state[];
extern volatile uint8_t a_cycle[];
extern volatile uint8_t a_counter[];
extern volatile uint8_t a_index;
extern volatile uint8_t a_max;

#include "fd-serial.h"

#if SERIAL_CYCLES != 1 && SERIAL_CYCLES != 4
#error "SERIAL_CYCLES only works with 1 and 4 at present"
#endif

#ifndef CPU_FREQ
#define CPU_FREQ 8000000
#endif

#if SERIAL_RATE == 9600
// Prescaler CK/4
#define PRESCALER (1<<CS11 | 1<<CS10)
#define PRESCALER_DIVISOR 4
// 8000000 / PRESCALER / 9600 = 208.333

#if SERIAL_CYCLES == 1
#define SERIAL_TOP 207
#define SERIAL_HALFBIT 104
#elif SERIAL_CYCLES == 4
#define SERIAL_TOP 51
#endif

#else
#error "Serial rates other than 9600 are not presently supported"
#endif

/* Data structure used by this module */

static struct fd_uart fd_uart1;

/*
**  Start the timer. The timer must be running while characters
**  are being received or sent.
*/

static void _starttimer(void) {

	TCCR1 |= PRESCALER;
}

/*
**  Stop the timer. This will not only save power but also stop
**  the regular timer interrupts on TIMER1_COMPA and TIMER1_COMPB.
*/

static void _stoptimer(void) {

	TCCR1 &= ~PRESCALER;
}

/*
**  Enable PCINT0
*/

static void _enable_int0(void) {
	GIMSK |= 1<<INT0;
}

/*
**  Disable PCINT0
*/

static void _disable_int0(void) {
	GIMSK &= ~( 1<<INT0 );
}

inline void _start_tx(void) {
	TIMSK |= 1<<OCIE1A;
}

inline void _stop_tx(void) {
	TIMSK &= ~( 1<<OCIE1A);
}

inline void _start_rx(void) {
	TIMSK |= 1<<OCIE1B;
}

inline void _stop_rx(void) {
	TIMSK &= ~( 1<<OCIE1B);
}

/*
**  Initialise the software UART.
**
**  Configure timer1 as follows:
**    1 interrupts per data bit
**    CTC mode (CTC1=1)
**    No output pin
**    Frequency = 8000000 / 4 / 208 = 9615 bits/sec
**    Prescaler = 4, Clock source = System clock, OCR1C = 207
**  Configure PCINT0 so an interrupt occurs on the falling edge
**    of PCINT0 (pin 7)
*/

void fdserial_init(void) {
	uint8_t com_mode = 0<<COM1A1 | 0<<COM1A0;
	uint8_t ctc_mode = 1<<CTC1;

	fd_uart1.send_ready = 1;
	fd_uart1.tx_state = 0;

	fd_uart1.available = 0;
	fd_uart1.rx_state = 0;

	// Configure PCINT0 to interrupt on falling edge
	MCUCR |= 1<<ISC01;

	TCNT1 = 0;
	OCR1A = 16; // this will be used for send bit timing
	OCR1B = 32; // this will be used for receive bit timing
	OCR1C = SERIAL_TOP;

	// Interrupt per rx or tx bit
	// TIMSK |= 1<<OCIE1B;

	// Debugging output pins
	DDRB |= 1<<PORTB4 | 1<<PORTB1 | 1<<PORTB0;

	// Output pin PB3, and raise it
	DDRB |= 1<<PORTB3;
	PORTB |= 1<<PORTB3;

	_stoptimer();
	TCCR1 = ctc_mode | com_mode;
	_starttimer();
	_enable_int0();
	_start_tx();
}

/*
**  fdserial_available()
**    Return true if a character has been received on the receive interface.
*/

uint8_t fdserial_available(void) {
	return fd_uart1.available;
}

/*
**  fdserial_sendok()
**    Return true if the transmit interface is free to transmit a character
*/

uint8_t fdserial_sendok(void) {
	return fd_uart1.send_ready;
}

/*
**  fdserial_send(c)
**    Send the character c
*/

void fdserial_send(unsigned char send_arg) {
	// Wait until previous byte finished
	while (! fd_uart1.send_ready) { }

	OCR1A = TCNT1;
#if SERIAL_CYCLES != 1
	fd_uart1.tx_cycle = SERIAL_CYCLES;
#endif
	fd_uart1.send_ready = 0;
	fd_uart1.send_byte = send_arg;
	fd_uart1.tx_state = 1; // Send start bit
	_start_tx();
}

/*
**  c = fdserial_recv()
**    Return the received character.
**    The receive interface is buffered, so when a character is available
**    the MCU has at most 1 character-time to receive it, otherwise it
**    will be overwritten by the next character.
**
**    This function will wait until a character is received.
*/

unsigned char fdserial_recv() {
	// Wait until available
	while (! fd_uart1.available) { }

	unsigned char c = fd_uart1.recv_byte;
	fd_uart1.recv_byte = 0;  // Reading nulls means you are probably doing something wrong
	fd_uart1.available = 0;

	return c;
}


/*
**  fdserial_alarm(uint32_t duration)
**
**  Keep the transmit system busy for the specified number of ms.
**
**  Wait until the transmit state is idle, then setup the
**  transmit bit comparator to count down.
*/

void fdserial_alarm(uint32_t duration) {
	uint32_t timer_ticks = ( duration * CPU_FREQ ) / PRESCALER_DIVISOR / 1000;
	uint32_t cycles = timer_ticks / ( SERIAL_TOP + 1);
	uint8_t remainder = timer_ticks - (cycles * (SERIAL_TOP + 1));
	// Wait until available
	while (! fd_uart1.send_ready) { }

	OCR1A = TCNT1 - remainder;
	fd_uart1.delay = cycles;
	fd_uart1.send_ready = 0;
	fd_uart1.tx_state = 5;
}

/*
**  fdserial_delay(uint32_t duration)
**
**  Delay for the specified number of ms.
**
**  Setup an alarm for the specified duration, then
**  spin until the alarm has expired (transmit state
**  has returned to idle).
*/

void fdserial_delay(uint32_t duration) {
	fdserial_alarm(duration);

	// Wait until alarm expires
	while (! fd_uart1.send_ready) { }
}


// Interrupt routine for timer1, TCCR1A, tx bits

ISR(TIMER1_COMPA_vect)
{

#if SERIAL_CYCLES != 1
	if (--fd_uart1.tx_cycle) {
		return;
	}

	fd_uart1.tx_cycle = SERIAL_CYCLES;
#endif

	// Toggle bits in PORTB to update them to the current rx_state
	uint8_t x = (PORTB & 0x03) ^ fd_uart1.rx_state;
	PORTB ^= x;

	switch(fd_uart1.tx_state) {
		case 0: // Idle
			// Show the rx_state (0..3) on PORTB0 and PORTB1
			return;

		case 1: // Send start bit
			PORTB &= ~( 1<<PORTB3 );
			fd_uart1.tx_state = 2;
			fd_uart1.send_bits = 8;
			return;

		case 2: // Send a bit
			if (fd_uart1.send_byte & 1) {
				PORTB |= 1<<PORTB3;
			} else {
				PORTB &= ~( 1<<PORTB3 );
			}
			fd_uart1.send_byte >>= 1;

			if (! --fd_uart1.send_bits) {
				fd_uart1.tx_state = 3;
			}
			return;

		case 3: // Send stop bit
			PORTB |= 1<<PORTB3;
			fd_uart1.tx_state = 4;
			return;

		case 4: // Return to idle mode
			fd_uart1.send_ready = 1;
			fd_uart1.tx_state = 0;
			// _stop_tx();
			return;
		case 5: // Timed delay
			if (! --fd_uart1.delay) {
				fd_uart1.send_ready = 1;
				fd_uart1.tx_state = 0;
			}
			return;
	}
}

// Interrupt routine for timer1, TCCR1A, rx bits

ISR(TIMER1_COMPB_vect)
{
	PORTB |= 1<<PORTB4;

	// Read the bit as early as possible, to try to hit the
	// center mark
	uint8_t read_bit = PINB & (1<<PORTB2);

#if SERIAL_CYCLES != 1
	if (--fd_uart1.rx_cycle) {
		return;
	}

	fd_uart1.rx_cycle = SERIAL_CYCLES;
#endif

	switch(fd_uart1.rx_state) {
		case 0: // Idle
			// Midpoint of start bit. Go on to first data bit.
			fd_uart1.rx_state = 2;
			fd_uart1.recv_bits = 8;
#if SERIAL_CYCLES != 1
			fd_uart1.rx_cycle = 2;
#endif
			break;

		case 1: // Reading start bit
			// Go straight on to first data bit
			fd_uart1.rx_state = 2;
			fd_uart1.recv_bits = 8;
			break;

		case 2: // Reading a data bit
			fd_uart1.recv_shift >>= 1;
			if (read_bit) {
				fd_uart1.recv_shift |= 0x80;
			}

			if (! --fd_uart1.recv_bits) {
				fd_uart1.rx_state = 3;
			}
			break;

		case 3: // Byte done, wait for high
			if (read_bit) {
				fd_uart1.recv_byte = fd_uart1.recv_shift;
				fd_uart1.available = 1;
				fd_uart1.rx_state = 0;
				_stop_rx();
				_enable_int0();
				// Clear any pending INT0
				GIFR |= 1<<INTF0;
			}
			break;
	}
	PORTB &= ~(1<<PORTB4);
}

// This is called on the falling edge of INT0 (pin 7).
// It will also be called at the end of a received byte (when interrupts
// are re-enabled) if the byte contained any zero-bits.

ISR(INT0_vect) {
	uint8_t tcnt1 = TCNT1;

#if SERIAL_CYCLES != 1
	// This will cause a timer interrupt half a bit time later
	fd_uart1.tx_cycle = 2;
#endif

	uint8_t read_bit = PINB & (1<<PORTB2);

	// Only start RX if we are reading a start bit.
	// This prevents an "extra" INT0 interrupt at the end of a byte
	// from restarting the receive state machine.
	if (! read_bit) {
		// Set sample time, half a bit after now.
		if (tcnt1 > 103) {
			OCR1B = tcnt1 - 103;
		} else {
			OCR1B = tcnt1 + SERIAL_HALFBIT;
		}
		// disable int0
		GIMSK &= ~( 1<<INT0 );
		// Clear pending RX timer interrupt
		TIFR |= 1<<OCF1B;
		// start rx
		TIMSK |= 1<<OCIE1B;
	}

}
