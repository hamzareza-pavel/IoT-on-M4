#ifndef TCP_H
#define TCP_H

#include "common.h"

#define FIN 0x01
#define SYN 0x02
#define RST 0x04
#define PUSH 0x08
#define ACK 0x10
#define URG 0x20
#define ECE 0x40
#define CWR 0x80

//TCP states. ignoring CLOSED state as our server will always be in passive open state.
#define LISTEN 0
#define SYN_RECEIVED 1
#define ESTABLISHED 2
#define FIN_WAIT_1 3
#define FIN_WAIT_2 4
#define CLOSING 5
#define TIME_WAIT 6
#define SYN_SENT 7

typedef struct _tcpFrame // 8 bytes
{
  uint16_t sourcePort;
  uint16_t destPort;
  uint32_t sequenceNumber;
  uint32_t ackNumber;
  unsigned reservedNS:4; // reserved 3 bits and NS bit which is always 0
  unsigned off:4;
  uint8_t flags;
  uint16_t win;
  uint16_t sum;
  uint16_t urp;
  uint8_t data;
} tcpFrame;

typedef struct _tcpServerState
{
    uint8_t state;
    uint32_t runningSeqn;
    uint16_t myPort;
    uint16_t serverPort;
    uint32_t ackToSend;
} tcpServerState;



bool etherIsTcp(uint8_t packet[]);
void processTcpMessage(uint8_t packet[]);
void resetTcpStateTimer();
void sendTcpPacket(uint8_t* tcpData, uint8_t tcpDataSize, uint8_t flags, uint8_t* serverMac, uint8_t* serverIP, uint16_t destPort);
uint8_t getTcpConnectionState();
void establishConnection(uint8_t* serverMac, uint8_t* serverIP, uint16_t destPort);

#endif
