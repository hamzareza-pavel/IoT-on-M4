// Timer Service Library
// Jason Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

// Hardware configuration:
// Timer 4

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#ifndef TIMER_H_
#define TIMER_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "tm4c123gh6pm.h"
#include "gpio.h"
#include "spi0.h"
#include "wait.h"

#define BLUE_LED PORTF,2

typedef void (*_callback)();

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void initTimer();
bool startOneshotTimer(_callback callback, uint32_t seconds);
bool startPeriodicTimer(_callback callback, uint32_t seconds);
bool stopTimer(_callback callback);
bool restartTimer(_callback callback);

void flashBlue();
void flashRed();
void flashGreen();


#endif
