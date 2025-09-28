/*
 * commands.cpp
 *
 *  Created on: May 31, 2025
 *      Author: R
 */

#include "usbd_cdc_if.h"

#include "inttypes.h"
#include "ctype.h"

#include "main.h"
#include "core_main.h"
#include "commands.h"
#include "transmitter.h"
#include "receiver.h"

// USB RX / TX buffers
char usb_tx_buffer[TX_BUFFER_SIZE];

// tracking variable for last activity time on USB, in millis
uint32_t last_USB_time = 0;

// Child nodes for "rx word"
const CommandNode rx_word_commands[] = {
	{ "matchcount", handleRxMatchCount, 0, 0 },
	{ "timeout", handleRxTimeout, 0, 0 },
	{ "minlength", handleRxMinLength, 0, 0 },
	{ "maxlength", handleRxMaxLength, 0, 0 }
};

// Child nodes for "rx"
const CommandNode rx_commands[] = {
	{ "mode", handleRxMode, 0, 0 },
	{ "bitperiod", handleBitPeriod, 0, 0 },
	{ "word", 0 , rx_word_commands, 4 },
	{ "ignoresyncbit", handleSyncBit, 0, 0},
	{ "logic", handleLogic, 0, 0 }
};

// Child nodes for "tx time"
const CommandNode tx_time_commands[] = {
	{ "long", handleTxLong, 0, 0 },
	{ "short", handleTxShort, 0, 0 }
};

// Child nodes for "tx delay"
const CommandNode tx_delay_commands[] = {
	{ "frame", handleTxFrameDelay, 0, 0 },
	{ "burst", handleTxBurstDelay, 0, 0 }
};

// Child nodes for "tx"
const CommandNode tx_commands[] = {
    { "time", 0, tx_time_commands, 2 },
	{ "delay", 0, tx_delay_commands, 2 },
	{ "ignoresyncbit", handleSyncBit, 0, 0 },
	{ "repeat", handleTxRepeat, 0, 0 },
	{ "logic", handleLogic, 0, 0 }
};

// Top-level commands
const CommandNode usb_nodes[] = {
    { "rx", 0, rx_commands, 5 },
    { "tx", handleTxWord, tx_commands, 5 },
	{ "status", handleStatus, 0, 0, },
	{ "version", handleVersion, 0, 0 }
};

#define USB_COMMAND_COUNT (sizeof(usb_nodes) / sizeof(CommandNode))

/*
 * Utility: Split command into list of tokens
 */
static int tokenize(char* input, char* argv[], int max_tokens) {
    int count = 0;
    char* token = strtok(input, " \r\n");
    while (token && count < max_tokens) {
        argv[count++] = token;
        token = strtok(0, " \r\n");
    }
    return count;
}

/*
 * Command dispatcher using the command hierarchy defined above
 */
bool dispatch(const CommandNode* nodes, uint8_t count, CommandContext* ctx, int index) {
	for (uint8_t i = 0; i < count; i++) {
		if (strcmp(ctx->argv[index], nodes[i].token) == 0) {
			bool child_handled = false;

			if (index + 1 < ctx->argc && nodes[i].children) {
				// first try to dispatch the command to child nodes
				ctx->arg_idx = index;
				child_handled = dispatch(nodes[i].children, nodes[i].child_count, ctx, index + 1);
			}
			// check if child failed to handle the command and if a handler is specified for this command
			if (!child_handled && nodes[i].handler) {
				ctx->arg_idx = index;
				ctx->remaining = (index + 1 < ctx->argc) ? ctx->argv[index + 1] : 0;
				nodes[i].handler(ctx);
				return true;
			} else if (!child_handled && index + 1 < ctx->argc) {
				// child didn't handle, and there's an extra param; so the param must be bad
				sprintf(usb_tx_buffer, "%u %s\r\n", USB_CC_BAD_PARAM, ctx->argv[index+1]);
				return true;
			} else if (!child_handled) {
				// child didn't handle, and there's no handler here; we must be missing a param
				sprintf(usb_tx_buffer, "%u\r\n", USB_CC_MISSING_PARAM);
				return true;
			}

			return child_handled;
		}
	}
	sprintf(usb_tx_buffer, "%u %s\r\n", USB_CC_UNKNOWN, ctx->argv[index]);
	return false;
}

// ========== USB SLAVE COMMAND DICTIONARY ==============
/*
 * ****** RECEIVER USER PARAMETERS ******
 *  // TODO: allow for hex string data sending
 *  - status						// return the value of the 'status' variable
 *  - rx ...						// receive commands
 * 		+ mode 						// get current rx mode
 * 		+ mode <0:1:2>				// set rx mode: 0=always off, 1=always on, 2=off during transmit
 * 		+ bitperiod					// get max bit period width, in microseconds
 * 		+ bitperiod <uint32_t>		// set max bit period width, in microseconds
 * 		+ word ...
 * 			+ matchcount			// how many words must match before being considered a "valid" word
 * 			+ matchcount <uint8_t> 	// set match count threshold
 * 			+ minlength				// minimum length of a received word; shorter words get discarded
 * 			+ minlength <uint8_t>	// set minimum length of a received word;
 * 			+ maxlength				// maximum length of a received word; longer words get discarded
 * 			+ maxlength <uint8_t>	// set maximum length of a received word;
 * 			+ timeout				// get timeout for receive correlation buffer; clear buffer if nothing received after timeout, in microseconds
 *	 		+ timeout <uint32_t>	// set timeout for rx correlation buffer, in microseconds
 *		+ ignoresyncbit				// get status of whether sync bit should be ignored
 *		+ ignoresyncbit <0:1>		// set whether sync bit should be ignored (0=no, 1=yes)
 *		+ logic						// get receiver logic format; 0:long high == 0; 1: long high == 1
 * 		+ logic <1:0>				// set receiver logic format
 *
 * 	- tx ...						// transmit commands; if blank, returns any queued data or MISSING_PARAM error
 * 		+ time ...					// timing parameters
 * 			+ short					// get short tx pulse duration, in microseconds
 * 			+ short <uint16_t>		// set short tx pulse duration, in microseconds
 * 			+ long					// get long tx pulse duraiton, in microseconds
 * 			+ long <uint16_t> 		// set long tx pulse duraiton, in microseconds
 * 		+ delay
 * 			+ frame					// get time delay between frames, in microseconds, for a burst transmission
 * 			+ frame <uint32_t>		// set time delay, in microseconds, between frames transmitted
 * 			+ burst					// get time delay between bursts of frames (e.g. if transmitting and there's a queued word to transmit)
 * 			+ burst <uint32_t>		// set time delay between bursts of frames, in microseconds.
 * 		+ ignoresyncbit				// get whether a sync bit should be transmitted - i.e. data transmission start timing bit
 * 		+ ignoresyncbit <0:1>		// set whether a sync bit should be transmitted
 * 		+ repeat					// get how many times a transmit frame gets repeated
 * 		+ repeat <uint8_t>			// set how many times a transmit frame gets repeated
 * 		+ <sequence of 0:1>			// transmit a word, defined by a string of up to 64 binary 1:0 chars.
 *		+ logic						// get transmitter logic format; 0:long high == 0; 1: long high == 1
 * 		+ logic <1:0>				// set transmitter logic format
 *
 *
 *	****** RECEIVER OUTPUT SENTENCE ******
 *	<status> word:<0:1 string> len:<length of word> long_us:<us> short_us:<us> period_us:<us> logic:0 ignoresync:1
 *		// when the receiver detects a valid word, transmit it to the usb host
 *		// with timing information and logic assumption
 */
void processUSB() {
	// check for null receive buffer
	if ((uint8_t) usb_rx_buffer[0] == 0) {
		return;
	}
	// flash USB activity light on
	HAL_GPIO_WritePin(USB_ACT_GPIO_Port, USB_ACT_Pin, GPIO_PIN_SET);

	// reset tx buffer for command acknowledge
	memset(usb_tx_buffer, 0, sizeof(usb_tx_buffer));

	// process command received
    CommandContext ctx;
    ctx.argc = tokenize(usb_rx_buffer, ctx.argv, MAX_COMMAND_TOKENS);
    ctx.remaining = 0;

    if (ctx.argc > 0) {
        dispatch(usb_nodes, USB_COMMAND_COUNT, &ctx, 0);
    }

    // transmit any error messages or feedback
	CDC_Transmit_FS((uint8_t*) usb_tx_buffer, strlen(usb_tx_buffer));

    // reset the command buffer
    memset(usb_rx_buffer, 0, sizeof(usb_rx_buffer));

    // note last time of USB access
    last_USB_time = HAL_GetTick();
}

void pushUSB() {
	HAL_GPIO_WritePin(USB_ACT_GPIO_Port, USB_ACT_Pin, GPIO_PIN_SET);
	CDC_Transmit_FS((uint8_t*) usb_tx_buffer, strlen(usb_tx_buffer));
	last_USB_time = HAL_GetTick();
}

/*
* Handle the command "<rx:tx> logic <0:1>"
*/
void handleLogic(CommandContext* ctx) {
	bool isRx = strncmp(ctx->argv[ctx->arg_idx - 1], "rx", 1) == 0;
	if (ctx->remaining) { // value passed with call
		uint8_t value = atoi(ctx->remaining); // parse argument
		if (value > 1) { // invalid range of values
			sprintf(usb_tx_buffer, "%u %u\r\n", USB_CC_BAD_VALUE, (unsigned int) value);
			return;
		}
		if (isRx)
			rx.invert_logic = (bool) value;
		else
			tx.invert_logic = (bool) value;
		bufferOk();
		return;
	}
	bufferValueResponse(ctx, isRx ? (bool) rx.invert_logic : (bool) tx.invert_logic);
}

/*
 * Handle command "<rx:tx> ignoresyncbit <0:1>
 */
void handleSyncBit(CommandContext* ctx) {
	bool isRx = strncmp(ctx->argv[ctx->arg_idx - 1], "rx", 1) == 0;
	if (ctx->remaining) {// value passed with call
		uint32_t value = atoi(ctx->remaining); // parse argument
		if (value > 1) {
			sprintf(usb_tx_buffer, "%u %" PRIu32 "\r\n", USB_CC_BAD_VALUE, value);
			return;
		}
		if (isRx)
			rx.ignore_sync_bit = (bool) value;
		else
			tx.ignore_sync_bit = (bool) value;
		bufferOk();
		return;
	}
	bufferValueResponse(ctx, isRx ? (long) rx.ignore_sync_bit : (long) tx.ignore_sync_bit);

}

/*
 * Handle command "status"
 */
void handleStatus(CommandContext* ctx) {
	if (ctx->remaining) { // value passed with call - illegal
		sprintf(usb_tx_buffer, "%u %s\r\n", USB_CC_BAD_PARAM, ctx->remaining);
		return;
	}
	bufferValueResponse(ctx, status);

}

/*
 * Handle command "version"
 */
void handleVersion(CommandContext* ctx) {
	if (ctx->remaining) { // value passed with call - illegal
		sprintf(usb_tx_buffer, "%u %s\r\n", USB_CC_BAD_PARAM, ctx->remaining);
		return;
	}
	sprintf(usb_tx_buffer, "%u %s\r\n", USB_CC_OK, version);
}

/*
 * Handle command "rx mode <0:1:2>"
 * to set or get Receiver operating mode
 */
void handleRxMode(CommandContext* ctx) {
	if (ctx->remaining) {// value passed with call
		uint8_t value = atoi(ctx->remaining); // parse argument
		if (value > 2) { // invalid range of values
			sprintf(usb_tx_buffer, "%u %" PRIu8 "\r\n", USB_CC_BAD_VALUE, value);
			return;
		}
		rx.mode = value;
		if (value == 0) disableRx();
		else enableRx();
		bufferOk();
		return;
	}
	bufferValueResponse(ctx, rx.mode);
}

/*
 * Handle the command "rx timeout <uint16_t>"
 */
void handleRxTimeout(CommandContext* ctx) {
	if (ctx->remaining) {// value passed with call
		uint32_t value = atoi(ctx->remaining); // parse argument
		if (value > 5e6 || value < rx.bit_max_period) {
			// invalid range of values... 5 seconds not realistic for data rx
			// also any shorter than the maximum bit period means data blends together
			sprintf(usb_tx_buffer, "%u %" PRIu32 "\r\n", USB_CC_BAD_VALUE, value);
			return;
		}
		rx.correl.timeout_us = value;
		bufferOk();
		return;
	}
	bufferValueResponse(ctx, rx.correl.timeout_us);
}

/*
 * Handle command "rx bitperiod <uint32_t>"
 */
void handleBitPeriod(CommandContext* ctx) {
	if (ctx->remaining) { // value passed with call
		uint32_t value = atoi(ctx->remaining); // parse argument
		rx.bit_max_period = value;
		bufferOk();
		return;
	}
	bufferValueResponse(ctx, rx.bit_max_period);
}

/*
 * Handle the command "rx word matchcount"
 */
void handleRxMatchCount(CommandContext* ctx) {
	if (ctx->remaining) {// value passed with call
		uint8_t value = atoi(ctx->remaining); // parse argument
		if (value > RX_CORREL_WORDS) {
			// wouldn't be able to buffer enough matches
			sprintf(usb_tx_buffer, "%u %u\r\n", USB_CC_BAD_VALUE, (unsigned int) value);
			return;
		}
		rx.correl.match_thresh = value;
		bufferOk();
		return;
	}
	bufferValueResponse(ctx, rx.correl.match_thresh);
}

/*
 * Handle command "rx word minlength"
 */
void handleRxMinLength(CommandContext* ctx) {
	if (ctx->remaining) {// value passed with call
		uint8_t value = atoi(ctx->remaining); // parse argument
		if (value > RX_MAX_BITS || value > rx.correl.max_word_len) {
			// not enough space in the word buffer
			sprintf(usb_tx_buffer, "%u %u\r\n", USB_CC_BAD_VALUE, (unsigned int) value);
			return;
		}
		rx.correl.min_word_len = value;
		bufferOk();
		return;
	}
	bufferValueResponse(ctx, rx.correl.min_word_len);
}

/*
 * Handle command "rx word maxlength"
 */
void handleRxMaxLength(CommandContext* ctx) {
	if (ctx->remaining) {// value passed with call
		uint8_t value = atoi(ctx->remaining); // parse argument
		if (value > RX_MAX_BITS || value < rx.correl.min_word_len) {
			// not enough space in the word buffer
			sprintf(usb_tx_buffer, "%u %u\r\n", USB_CC_BAD_VALUE, (unsigned int) value);
			return;
		}
		rx.correl.max_word_len = value;
		bufferOk();
		return;
	}
	bufferValueResponse(ctx, rx.correl.max_word_len);
}

/*
 * Handle command "tx time long <uint16_t>"
 */
void handleTxLong(CommandContext* ctx) {
	if (ctx->remaining) {// value passed with call
		uint32_t value = atoi(ctx->remaining); // parse argument
		if (value > (UINT16_MAX >> 2) || value < tx.t_short) {
			// not enough space in the word buffer
			sprintf(usb_tx_buffer, "%u %" PRIu32 "\r\n", USB_CC_BAD_VALUE, value);
			return;
		}
		tx.t_long = (uint16_t) value;
		updateARR(&tx);
		bufferOk();
		return;
	}
	bufferValueResponse(ctx, tx.t_long);
}

/*
 * Handle command "tx time short <uint16_t>"
 */
void handleTxShort(CommandContext* ctx) {
	if (ctx->remaining) {// value passed with call
		uint32_t value = atoi(ctx->remaining); // parse argument
		if (value > (UINT16_MAX >> 2) || value > tx.t_long) {
			// not enough space in the word buffer
			sprintf(usb_tx_buffer, "%u %" PRIu32 "\r\n", USB_CC_BAD_VALUE, value);
			return;
		}
		tx.t_short = (uint16_t) value;
		updateARR(&tx);
		bufferOk();
		return;
	}
	bufferValueResponse(ctx, tx.t_short);
}

/*
 * Handle command "tx delay frame <uint32_t>"
 */
void handleTxFrameDelay(CommandContext* ctx) {
	if (ctx->remaining) {// value passed with call
		uint32_t value = atoi(ctx->remaining); // parse argument
		if (value > 50*(tx.t_short + tx.t_long) || value < (tx.t_short + tx.t_long)) {
			// absurdly long delay between frames. Typically it's about 6-8 * t_long
			// constrained to > 1*period; < 50*period
			sprintf(usb_tx_buffer, "%u %" PRIu32 "\r\n", USB_CC_BAD_VALUE, value);
			return;
		}
		tx.frame_delay_us = value;
		bufferOk();
		return;
	}
	bufferValueResponse(ctx, tx.frame_delay_us);
}

/*
 * Handle command "tx delay burst <uint32_t>"
 */
void handleTxBurstDelay(CommandContext* ctx) {
	if (ctx->remaining) {// value passed with call
		uint32_t value = atoi(ctx->remaining); // parse argument
		if (value > 60e6 || value < (tx.t_long + tx.t_short)) {
			// could do up to 60 seconds between transmissions... but that's absurd
			// can't be less than the frame delay
			sprintf(usb_tx_buffer, "%u %" PRIu32 "\r\n", USB_CC_BAD_VALUE, value);
			return;
		}
		tx.burst_delay_us = value;
		bufferOk();
		return;
	}
	bufferValueResponse(ctx, tx.burst_delay_us);
}

/*
 * Handle command "tx repeat <uint8_t>"
 */
void handleTxRepeat(CommandContext* ctx) {
	if (ctx->remaining) {// value passed with call
		uint32_t value = atoi(ctx->remaining); // parse argument
		if (value > 100) {
			// why would you need to repeat more than 100 times??
			sprintf(usb_tx_buffer, "%u %" PRIu32 "\r\n", USB_CC_BAD_VALUE, value);
			return;
		}
		tx.frame_repeat = (uint8_t) value;
		bufferOk();
		return;
	}
	bufferValueResponse(ctx, tx.frame_repeat);
}

/*
#define LED_Pin GPIO_PIN_13
#define LED_GPIO_Port GPIOC
#define RX_EN_Pin GPIO_PIN_6
#define RX_EN_GPIO_Port GPIOB
#define LED2_Pin GPIO_PIN_7
#define LED2_GPIO_Port GPIOB
 */

/*
 * Handle command "tx <sequence 1:0>"
 */
void handleTxWord(CommandContext* ctx) {
	if (!ctx->remaining) {// no value passed with call
		// we're missing a parameter
		sprintf(usb_tx_buffer, "%u\r\n", USB_CC_MISSING_PARAM);
		return;
	}

	// check for valid word length
	if (strlen(ctx->remaining) > TX_MAX_BITS + (tx.ignore_sync_bit ? 1 : 0)) {
		sprintf(usb_tx_buffer, "%u %s\r\n", USB_CC_BAD_VALUE, ctx->remaining);
		return;
	}

	// check if the tx buffer has data in the last index already; if so, return busy error
	if (tx.buffer[TX_BUFFER_LEN - 1][0] != 0) {
		sprintf(usb_tx_buffer, "%u\r\n", USB_CC_BUSY);
		return;
	}

	// parameter passed; check each char for validity of only 1 or 0 binary characters
	for(unsigned int i = 0; i < strlen(ctx->remaining); i++) {
		if (ctx->remaining[i] != '1' && ctx->remaining[i] != '0') {
			// invalid character
			sprintf(usb_tx_buffer, "%u %s\r\n", USB_CC_BAD_VALUE, ctx->remaining);
			return;
		}
	}

	// find the next available index and insert data there
	for (unsigned int i = 0; i < TX_BUFFER_LEN; i++) {
		if (tx.buffer[i][0] == 0) {
			// nothing is queued yet, and data is valid, so push it to the txData buffer
			// add leading '0' as the TX start bit
			sprintf(tx.buffer[i], "%s%s", tx.ignore_sync_bit ? "" : (tx.invert_logic ? "1" : "0"), ctx->remaining);
			bufferOk();
			return;
		}
	}
}

/*
 * Response for getting generic values
 */
void bufferValueResponse(CommandContext* ctx, long responseValue) {
	char* idx = usb_tx_buffer;
	sprintf(usb_tx_buffer, "%u ", USB_CC_OK);
	idx += strlen(usb_tx_buffer);
	for (uint8_t argi = 1; argi < ctx->argc; argi++) {
		sprintf(idx, "%s ", ctx->argv[argi]);
		idx += strlen(ctx->argv[argi]) + 1;
	}
	sprintf(idx, "%" PRId32 "\r\n", responseValue);
}

/*
 * Response for an "OK" to the USB host
 */
void bufferOk() {
	sprintf(usb_tx_buffer, "%u OK\r\n", USB_CC_OK);
}
