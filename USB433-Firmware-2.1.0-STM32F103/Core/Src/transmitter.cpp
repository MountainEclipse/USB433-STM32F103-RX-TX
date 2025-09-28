/*
 * transmitter.cpp
 *
 *  Created on: Jun 26, 2025
 *      Author: R
 */

#include "stm32f1xx_hal.h"

#include "stdio.h"
#include "string.h"
#include "limits.h"

#include "core_main.h"
#include "main.h"
#include "transmitter.h"
#include "receiver.h"

Transmitter tx;
TxPacket data;

/*
 * Initialize timers and parameters needed for OOK Tx operations
 */
void txInit(Transmitter* settings) {
	__HAL_TIM_ENABLE_DMA(&htim1, TIM_DMA_UPDATE);

	// set up ARR register for Transmitter
	updateARR(settings); // set TIM1 ARR for TX generation frequency
}

/*
 * Update the Tx timing buffer ARR for signal period, based on configured
 * long and short time values
 */
void updateARR(Transmitter* settings) {
	// ARR is 0-based, so correct for that by subtracting 1
	htim1.Instance->ARR = (uint32_t) (settings->t_long + settings->t_short) - 1;
}

/*
 * Prepare a packet to transmit using the 0 index of settings->buffer
 */
void makeTxPacket(Transmitter* settings, TxPacket* packet) {
	if (settings->buffer[0][0] == 0){
		status |= (TX_BUFFER_EMPTY << 8);
		return;
	} else {
		status &= ~(TX_BUFFER_EMPTY << 8);
	}

	// reset the packet to initial conditions
	packet->dma_len = 0;
	packet->frames_sent = 0;
	memset(packet->dma_buffer, 0, sizeof(packet->dma_buffer));

	for (uint16_t i = 0; i < strlen(settings->buffer[0]); i++) {
		if (settings->buffer[0][i] == '1') {
			packet->dma_buffer[packet->dma_len++] =
				settings->invert_logic ? settings->t_long : settings->t_short;
		} else if (settings->buffer[0][i] == '0') {
			packet->dma_buffer[packet->dma_len++] =
				settings->invert_logic ? settings->t_short : settings->t_long;
		} else {
			packet->dma_len = 0;
			memset(packet->dma_buffer, 0, sizeof(packet->dma_buffer));

			status |= (TX_PREP_FAILED << 8);
			return;
		}
	}

	// add final bit to account for "stop" condition
	packet->dma_buffer[packet->dma_len++] = 0;
}

/*
 * Handle the transmission dispatch process based on frames and burst completion for a packet.
 */
void processTx(Transmitter* settings, TxPacket* packet) {
	uint32_t now;
	if (packet->burst_complete) {
		// any prior transmission has completed
		now = HAL_GetTick();
		if (now < packet->last_frame_time_ms || (now - packet->last_frame_time_ms) * 1000 > settings->burst_delay_us) { // inter-burst delay elapsed
			// next burst needs to be queued
			makeTxPacket(settings, packet);

			// check if there's a no-data or failed prep flag
			if ((status >> 8) & (TX_BUFFER_EMPTY | TX_PREP_FAILED)) {
				return;
			}

			// start the burst transmission
			packet->burst_complete = false;
			status &= (TX_COMPLETE << 8); // set status flag as tx incomplete

			HAL_GPIO_WritePin(TX_ACT_GPIO_Port, TX_ACT_Pin, GPIO_PIN_SET);

			// disable receive radio before transmission starts
			if (rx.mode == 2 && isRxEnabled()) {
				disableRx();
			}
		} else {
			// inter-burst timeout hasn't happened, so return;
			return;
		}
	}

	if (packet->frame_complete) {
		// a frame transmission is complete within a burst
		now = HAL_GetTick();
		if (now < packet->last_frame_time_ms) {
			// tick counter has rolled over. Adjust
			packet->last_frame_time_ms = UINT32_MAX - packet->last_frame_time_ms;
		}

		if (packet->frames_sent > settings->frame_repeat) {
			// the number of frames sent equals the desired burst amount
			packet->burst_complete = true;
			status |= (TX_COMPLETE << 8); // indicate transmission complete

			HAL_GPIO_WritePin(TX_ACT_GPIO_Port, TX_ACT_Pin, GPIO_PIN_RESET);

			// re-enable the receive radio if rx mode is
			if (rx.mode == 2 && !isRxEnabled()) {
				// enable radio if it's currently disabled
				enableRx();
			}
		} else if (now < packet->last_frame_time_ms || now - packet->last_frame_time_ms >= (settings->frame_delay_us / (float) 1000)) {
			// an adequate delay has elapsed between frames, trigger the next transmission
			// (tick, even after adjustment for rollover is earlier than last time or delta elapsed)

			// set frame complete false, increment number of frames sent, and trigger transmit
			packet->frame_complete = false;
			packet->frames_sent++;

			HAL_TIM_PWM_Start_DMA(&htim1, TIM_CHANNEL_1, (uint32_t *) packet->dma_buffer, packet->dma_len);
		}
	}
}

// ==================== HAL ISR Callback ==========================

/*
 * Interrupt callback to execute when transmit data is complete
 *  - i.e. last data has been sent to register - so data needs to be n+1, where
 *  	   final data is a null value; null value will be first pulse sent.
 */
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim) {
	if(htim->Instance == TIM1) {
		if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
			HAL_TIM_PWM_Stop_DMA(htim, TIM_CHANNEL_1);
			data.frame_complete = true;
			data.last_frame_time_ms = HAL_GetTick();
		}
	}
}
