/*
**  Tullnet Full Duplex Serial UART
**  (C) 2010, Nick Andrew <nick@tull.net>
*/

#ifndef _FD_SERIAL_H
#define _FD_SERIAL_H

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
	volatile unsigned char tx_state;
	volatile unsigned char rx_state;
	volatile unsigned char send_byte;
	volatile unsigned char recv_byte;  // buffered received byte
	volatile unsigned char recv_shift; // rx data shifted into this byte
	volatile unsigned char send_bits;
	volatile unsigned char recv_bits;
	volatile unsigned char available;  // 1 = rx data available
	volatile unsigned char send_ready;  // 1 = can send a byte
#if SERIAL_CYCLES != 1
	volatile unsigned char tx_cycle;
	volatile unsigned char rx_cycle;
#endif
};

void fdserial_init(void);

unsigned int fdserial_available(void);

unsigned int fdserial_sendok(void);

void fdserial_send(unsigned char send_arg);

unsigned char fdserial_recv(void);

#endif
