/*
 * localIO.h
 *
 *  Created on: Mar 31, 2015
 *      Author: Aaron Needles
 */

#ifndef LOCALIO_H_
#define LOCALIO_H_

#include <stddef.h> // For NULL
#include "mraa.h"

#define ON_BOARD_LED__VIA_MRAA 13
// D1 is replaced by Serial Port. Others are 2..8 for D2..D8. 0..3 for A0..A3

// Currently D2..D4 work
// Currently D5, D6, D8 work, but D7 does not

// D2..D4
#define START_DIG_OUT 2
#define NUM_DIG_OUT 3
extern mraa_gpio_context digOut[NUM_DIG_OUT];
// D5..D8
#define START_DIG_IN 5
#define NUM_DIG_IN 4
extern mraa_gpio_context digIn[NUM_DIG_IN];
// A0..A3
#define NUM_ANA_IN 4
extern mraa_aio_context anaIn[NUM_ANA_IN];

extern mraa_gpio_context onboardLED;

// Most current values captured
extern uint8_t curDigOuputs;
extern uint8_t curDigInputs;
extern uint16_t curAnaInputs[NUM_ANA_IN];


int init_IO();
void scan_IO();
void writeDigOut(int addr, int val);
void close_IO();

#define MAX_NANOSECS 999999999L
void MicroSleep(long microsecs);


#endif /* LOCALIO_H_ */
