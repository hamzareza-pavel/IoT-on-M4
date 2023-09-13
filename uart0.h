// UART0 Library
// Jason Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL
// Target uC:       TM4C123GH6PM
// System Clock:    -

// Hardware configuration:
// UART Interface:
//   U0TX (PA1) and U0RX (PA0) are connected to the 2nd controller
//   The USB on the 2nd controller enumerates to an ICDI interface and a virtual COM port

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#ifndef UART0_H_
#define UART0_H_


#define MAX_CHARS 50

#include <stdbool.h>

typedef struct _USER_DATA
{
    char buffer[MAX_CHARS+1];
    char command[15];
    char strParam[15];
    char topic[30];
    char topicvalue[15];
    uint8_t ip[4];
    bool valid;
}USER_DATA;

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void initUart0();
void setUart0BaudRate(uint32_t baudRate, uint32_t fcyc);
void putcUart0(char c);
void putsUart0(char* str);
char getcUart0();
bool kbhitUart0();
void toLowerCase(char* str);
bool isDigit(char c);
uint32_t IPStrToUint32(char *ip);
void printSubscribedTopics();

#endif
