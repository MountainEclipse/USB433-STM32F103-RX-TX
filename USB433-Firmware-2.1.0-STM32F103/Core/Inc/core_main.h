/*
 * core_main.h
 *
 *  Created on: May 14, 2025
 *      Author: RM
 */

#ifndef INC_CORE_MAIN_H_
#define INC_CORE_MAIN_H_

// system status flag and bit values
extern uint32_t status;

// entry point funcs from main.cpp
int USER_setup(void);
int USER_loop(void);
void USER_shutdown(void);
void USER_error(void);

#endif /* INC_CORE_MAIN_H_ */
