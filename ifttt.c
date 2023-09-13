/*
 * ifttt.c
 *      Author: Pavel
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "ifttt.h"
#include "uart0.h"

#define IFTTT_MAX_OUTPUT_NUMBER 3
#define IFTTT_MAX_INPUT_NUMBER 3

char supportedInputs[IFTTT_MAX_INPUT_NUMBER][15]   = {
                           "PUSHBUTTON",
                           "UART",
                           "UDP",
};

char  supportedOutputs[IFTTT_MAX_OUTPUT_NUMBER][15] = {
                            "LED",
                            "UART",
                            "UDP",
};

void printInputList(char* strMsg)
{
    uint8_t i = 0;
    putsUart0(strMsg);
    putcUart0('\n');
    for(i = 0; i< IFTTT_MAX_INPUT_NUMBER; i++)
    {

        putsUart0(supportedInputs[i]);
        putcUart0(', ');

    }
    putcUart0('\n');
}

void printOutputList(char* strMsg)
{
    uint8_t i = 0;
    putsUart0(strMsg);
    putcUart0('\n');
    for(i = 0; i< IFTTT_MAX_OUTPUT_NUMBER; i++)
    {
        putsUart0(supportedOutputs[i]);
        putcUart0(', ');

    }
    putcUart0('\n');
}
