/*
 * localIO.c
 *
 *  Created on: Mar 31, 2015
 *      Author: Aaron Needles
 */

#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include "mraa.h"
#include "localIO.h"

// External
mraa_gpio_context digOut[NUM_DIG_OUT] = {NULL, NULL, NULL};
mraa_gpio_context digIn[NUM_DIG_IN] = {NULL, NULL, NULL, NULL};
mraa_aio_context anaIn[NUM_ANA_IN] = {NULL, NULL, NULL};

mraa_gpio_context onboardLED = NULL;

// Most current values captured
uint8_t curDigOuputs;
uint8_t curDigInputs;
uint16_t curAnaInputs[NUM_ANA_IN];

// Local


/*
 * init_IO() -  * Initialize analog and digital IO points.
 * Return: 0 for fault, 1 for success.
 */
int init_IO()
{
	int i;

	// Make sure we're running on the Edison.
	// (Review GPIO mappings if compiling for another target.)
	mraa_platform_t platform = mraa_get_platform_type();
	if (platform != MRAA_INTEL_EDISON_FAB_C) {
		return 0;
	}

	// Digital Output (D2..D4)
	for (i=0; i<NUM_DIG_OUT; i++) {
		digOut[i] = mraa_gpio_init(START_DIG_OUT + i);
		if (digOut[i] == NULL) {
			fprintf(stderr, "mraa_gpio_init ... Exiting.");
			return 0;
		}
		// Set the pin as output
		if (mraa_gpio_dir(digOut[i], MRAA_GPIO_OUT) != MRAA_SUCCESS) {
			fprintf(stderr, "mraa_gpio_dir (OUT) ... Exiting.");
			return 0;
		};
	}

	// Digital Input (D5..D8)
	for (i=0; i<NUM_DIG_IN; i++) {
		// TBD - skip third in sequence which is D7
		if (i == 2) {
			continue;
		}

		digIn[i] = mraa_gpio_init(START_DIG_IN + i);
		if (digIn[i] == NULL) {
			fprintf(stderr, "mraa_gpio_init ... Exiting.");
			return 0;
		}
		// Set the pin as input (although example may have not included this)
		if (mraa_gpio_dir(digOut[i], MRAA_GPIO_IN) != MRAA_SUCCESS) {
			fprintf(stderr, "mraa_gpio_dir (IN) ... Exiting.");
			return 0;
		};
	}

/*
	// Onboard LED
#define CHANNEL_EDISON_ONBOARD_LED 7
	onboardLED = mraa_gpio_init(CHANNEL_EDISON_ONBOARD_LED);
	if (onboardLED == NULL) {
		fprintf(stderr, "mraa_gpio_init ... Exiting.");
		return 0;
	}

	// set the pin as output
	if (mraa_gpio_dir(onboardLED, MRAA_GPIO_OUT) != MRAA_SUCCESS) {
		fprintf(stderr, "mraa_gpio_dir ... Exiting.");
		return 0;
	};
*/

	// Analog Input
	for (i=0; i<NUM_ANA_IN; i++) {
		anaIn[i] = mraa_aio_init(i);
		if (anaIn[i] == NULL) {
			fprintf(stderr, "mraa_aio_init ... Exiting.");
			return 0;
		}
	}

	return 1; // Success
}

void scan_IO()
{
	int i;
	int temp;

	curDigInputs = 0;
	for (i=0; i<NUM_DIG_IN; i++) {
		if (digIn[i] == NULL) {
			temp = -1;
		}
		else {
			temp = mraa_gpio_read(digIn[i]);
		}
		if (temp != -1) {
			curDigInputs |= (temp << i);
		}
	}

	for (i=0; i<NUM_ANA_IN; i++) {
		curAnaInputs[i] = mraa_aio_read(anaIn[i]);
	}
}

void writeDigOut(int addr, int val)
{
	// Write it
	mraa_gpio_write(digOut[(int)addr], val);
}

void close_IO()
{
	int i;

	for (i=0; i<NUM_DIG_IN; i++) {
		mraa_gpio_close(digIn[i]);
	}
	for (i=0; i<NUM_DIG_OUT; i++) {
	    mraa_gpio_close(digOut[i]);
	}
	for (i=0; i<NUM_ANA_IN; i++) {
	    mraa_aio_close(anaIn[i]);
	}
}

/*
 * MicroSleep(long microsecs) -  Sleep for this number of microseconds, up to 1 second.
 * Return: 0 for fault, 1 for success.
 *
 * Refrence: http://www.linuxhowtos.org/manpages/3p/nanosleep.htm
 */
void MicroSleep(long microsecs)
{
	struct timespec nano;
	long nanosecs;

	nanosecs = microsecs * 1000;
	if (nanosecs > MAX_NANOSECS) {
		nanosecs = MAX_NANOSECS;
	}
	nano.tv_sec = 0;
	nano.tv_nsec = nanosecs;

	nanosleep(&nano, NULL);
}
