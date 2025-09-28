/*
 * receiver.h
 *
 *  Created on: Jun 26, 2025
 *      Author: R
 */

#ifndef INC_RECEIVER_H_
#define INC_RECEIVER_H_

#include "stm32f1xx_hal.h"

#include "stdint.h"

#define RX_RADIO_EN_POLARITY true // true = active high; false = active low

#define RX_MAX_BITS 64
#define RX_BUFFER_SAMPLES 5*RX_MAX_BITS // TODO: update to 5 for release
#define RX_CORREL_WORDS 12

// status flags
#define RX_WORD_AVAILABLE 0x01

typedef struct {
	uint8_t len = 0;
	char word[RX_MAX_BITS+1];
	uint32_t long_us = 0;
	uint32_t short_us = 0;
	uint32_t period_us = 0;
	bool logic = false;
} RxPacket;

typedef struct {
	uint8_t index = 0; // current word being written
	uint32_t last_word_time_ms = 0; // when last word was injected
	RxPacket received[RX_CORREL_WORDS]; // buffer for all received words
	RxPacket* last_match = 0;
	uint32_t timeout_us = 100000; // microseconds after which the correl buffer gets cleared
	uint8_t match_thresh = 3; // min number of repeated messages to be considered valid.
	uint8_t min_word_len = 8; // min chars for a code to be valid
	uint8_t max_word_len = RX_MAX_BITS;
} RxCorrelBuffer;

typedef struct {
	// basic control variables for receiver
	bool invert_logic = false;
	bool ignore_sync_bit = true;
	uint8_t mode = 2;
	// buffer variables
	uint16_t stor_idx = 0; // index of current sample to store
	uint16_t proc_idx = 0; // index of final sample to process
	uint16_t tgt_idx = 0; // target index for processing up to. Snapshots stor_idx-1 when word end detected
	uint32_t bit_max_period = 5000; // set max bit period, in microseconds
	uint32_t measured_periods[RX_BUFFER_SAMPLES]; // us, per timer prescaler
	uint32_t measured_widths[RX_BUFFER_SAMPLES]; // us, per timer prescaler
	// correlation buffer struct (for word repetition detect)
	RxCorrelBuffer correl;
} Receiver;

extern Receiver rx;
extern TIM_HandleTypeDef htim2;
extern uint32_t status;

void rxInit(Receiver* settings);

bool isRxEnabled(void);
void enableRx(void);
void disableRx(void);

//////////////////////////////////////////////////

void clearRxPacket(RxPacket* buf);

void rxWordRepeated(char *buffer, size_t size);
void receivedWord(RxCorrelBuffer* correl, RxPacket* data);
void checkRxBuffers(void);

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim);

#endif /* INC_RECEIVER_H_ */
