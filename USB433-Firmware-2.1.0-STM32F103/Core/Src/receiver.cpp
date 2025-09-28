/*
 * receiver.cpp
 *
 *  Created on: Jun 26, 2025
 *      Author: R
 */

#include "stm32f1xx_hal.h"

#include "string.h"
#include "stdio.h"
#include "stdint.h"

#include "main.h"
#include "receiver.h"
#include "more_math.h"

uint8_t duty_tol = 15; // cutoff between "normal" and "abnormal" duty cycles. Must be between 1 and 49 for correct operation
float period_lim = 1.3; // factor away from the mode period at which the period is replaced by the mode period
uint16_t overflow_count; // counts how much the input capture timer has overflowed the count
uint32_t measPeriodSorted[RX_BUFFER_SAMPLES];

// for word computations
uint32_t measLong[RX_MAX_BITS + 1];
uint32_t measShort[RX_MAX_BITS + 1];
uint32_t measPeriod[RX_MAX_BITS + 1];

Receiver rx;

/*
 * Initialize timers and parameters needed for OOK Rx operations
 */
void rxInit(Receiver* settings) {
	// set up Input capture monitoring of Receiver
	overflow_count = 0;

	HAL_TIM_Base_Start_IT(&htim2);
    HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_1); // rising edge channel
    HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_2); // falling edge channel
}

bool isRxEnabled() {
	return HAL_GPIO_ReadPin(RX_EN_GPIO_Port, RX_EN_Pin) == (RX_RADIO_EN_POLARITY ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/*
 * Enable receiver radio
 */
void enableRx() {
	HAL_GPIO_WritePin(RX_EN_GPIO_Port, RX_EN_Pin, RX_RADIO_EN_POLARITY ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/*
 * Disable receiver radio
 */
void disableRx() {
	HAL_GPIO_WritePin(RX_EN_GPIO_Port, RX_EN_Pin, RX_RADIO_EN_POLARITY ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

////////////////////////////////////////////////////////////////////////

/*
 * Clear a receiver buffer to its zero state
 */
void clearRxPacket(RxPacket* buf) {
	memset(buf->word, 0, sizeof(buf->word));
	buf->len = 0;
	buf->long_us = 0;
	buf->short_us = 0;
	buf->period_us = 0;
	buf->logic = false;
}

/*
 * Callback to fire when a word is ready. Data is in buffer, length of
 * word is 'count'
 */
void receivedWord(RxCorrelBuffer* correl, RxPacket* data) {
	// ignore the word if it's outside the bounds of min and max word lengths
	if (data->len < correl->min_word_len || data->len > correl->max_word_len + (rx.ignore_sync_bit ? 0 : 1)) return;
	data->long_us = mode(measLong, data->len);
	data->short_us = mode(measShort, data->len);
	data->period_us = mode(measPeriod, data->len);

	// if the correlation cache has timed out, clear it before adding
	uint32_t delta = 0;
	if (HAL_GetTick() < correl->last_word_time_ms)
		delta = (UINT32_MAX - correl->last_word_time_ms) + HAL_GetTick();
	else
		delta = HAL_GetTick() - correl->last_word_time_ms;

	if (delta * 1000 >= correl->timeout_us) {
		for (uint8_t i = 0; i <= correl->index; i++) {
			clearRxPacket(&correl->received[i]);
		}
		correl->last_match = 0;
		correl->index = 0;
	}

	// inject packet to correlation buffer and log time
	correl->received[correl->index].len = data->len;
	correl->received[correl->index].long_us = data->long_us;
	correl->received[correl->index].short_us = data->short_us;
	correl->received[correl->index].period_us = data->period_us;
	correl->received[correl->index].logic = data->logic;
	memcpy(correl->received[correl->index].word, data->word, sizeof(data->word));

	correl->index = (correl->index + 1) % RX_CORREL_WORDS;
	correl->last_word_time_ms = HAL_GetTick();

	RxPacket* this_match = 0;
	// run correlation; call to user feedback function if correlation
	// is above the defined threshold
	for (int i = 0; i < (int) correl->index - (int) correl->match_thresh; i++) {
		uint8_t matches = 1;
		// skip any matching of the last sent word
		if (strcmp(correl->last_match->word, correl->received[i].word) == 0)
			continue;

		for (int j = i + 1; j < correl->index; j++) {
			// skip this j if string lengths mismatch
			if (strlen(correl->received[i].word) != strlen(correl->received[j].word))
				continue;

			// check if the strings match
			if (strcmp(correl->received[i].word, correl->received[j].word) == 0)
				matches++;
		}

		// check if match count is greater than the threshold
		if (matches >= correl->match_thresh) {
			this_match = &correl->received[i];
			break;
		}
	}

	if (this_match && correl->last_match != this_match) {
		// match threshold met; make sure it hasn't already been sent
		status |= (RX_WORD_AVAILABLE << 16);
		correl->last_match = this_match;
	}
}

/*
 * Check if there's a sentence ready to be processed
 */
void checkRxBuffers() {
	// FIXME: overflow entry logic with overflow_count enabled
	// check for end of a word via timeout operation; only act if there's no period info
	if (((overflow_count << 16) + TIM2->CNT) >= rx.bit_max_period && rx.tgt_idx ^ rx.stor_idx) {
		// if there's been a counter overflow
		// mark the end of the sample and increment the storage idx
		if (rx.measured_widths[rx.stor_idx]) {
			rx.measured_periods[rx.stor_idx] = (overflow_count << 16) + TIM2->CNT;
			overflow_count = 0;
			rx.stor_idx++;
			rx.stor_idx %= RX_BUFFER_SAMPLES;
		}

		rx.tgt_idx = rx.stor_idx;
	}

	if(rx.proc_idx == rx.tgt_idx)
		// sample to process is equal to target index; return
		return;

	// get a count of how many samples were in last chunk; copy data for sorting in buffers
	uint16_t sample_ct = 0;
	memset(measPeriodSorted, 0, sizeof(measPeriodSorted));
	if(rx.tgt_idx < rx.proc_idx) {
		// rollover occurred
		sample_ct = (RX_BUFFER_SAMPLES - rx.proc_idx) + rx.tgt_idx;
		// copy data to buffers for sorting and processing
		memcpy(measPeriodSorted, &rx.measured_periods[rx.proc_idx], (RX_BUFFER_SAMPLES - rx.proc_idx) * sizeof(rx.measured_periods[0]));
		memcpy(measPeriodSorted + (RX_BUFFER_SAMPLES - rx.proc_idx), rx.measured_periods, (rx.tgt_idx) * sizeof(rx.measured_periods[0]));
	} else {
		// get simple difference (zero case already handled)
		sample_ct = rx.tgt_idx - rx.proc_idx;
		// copy data to buffers for sorting and processing
		memcpy(measPeriodSorted, &rx.measured_periods[rx.proc_idx], sample_ct * sizeof(rx.measured_periods[0]));
	}

	// get the period for the bit rate based on the mode of periods
	uint32_t period_mode = mode(measPeriodSorted, sample_ct);

	// define a buffer to store temporary received word data
	RxPacket packet;
	memset(packet.word, 0, sizeof(packet.word));
	packet.logic = rx.invert_logic;

	// clear measurement buffers for short, long, and period values
	memset(measLong, 0, sizeof(measLong));
	memset(measShort, 0, sizeof(measShort));
	memset(measPeriod, 0, sizeof(measPeriod));

	// note word start index
	uint16_t word_start_idx = rx.proc_idx;

	// loop through samples to build word; since
	while(rx.proc_idx != rx.tgt_idx) {
		// period measured is approximately equal to the mode of periods
		// now check duty cycle to determine whether it's a '1' or '0'
		uint32_t this_period = (rx.measured_periods[rx.proc_idx]);
		uint32_t this_width = (rx.measured_widths[rx.proc_idx]);
		uint16_t duty_pct = (uint16_t) ((float) (this_width * 100) / (float) period_mode);

		// because the received word for OOK can have a sync bit
		//   at the start, optionally ignore the first bit of the received string
		if ((rx.ignore_sync_bit && rx.proc_idx ^ word_start_idx) || !rx.ignore_sync_bit) {
			measPeriod[packet.len] = (this_period > rx.correl.timeout_us) ? period_mode : this_period;
			if (duty_pct < 50) {
				packet.word[packet.len] = rx.invert_logic ? '0' : '1';
				measShort[packet.len] = this_width;
				measLong[packet.len] = measPeriod[packet.len] - measShort[packet.len];
			} else {
				packet.word[packet.len] = rx.invert_logic ? '1' : '0';
				measLong[packet.len] = this_width;
				measShort[packet.len] = measPeriod[packet.len] - measLong[packet.len];
			}
			packet.len++;
		}

		// increment sample to look at next, and clear this sample
		rx.measured_periods[rx.proc_idx] = 0;
		rx.measured_widths[rx.proc_idx] = 0;

		rx.proc_idx++;
		rx.proc_idx %= RX_BUFFER_SAMPLES;

		// handle processing the word to the correlation logic
		if (this_period > period_mode * period_lim && packet.len > 0) {
			// Entry conditions:
			// - the period observed is significantly larger than the mode period (i.e. inter-word delay)
			// - packet data is longer than 0 bits

			receivedWord(&rx.correl, &packet);
			clearRxPacket(&packet);
			word_start_idx = rx.proc_idx;
		}
	}
}

// ==================== HAL ISR Callback ==========================

/*
 * Input capture callback for measuring PWM values of input signal
 */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
	if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) { // if interrupt is rising edge
		// get period between last 2 rising edges
		uint16_t delta = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1) + 1; // correct for 0-based counting

		// store period to buffer
		rx.measured_periods[rx.stor_idx] = delta + (overflow_count << 16);
		overflow_count = 0;

		if(rx.measured_widths[rx.stor_idx]) {
			// increment the current sample mod sample count
			rx.stor_idx++;
			rx.stor_idx %= RX_BUFFER_SAMPLES;
		}
	} else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) { // if interrupt is falling edge (duty cycle info)
		// capture pulse width
		uint16_t delta = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2) + 1; // correct for 0-based counting

		// save the measurement
		rx.measured_widths[rx.stor_idx] = delta + (overflow_count << 16);
		overflow_count = 0;
	}
}

/*
 * Timer Overflowed interrupt
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	// discard the current sample if the timer period elapses
	// however this is also called on the rising edge (since it resets the timer)
	if (overflow_count < (rx.bit_max_period >> 16))
		overflow_count++;
}
