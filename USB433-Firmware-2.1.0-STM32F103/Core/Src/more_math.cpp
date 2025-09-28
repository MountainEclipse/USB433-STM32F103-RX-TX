/*
 * more_math.cpp
 *
 *  Created on: May 15, 2025
 *      Author: RM
 */

#include "more_math.h"
#include "stdint.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"

uint16_t min(uint16_t a, uint16_t b) {
	return (a <= b) ? a : b;
}

uint16_t max(uint16_t a, uint16_t b) {
	return (a >= b) ? a : b;
}

float min(float a, float b) {
	return (a <= b) ? a : b;
}

float max(float a, float b) {
	return (a >= b) ? a : b;
}

/*
 * Find the mode of an array of unsigned integers
 */
uint32_t mode(uint32_t arr[], uint16_t size) {
	int max_count = 0, i, j;
	uint32_t max_value = 0;

	// sort the array
	qsort(arr, size, sizeof(arr[0]), compare);

	//Iterate through sorted array to count occurrences of values
	for (i = 0; i < (int) size; i++) {
		int cnt = 1;

		// count occurrences of current element
		for (j = i + 1; j < (int) size; j++) {
			if (arr[j] == arr[i]) {
				cnt++;
			} else {
				break;
			}
		}

		// Update max count and value if current element's count is greater
		if (cnt > max_count) {
			max_count = cnt;
			max_value = arr[i];
		}

		// move to next distinct element
		i = j - 1;
	}

	return max_value;
}

/*
 * Comparator function for qsort
 */
int compare(const void* a, const void* b) {
	return (*(int*)a - *(int*)b);
}

/*
 * Compute average of a list for count values
 */
uint32_t avg(uint32_t* data, size_t count) {
	uint64_t sum = 0;
	for (unsigned int i = 0; i < count; i++) {
		sum += data[i];
	}
	return (uint32_t) (sum / count);
}
