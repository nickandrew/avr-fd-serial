/*
**  Tullnet Half Duplex UART using timer0
**  (C) 2010, Nick Andrew <nick@tull.net>
*/

#ifndef _SERIAL0_H
#define _SERIAL0_H

#include <stdint.h>

#ifndef SERIAL_RATE
// Support for different bitrates is not presently implemented
#define SERIAL_RATE 9600
#endif

struct serial0_uart {
	volatile uint8_t state;
	volatile unsigned char send_byte;
	volatile uint8_t send_bits;
	volatile uint8_t send_ready;   // 1 = can send a byte
	volatile uint32_t delay;       // No of bit times to delay
};

// Initialise data structures, timer, interrupts and output pin

void serial0_init(void);

uint8_t serial0_sendok(void);

void serial0_send(unsigned char send_arg);

// Set an alarm for a specified number of ms hence

void serial0_alarm(uint32_t duration);

// Wait for a specified number of ms

void serial0_delay(uint32_t duration);

#endif
