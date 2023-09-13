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

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "tm4c123gh6pm.h"
#include "uart0.h"
#include "common.h"
#include "dhcp.h"
#include "eeprom.h"
#include "mqtt.h"
#include "ifttt.h"

// PortA masks
#define UART_TX_MASK 2
#define UART_RX_MASK 1

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Initialize UART0
void initUart0()
{
    // Configure HW to work with 16 MHz XTAL, PLL enabled, system clock of 40 MHz
    SYSCTL_RCC_R = SYSCTL_RCC_XTAL_16MHZ | SYSCTL_RCC_OSCSRC_MAIN
            | SYSCTL_RCC_USESYSDIV | (4 << SYSCTL_RCC_SYSDIV_S);

    // Set GPIO ports to use APB (not needed since default configuration -- for clarity)
    SYSCTL_GPIOHBCTL_R = 0;

    // Enable clocks
    SYSCTL_RCGCUART_R |= SYSCTL_RCGCUART_R0;
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R0;
    _delay_cycles(3);

    // Configure UART0 pins
    GPIO_PORTA_DIR_R |= UART_TX_MASK;           // enable output on UART0 TX pin
    GPIO_PORTA_DIR_R &= ~UART_RX_MASK;           // enable input on UART0 RX pin
    GPIO_PORTA_DR2R_R |= UART_TX_MASK; // set drive strength to 2mA (not needed since default configuration -- for clarity)
    GPIO_PORTA_DEN_R |= UART_TX_MASK | UART_RX_MASK; // enable digital on UART0 pins
    GPIO_PORTA_AFSEL_R |= UART_TX_MASK | UART_RX_MASK; // use peripheral to drive PA0, PA1
    GPIO_PORTA_PCTL_R &= ~(GPIO_PCTL_PA1_M | GPIO_PCTL_PA0_M); // clear bits 0-7
    GPIO_PORTA_PCTL_R |= GPIO_PCTL_PA1_U0TX | GPIO_PCTL_PA0_U0RX;
    // select UART0 to drive pins PA0 and PA1: default, added for clarity

    // Configure UART0 to 115200 baud, 8N1 format
    UART0_CTL_R = 0;                 // turn-off UART0 to allow safe programming
    UART0_CC_R = UART_CC_CS_SYSCLK;                 // use system clock (40 MHz)
    UART0_IBRD_R = 21; // r = 40 MHz / (Nx115.2kHz), set floor(r)=21, where N=16
    UART0_FBRD_R = 45;                                  // round(fract(r)*64)=45
    UART0_LCRH_R = UART_LCRH_WLEN_8 | UART_LCRH_FEN; // configure for 8N1 w/ 16-level FIFO
    UART0_CTL_R = UART_CTL_TXE | UART_CTL_RXE | UART_CTL_UARTEN;
    // enable TX, RX, and module
}

// Set baud rate as function of instruction cycle frequency
void setUart0BaudRate(uint32_t baudRate, uint32_t fcyc)
{
    uint32_t divisorTimes128 = (fcyc * 8) / baudRate; // calculate divisor (r) in units of 1/128,
                                                      // where r = fcyc / 16 * baudRate
    UART0_IBRD_R = divisorTimes128 >> 7;        // set integer value to floor(r)
    UART0_FBRD_R = ((divisorTimes128 + 1)) >> 1 & 63; // set fractional value to round(fract(r)*64)
}

// Blocking function that writes a serial character when the UART buffer is not full
void putcUart0(char c)
{
    while (UART0_FR_R & UART_FR_TXFF)
        ;               // wait if uart0 tx fifo full
    UART0_DR_R = c;                                  // write character to fifo
}

// Blocking function that writes a string when the UART buffer is not full
void putsUart0(char* str)
{
    uint8_t i = 0;
    while (str[i] != '\0')
        putcUart0(str[i++]);
}

// Blocking function that returns with serial data once the buffer is not empty
char getcUart0()
{
    while (UART0_FR_R & UART_FR_RXFE)
        ;               // wait if uart0 rx fifo empty
    return UART0_DR_R & 0xFF;                        // get character from fifo
}

void toLowerCase(char* str)
{
    int index = 0;

    while (str[index] != '\0')
    {
        if (str[index] >= 'A' && str[index] <= 'Z')
        {
            str[index] = str[index] + 32;
        }
        index++;
    }
}
//Receives characters from the user interface
void getsUart0(USER_DATA* data)
{
    int i, count = 0;
    char c = '\0';
    for (i = 0; i < MAX_CHARS && c != 13 && c != 10; i++)
    {
        c = getcUart0();
        putcUart0(c);
        if (c == 8 || c == 127)
        {
            if (count > 0)
            {
                count = count - 1;
            }
        }
        else if (c < 32)
        {
            continue;
        }
        else
        {
            data->buffer[count++] = c;
        }
    }
    data->buffer[count] = '\0';
    toLowerCase(data->buffer);
    putcUart0('\n');
    return;
}

void parseFields(USER_DATA* data)
{
    char* token = strtok(data->buffer, " ");
    data->valid = true;
    strcpy(data->command, token);
    uint8_t i = 0, j = 0;
    for (i = 0;; i++)
    {
        token = strtok(NULL, " ");
        if (token == NULL)
        {
            break;
        }
        if((strcmp(data->command, "publish") == 0 || strcmp(data->command, "subscribe") == 0 || strcmp(data->command, "unsubscribe") == 0))
        {
            if(i == 0)
            {
                strcpy(data->topic, token);
            }
            else if(i == 1 && strcmp(data->command, "publish") == 0)
            {
                strcpy(data->topicvalue, token);
            }
        }
        else if (i == 0)
        {
            strcpy(data->strParam, token);
        }
        else if (i == 1 && strcmp(data->command, "set") == 0)
        {
            ipconv.ip32_t = IPStrToUint32(token);
            for (j = 0; j < 4; j++)
            {
                data->ip[j] = ipconv.ip[j];
            }
        }
    }
}

// Returns the status of the receive buffer
bool kbhitUart0()
{
    return !(UART0_FR_R & UART_FR_RXFE);
}

bool isDigit(char c)
{
    return (c >= '0' && c <= '9');
}

uint32_t IPStrToUint32(char *ip)
{
    char c;
    c = *ip;
    uint32_t integer;
    uint32_t val;
    uint8_t i, j = 0;
    for (j = 0; j < 4; j++)
    {
        if (!isdigit(c))
        {
            return (0);
        }
        val = 0;
        for (i = 0; i < 3; i++)
        {
            if (isdigit(c))
            {
                val = (val * 10) + (c - '0');
                c = *++ip;
            }
            else
                break;
        }
        if (val < 0 || val > 255)
        {
            return (0);
        }
        if (c == '.')
        {
            integer = (integer << 8) | val;
            c = *++ip;
        }
        else if (j == 3 && c == '\0')
        {
            integer = (integer << 8) | val;
            break;
        }

    }
    if (c != '\0')
    {
        return (0);
    }
    return (htonl(integer));
}

void executeUrtCommand(USER_DATA* data)
{
    if (strcmp(data->command, "dhcp") == 0)
    {
        if (strcmp(data->strParam, "on") == 0)
        {
            etherEnableDhcpMode();
            // writeEeprom(DHCPENABLED, 0x01);

        }
        else if (strcmp(data->strParam, "off") == 0)
        {
            etherDisableDhcpMode();
            turnOffDhcpTimers();
            //writeEeprom(DHCPENABLED, 0x00);
        }
        else if (strcmp(data->strParam, "refresh") == 0)
        {
            sendDhcpRequestPacket();
        }
        else if (strcmp(data->strParam, "release") == 0)
        {
            sendDhcpReleasePacket();
        }
    }
    else if (strcmp(data->command, "set") == 0)
    {
        if (strcmp(data->strParam, "ip") == 0)
        {
            etherSetIpAddress(data->ip[0], data->ip[1], data->ip[2],
                              data->ip[3]);
            //ipconv.ip = data->ip;
            //writeEeprom(IP, ipconv.ip32_t);
        }
        else if (strcmp(data->strParam, "gw") == 0)
        {
            etherSetIpGatewayAddress(data->ip[0], data->ip[1], data->ip[2],
                                     data->ip[3]);
            //ipconv.ip = data->ip;
            //writeEeprom(GW, ipconv.ip32_t);
        }
        else if (strcmp(data->strParam, "dns") == 0)
        {
            etherSetDns(data->ip[0], data->ip[1], data->ip[2], data->ip[3]);
            //ipconv.ip = data->ip;
            //writeEeprom(DNS, ipconv.ip32_t);
        }
        else if (strcmp(data->strParam, "sn") == 0)
        {
            etherSetIpSubnetMask(data->ip[0], data->ip[1], data->ip[2],
                                 data->ip[3]);
            //ipconv.ip = data->ip;
            //writeEeprom(SN, ipconv.ip32_t);
        }
        else if (strcmp(data->strParam, "mqtt") == 0)
        {
            setMqttBrokerIp(data->ip[0], data->ip[1], data->ip[2],
                                 data->ip[3]);
            ipconv.ip[0] = data->ip[0];ipconv.ip[1] = data->ip[1];ipconv.ip[2] = data->ip[2];ipconv.ip[3] = data->ip[3];
            //writeEeprom(EEPROM_MQTT_BROKER, ipconv.ip32_t);
        }
    }
    else if (strcmp(data->command, "ifconfig") == 0)
    {
        displayConnectionInfo();
    }
    else if (strcmp(data->command, "reboot") == 0)
    {
        //to-do
    }
    else if(strcmp(data->command, "connect") == 0)
    {
        uint8_t serverIP[IP_ADD_LENGTH];
        mqttGetIpAddress(serverIP);
        //     uint8_t serverIP[] = {192, 168, 1 ,199};
        //     uint8_t serverMac[] = {0x00, 0xe0,0x4c,0x68, 0x19, 0xf4};
            uint8_t serverMac[] = {0x60, 0x45,0xbd,0xfa, 0xf6, 0x2b};
            uint16_t destPort = 1883;
            uint8_t qos = 0;
        if(serverIP[0] == 0 && serverIP[1] == 0 && serverIP[2] == 0 && serverIP[3] == 0)
        {
            serverIP[0] = 192; serverIP[1] = 168; serverIP[2] = 1; serverIP[3] = 199;//{192, 168, 1 ,199};

        }
        mqttConnect(serverIP, serverMac, qos);
    }
    else if (strcmp(data->command, "disconnect") == 0)
    {
        mqttDisconnect();
    }
    else if(strcmp(data->command, "help") == 0)
    {
        if(strcmp(data->strParam, "inputs") == 0)
        {
            printInputList("supported inputs: ");
        }
        else if(strcmp(data->strParam, "outputs") == 0)
        {
            printOutputList("supported outputs: ");
        }
        else if(strcmp(data->strParam, "subs") == 0)
        {
            printSubscribedTopics();
        }
        else{
            printInputList("supported inputs: ");
            printOutputList("supported outputs: ");
            printSubscribedTopics();
        }
    }
    else if(strcmp(data->command, "subscribe") == 0)
    {
        mqttSubscribe(data->topic, strlen(data->topic));
    }
    else if(strcmp(data->command, "unsubscribe") == 0)
    {
        mqttUnsubscribe(data->topic, strlen(data->topic));
    }
    else if(strcmp(data->command, "publish") == 0)
    {
        mqttPublish(data->topic, strlen(data->topic), data->topicvalue, strlen(data->topicvalue));
    }
}

void printSubscribedTopics()
{
    mqttSubscribedTopic topics[] = getMqttSubscribedTopics();
    uint8_t i = 0;
    putsUart0("Subscribed Topics: ");
    putcUart0('\n');
    for(i =0; i < MAX_SUBSCRIBED_TOPIC;i++)
    {
        if(topics[i].topicId == 0xff)
        {
            putsUart0(topics[i].topicName);
            putcUart0(', ');
        }
    }
    putcUart0('\n');
}




















