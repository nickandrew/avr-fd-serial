/*
**  Tullnet Full Duplex Serial UART
**  (C) 2010, Nick Andrew <nick@tull.net>
*/

#ifndef _FD_SERIAL_H
#define _FD_SERIAL_H

#include <stdint.h>

#ifndef SERIAL_CYCLES
// This says how many interrupts will occur per bit duration.
// Set to 1 ==> When start bit is detected, set OCR1B halfway around the cycle
// Set to 4 ==> Do 3 times oversampling of each bit
#define SERIAL_CYCLES 1
#endif

#ifndef SERIAL_RATE
// Support for different bitrates is not presently implemented
#define SERIAL_RATE 9600
#endif

struct fd_uart {
	volatile uint8_t tx_state;
	volatile uint8_t rx_state;
	volatile unsigned char send_byte;
	volatile unsigned char recv_byte;  // buffered received byte
	volatile unsigned char recv_shift; // rx data shifted into this byte
	volatile uint8_t send_bits;
	volatile uint8_t recv_bits;
	volatile uint8_t available;  // 1 = rx data available
	volatile uint8_t send_ready;  // 1 = can send a byte
#if SERIAL_CYCLES != 1
	volatile uint8_t tx_cycle;
	volatile uint8_t rx_cycle;
#endif
};

void fdserial_init(void);

uint8_t fdserial_available(void);

uint8_t fdserial_sendok(void);

void fdserial_send(unsigned char send_arg);

unsigned char fdserial_recv(void);

#endif
