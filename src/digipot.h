/*
 * digipot.h
 *
 *  Created on: Jul 24, 2012
 *      Author: vic
 */

#ifndef DIGIPOT_H_
#define DIGIPOT_H_

#include <avr/io.h>
#define DIGIPOT_SLAVE_SELECT_DDR  DDRB
#define DIGIPOT_SLAVE_SELECT_PORT PORTB
#define DIGIPOT_SLAVE_SELECT_PIN  1

#define DIGIPOT_CMD_INCREMENT 0x04 //B00000100
#define DIGIPOT_CMD_DECREMENT 0x08 //B00001000
#define DIGIPOT_ADR_WIPER0    0x00 //B00000000
#define DIGIPOT_ADR_WIPER1    0x10 //B00010000
#define DIGIPOT_ADR_VOLATILE  0x00 //B00000000
#define DIGIPOT_WIPER0        DIGIPOT_ADR_WIPER0
#define DIGIPOT_WIPER1        DIGIPOT_ADR_WIPER1

void digipot_select_slave();
void digipot_deselect_slave();
void digipot_write(uint8_t data_byte);

void digipot_init();
void digipot_increment(uint8_t wiper_num, uint8_t n);
void digipot_decrement(uint8_t wiper_num, uint8_t n);

#endif /* DIGIPOT_H_ */