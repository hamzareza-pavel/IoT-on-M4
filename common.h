#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdbool.h>

// Max packet is calculated as:
// Ether frame header (18) + Max MTU (1500) + CRC (4)
#define MAX_PACKET_SIZE 1522

#define HTTP_PORT 80
#define TELNET_PORT 23

#define ETHER_UNICAST        0x80
#define ETHER_BROADCAST      0x01
#define ETHER_MULTICAST      0x02
#define ETHER_HASHTABLE      0x04
#define ETHER_MAGICPACKET    0x08
#define ETHER_PATTERNMATCH   0x10
#define ETHER_CHECKCRC       0x20

#define ETHER_HALFDUPLEX     0x00
#define ETHER_FULLDUPLEX     0x100

#define LOBYTE(x) ((x) & 0xFF)
#define HIBYTE(x) (((x) >> 8) & 0xFF)

uint16_t htons(uint16_t value);
uint32_t htonl(uint32_t value);
#define ntohs htons
#define ntohl htonl

// Packets
#define IP_ADD_LENGTH 4
#define HW_ADD_LENGTH 6

typedef struct _etherFrame // 14-bytes
{
  uint8_t destAddress[6];
  uint8_t sourceAddress[6];
  uint16_t frameType;
  uint8_t data;
} etherFrame;

typedef struct _ipFrame // minimum 20 bytes
{
  uint8_t revSize;
  uint8_t typeOfService;
  uint16_t length;
  uint16_t id;
  uint16_t flagsAndOffset;
  uint8_t ttl;
  uint8_t protocol;
  uint16_t headerChecksum;
  uint8_t sourceIp[4];
  uint8_t destIp[4];
} ipFrame;

typedef struct _icmpFrame
{
  uint8_t type;
  uint8_t code;
  uint16_t check;
  uint16_t id;
  uint16_t seq_no;
  uint8_t data;
} icmpFrame;

typedef struct _arpFrame
{
  uint16_t hardwareType;
  uint16_t protocolType;
  uint8_t hardwareSize;
  uint8_t protocolSize;
  uint16_t op;
  uint8_t sourceAddress[6];
  uint8_t sourceIp[4];
  uint8_t destAddress[6];
  uint8_t destIp[4];
} arpFrame;

typedef struct _udpFrame // 8 bytes
{
  uint16_t sourcePort;
  uint16_t destPort;
  uint16_t length;
  uint16_t check;
  uint8_t  data;
} udpFrame;

extern uint32_t sum;
extern uint8_t macAddress[HW_ADD_LENGTH];
extern uint8_t ipAddress[IP_ADD_LENGTH];
extern uint8_t ipSubnetMask[IP_ADD_LENGTH];
extern uint8_t ipGwAddress[IP_ADD_LENGTH];
extern bool    dhcpEnabled;
union
{
    uint32_t ip32_t;
    uint8_t ip[4];
} ipconv;

#endif
