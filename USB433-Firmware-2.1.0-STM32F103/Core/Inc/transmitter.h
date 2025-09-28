/*
 * transmitter.h
 *
 *  Created on: Jun 26, 2025
 *      Author: R
 */

#ifndef INC_TRANSMITTER_H_
#define INC_TRANSMITTER_H_


#include "stdint.h"

#define TX_BUFFER_LEN 5 // length of words that can be buffered
#define TX_MAX_BITS 64 // max length of a word to transmit

#define TX_BUFFER_EMPTY 0x01 // no data to transmit
#define TX_PREP_FAILED 0x02 // invalid characters caused transmit buffer to fail
#define TX_COMPLETE 0x04 // transmission complete flag

typedef struct {
	bool frame_complete = true; // indicates DMA transmission complete
	bool burst_complete = true; // indicates burst of frames is complete
	uint8_t frames_sent = 0; // how many total frames have been sent
	uint32_t last_frame_time_ms = 0; // when the last frame completed
	uint16_t dma_buffer[TX_MAX_BITS + 1]; // buffer to use for DMA transmission; holds duty cycle (timer CCR) values
	uint16_t dma_len = 0; // counter for how many bits to send for packet
} TxPacket;

typedef struct {
	// data structure params
	bool invert_logic = false; // true: long high == 1; false: long high == 0
	bool ignore_sync_bit = false; // word is n bits long; if true, prepend a long-high bit to the start to sync data to known state
	// timing params
	uint16_t t_short = 300; // time of short pulse, in microseconds
	uint16_t t_long = 700; // time of long pulse, in microseconds
	uint32_t frame_delay_us = 6600; // time between sending the same frame of data, in microseconds
	uint32_t burst_delay_us = 100000; // time between sending packets of different data
	// data transmission params
	uint8_t frame_repeat = 7; // by default, send once and repeat n times
	char buffer[TX_BUFFER_LEN][TX_MAX_BITS];

} Transmitter;

extern TIM_HandleTypeDef htim1;
extern Transmitter tx;
extern TxPacket data;

void txInit(Transmitter* settings);
void updateARR(Transmitter* settings);
void makeTxPacket(Transmitter* settings, TxPacket* packet);
void processTx(Transmitter* settings, TxPacket* packet);


#endif /* INC_TRANSMITTER_H_ */
