#ifndef DHCP_H
#define DHCP_H

#include <stdint.h>
#include <stdbool.h>


//dhcp states. Ignoring INIT_REBOOT and REBOOTING

#define INIT 1
#define SELECTING 2
#define REQUESTING 3
#define BOUND 4
#define REBINDING 5
#define RENEWING 6
#define IPVALIDATING 7// this step is to send ARP probe and validate if the IP address is used by someone else

void dhcpSendRequestMessage(uint8_t packet[]);
uint8_t appendDhcpOption(uint8_t *packet, uint8_t code, uint8_t *data, uint8_t len);
void sendDhcpDiscoveryPacket();
void sendDhcpReleasePacket();
bool etherIsDhcp(uint8_t packet[]);
void dhcpStateMachineReceivedPacketHandler(uint8_t packet[]);
void etherArpReceivedRequestIp(uint8_t requestIp[]);
void arpProbeFinished();
void sendDhcpDeclinePacket();
void sendArpProbe();
void leaseExpired();
void startStateTransitionTimers();
void rebindingT2Timer();
void renewingT1Timer();
void turnOffDhcpTimers();

#endif
