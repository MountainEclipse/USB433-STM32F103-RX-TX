/*
 * more_math.h
 *
 *  Created on: May 15, 2025
 *      Author: RM
 */

#ifndef INC_MORE_MATH_H_
#define INC_MORE_MATH_H_

#include "stdint.h"
#include "stdlib.h"

uint16_t min(uint16_t a, uint16_t b);
uint16_t max(uint16_t a, uint16_t b);
float min(float a, float b);
float max(float a, float b);
uint32_t mode(uint32_t arr[], uint16_t size);
int compare(const void* a, const void* b);
uint32_t avg(uint32_t* data, size_t count);

#endif /* INC_MORE_MATH_H_ */
