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

#if SERIAL_CYCLES != 1
#error "SERIAL_CYCLES only works with 1 at present"
#endif

#ifndef CPU_FREQ
#define CPU_FREQ 8000000
#endif

#if SERIAL_RATE == 9600
// Prescaler CK/4
#define PRESCALER (1<<CS11 | 1<<CS10)
#define PRESCALER_DIVISOR 4
// 8000000 / PRESCALER / 9600 = 208.333
#define SERIAL_TOP 207
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
	OCR1A = 0; // this will be used for send bit timing
	OCR1B = 0; // this will be used for receive bit timing
	OCR1C = SERIAL_TOP;

	// Interrupt per rx or tx bit
	TIMSK |= 1<<OCIE1A | 1<<OCIE1B;

	// Output pin PB3, and raise it
	DDRB |= 1<<PORTB3;
	PORTB |= 1<<PORTB3;

	_stoptimer();
	TCCR1 = ctc_mode | com_mode;
	_starttimer();
	_enable_int0();
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
	if (! --fd_uart1.tx_cycle) {
		// Ignore this inter-bit interrupt
		return;
	}

	// Reset it to the multiple of the bit frequency
	fd_uart1.tx_cycle = SERIAL_CYCLES;
#endif

	switch(fd_uart1.tx_state) {
		case 0: // Idle
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
	// Read the bit as early as possible, to try to hit the
	// center mark
	uint8_t read_bit = PINB & (1<<PORTB2);

	switch(fd_uart1.rx_state) {
		case 0: // Idle
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

		case 3: // Byte done
			fd_uart1.recv_byte = fd_uart1.recv_shift;
			fd_uart1.available = 1;
			fd_uart1.rx_state = 0;
			_enable_int0();
			break;
	}
}

// This is called on the falling edge of INT0 (pin 7)

ISR(INT0_vect) {
	// This will cause a timer interrupt half a bit time later
	uint8_t tcnt1 = TCNT1;
	OCR1B = (tcnt1 + 90) % 208;

	_disable_int0();
	fd_uart1.rx_state = 1;
}
