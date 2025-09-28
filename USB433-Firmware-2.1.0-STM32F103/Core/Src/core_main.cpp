/*
 * core_main.cpp
 *
 *  Created on: May 14, 2025
 *      Author: RM
 */
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_tim.h"
#include "usbd_cdc_if.h"

#include "stdio.h"
#include "stdint.h"
#include "string.h"
#include "inttypes.h"

#include "main.h"
#include "core_main.h"
#include "commands.h"
#include "transmitter.h"
#include "receiver.h"

// errors and system status flags
// This status is sectioned into 4 bytes:
// bits  0 -  7: USB response values (not handled directly here)
// bits  8 - 15: Transmitter process status
// bits 16 - 23: Receiver process status
uint32_t status = 0;

// Firmware version string
const char version[] = "2.1.0";

// timeout parameter, in millis, for the USB activity indicator
//   to turn off after last Read or Write activity
uint16_t timeout_ms = 250;

// ================= OOK RX PARAMS ======================



// =================== SYS PARAMS =======================

int USER_setup(void) {
	txInit(&tx); // initialize Tx function
	rxInit(&rx); // initialize Rx function

	// flash on-board LED to indicate system active
	HAL_GPIO_WritePin(USB_ACT_GPIO_Port, USB_ACT_Pin, GPIO_PIN_SET);
	HAL_Delay(500);
	HAL_GPIO_WritePin(USB_ACT_GPIO_Port, USB_ACT_Pin, GPIO_PIN_RESET);

	if (rx.mode > 0) {
		enableRx(); // enable radio receiver
	}
	return 0;
}

/*
 * User-defined loop structure. There are 3 loop return conditions:
 * return 0: loop should continue
 * return >0: loop should terminate, no error handling; program will execute USER_shutdown
 * return <0: loop should terminate, errors to report; program will execute USER_Error
 */
int USER_loop(void) {
	// process any potential USB data received
	processUSB();

	// process updates to current / next transmission
	processTx(&tx, &data);

	// handle system feedback due to transmit status values
	if (((status >> 8) & 0xFF) > TX_BUFFER_EMPTY) {
		memset(usb_tx_buffer, 0, sizeof(usb_tx_buffer));
		// buffer outputs to usb host on tx buffer prep fail or tx complete flags
		if ((status >> 8) & TX_PREP_FAILED) {
			sprintf(usb_tx_buffer, "%" PRIu32 " %s\r\n", status & (TX_PREP_FAILED << 8), tx.buffer[0]);
		} else if ((status >> 8) & TX_COMPLETE) {
			sprintf(usb_tx_buffer, "%" PRIu32 " %s\r\n", status & (TX_COMPLETE << 8), tx.buffer[0]);
		}

		// tx buffer retains tx data until either a fail to buffer or a transmission complete
		// Once either of those conditions are met, shift the buffer over.
		if ((status >> 8) & (TX_PREP_FAILED | TX_COMPLETE)) {
			// shift the tx_buffer to the left
			memmove(tx.buffer, tx.buffer[1], (TX_BUFFER_LEN - 1) * TX_MAX_BITS);
			memset(tx.buffer[TX_BUFFER_LEN - 1], 0, TX_MAX_BITS);
			status &= ~((TX_PREP_FAILED | TX_COMPLETE) << 8); // clear the flags
		}
		pushUSB();
	}

	// process RF received buffer content
	checkRxBuffers();

	// TODO: check for matching received words, and calculate average of observed timings

	// check if the receiver status is non-zero
	if ((status >> 16) & 0xFF) {
		memset(usb_tx_buffer, 0, sizeof(usb_tx_buffer));
		// data received, so transmit to USB host
		if ((status >> 16) & RX_WORD_AVAILABLE) {

			// statistically calculate the timings based on average of match timings
			RxPacket stat_packet;
			memset(stat_packet.word, 0, sizeof(stat_packet.word)); // clear to zero
			memcpy(stat_packet.word, rx.correl.last_match->word, strlen(rx.correl.last_match->word)); // copy match string

			uint8_t matches = 0;
			for (uint8_t i = 0; i < rx.correl.index; i++) {
				// ignore non-matches
				if (strcmp(stat_packet.word, rx.correl.received[i].word) != 0) {
					continue;
				}

				stat_packet.long_us = (stat_packet.long_us * matches + rx.correl.received[i].long_us) / (matches + 1);
				stat_packet.short_us = (stat_packet.short_us * matches + rx.correl.received[i].short_us) / (matches + 1);
				stat_packet.period_us = (stat_packet.period_us * matches + rx.correl.received[i].period_us) / (matches + 1);

				// increment match counter for average calcs
				matches++;
			}
			sprintf(usb_tx_buffer, "%" PRIu32 " word:%s len:%" PRIu16 " long_us:%" PRIu32 " short_us:%" PRIu32 " period_us:%" PRIu32 " logic:%u ignoresync:%u\r\n",
					status & (RX_WORD_AVAILABLE << 16), stat_packet.word, rx.correl.last_match->len,
					stat_packet.long_us, stat_packet.short_us, stat_packet.period_us,
					(unsigned int) rx.correl.last_match->logic, (unsigned int) rx.ignore_sync_bit);
			status &= ~(RX_WORD_AVAILABLE << 16);
		}
		pushUSB();
	}

	// check when last USB activity was, and turn off activity LED after timeout
	uint32_t delta = (HAL_GetTick() >= last_USB_time) ? HAL_GetTick() - last_USB_time : HAL_GetTick() + (UINT32_MAX - last_USB_time);
	if (delta > timeout_ms && HAL_GPIO_ReadPin(USB_ACT_GPIO_Port, USB_ACT_Pin) == GPIO_PIN_SET) {
		HAL_GPIO_WritePin(USB_ACT_GPIO_Port, USB_ACT_Pin, GPIO_PIN_RESET);
	}

	return 0;
}

void USER_error(void) {
	// blink LED based on error code 1's and 0's
}

void USER_shutdown(void) {

}
