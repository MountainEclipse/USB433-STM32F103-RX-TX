/*
 * commands.h
 *
 *  Created on: May 31, 2025
 *      Author: R
 */

#ifndef INC_COMMANDS_H_
#define INC_COMMANDS_H_

#include "usbd_cdc_if.h"

#include "receiver.h"
#include "transmitter.h"

#define MAX_COMMAND_TOKENS 10

// USB fields
#define USB_CC_OK 0x00 // command completed successfully
#define USB_CC_BUSY 0x01 // can't complete due to a currently executing action -- e.g. tx buffer full
#define USB_CC_UNKNOWN 0x10 // unknown command error
#define USB_CC_BAD_VALUE 0x20 // command is known, but the value is invalid
#define USB_CC_BAD_PARAM 0x30 // parameter for a command is undefined
#define USB_CC_MISSING_PARAM 0x31 // needs a parameter to be sent

#define TX_BUFFER_SIZE APP_RX_DATA_SIZE + 16
extern char usb_tx_buffer[TX_BUFFER_SIZE];
extern uint32_t last_USB_time;
extern const char version[];

// rx/tx structs
extern Transmitter tx;
extern Receiver rx;

typedef struct CommandContext {
    int argc;
    char* argv[MAX_COMMAND_TOKENS];
    char* remaining;
    int arg_idx = -1; // successfully parsed to this point
} CommandContext;

// Function pointer for handlers
typedef void (*CommandHandler)(CommandContext* ctx);

// Node structure for command tree
typedef struct CommandNode {
    const char* token;
    CommandHandler handler;
    const CommandNode* children;
    uint8_t child_count;
} CommandNode;

void processUSB(void);

void pushUSB(void);

// response functions
void bufferOk(void);
void bufferValueResponse(CommandContext* ctx, long responseValue);

// handler functions
void handleLogic(CommandContext* ctx);
void handleSyncBit(CommandContext* ctx);
void handleStatus(CommandContext* ctx);
void handleVersion(CommandContext* ctx);

void handleRxMode(CommandContext* ctx);
void handleRxTimeout(CommandContext* ctx);
void handleBitPeriod(CommandContext* ctx);
void handleRxMatchCount(CommandContext* ctx);
void handleRxMinLength(CommandContext* ctx);
void handleRxMaxLength(CommandContext* ctx);

void handleTxLong(CommandContext* ctx);
void handleTxShort(CommandContext* ctx);
void handleTxFrameDelay(CommandContext* ctx);
void handleTxBurstDelay(CommandContext* ctx);
void handleTxRepeat(CommandContext* ctx);
void handleTxWord(CommandContext* ctx);

#endif /* INC_COMMANDS_H_ */
