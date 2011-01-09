/*
**  Tullnet Full Duplex Serial UART
**  (C) 2010, Nick Andrew <nick@tull.net>
*/

#ifndef _FD_SERIAL_H
#define _FD_SERIAL_H

#include <stdint.h>

// Size of rx buffer. Up to one less characters can be
// received in the background and not yet read by the caller.
#define RING_BUFFER 20

#ifndef SERIAL_RATE
// Support for different bitrates is not presently implemented
#define SERIAL_RATE 9600
#endif

struct fd_uart {
	volatile uint8_t tx_state;
	volatile uint8_t rx_state;
	volatile unsigned char send_byte;  // byte presently being sent (shifted)
	volatile unsigned char recv_byte;  // buffered received byte
	volatile unsigned char recv_shift; // rx data shifted into this byte
	volatile uint8_t send_bits;        // Number of bits remaining to send
	volatile uint8_t recv_bits;        // Number of bits remaining to receive
	volatile uint8_t available;        // 1 = rx data available
	volatile uint8_t send_ready;       // 1 = can send a byte
	volatile uint16_t delay;           // Number of bit times to delay
#ifdef RING_BUFFER
	volatile unsigned char rx_buf[RING_BUFFER];
	volatile uint8_t rx_head;          // Index of next char to append
	volatile uint8_t rx_tail;          // Index of next char to remove
#endif
};

// Initialise data structures, timer, interrupts and output pin

void fdserial_init(void);

// Return count of bytes available to read

uint8_t fdserial_available(void);

// Return true when the tx sender is idle

uint8_t fdserial_sendok(void);

void fdserial_send(unsigned char send_arg);

unsigned char fdserial_recv(void);

// Set an alarm for a specified number of ms hence

void fdserial_alarm(uint32_t duration);

// Wait for a specified number of ms

void fdserial_delay(uint32_t duration);

#endif
