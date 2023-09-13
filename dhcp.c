#include "dhcp.h"
#include "common.h"
#include "eth0.h"
#include "timer.h"

#define DHCP_MAGIC_COOKIE   0x63825363
#define MAX_OPTIONS_SIZE 312 // from rfc 2132

#define DHCP_CLIENT_PORT 68
#define DHCP_SERVER_PORT 67

// used DHCP options
#define DHCP_SUBNET_MASK 1
#define DHCP_ROUTER 3
#define DHCP_DOMAIN_NAME_SERVER 6
#define DHCP_HOST_NAME 12
#define DHCP_DOMAIN_NAME 15
#define DHCP_REQUESTED_IP 50
#define DHCP_LEASE_TIME 51
#define DHCP_MESSAGETYPE 53
#define DHCP_SERVER_IDENTIFIER 54
#define DHCP_PARAMETER_REQUESTLIST 55
#define DHCP_CLIENT_IDENTIFIER 61
#define DHCP_END 255

// DHCP Message types. we won't be using DHCPINFORM and LEASEQUERY
#define DHCPDISCOVER 1
#define DHCPOFFER 2
#define DHCPREQUEST 3
#define DHCPDECLINE 4
#define DHCPACK 5
#define DHCPNAK 6
#define DHCPRELEASE 7

#define TRANSACTION_ID 0x21274a1d // random transaction id to check validity of sent packet from server

typedef struct _dhcpFrame
{
    uint8_t op;
    uint8_t htype;
    uint8_t hlen;
    uint8_t hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint8_t ciaddr[4];
    uint8_t yiaddr[4];
    uint8_t siaddr[4];
    uint8_t giaddr[4];
    uint8_t chaddr[16];
    uint8_t data[192];
    uint32_t magicCookie;
    uint8_t options[0];
} dhcpFrame;

typedef struct _dhcpClientState
{
    uint8_t state;
    uint8_t serverMacAddress[HW_ADD_LENGTH];
    uint8_t serverIpAddress[IP_ADD_LENGTH];
    uint32_t leaseTime;
    uint32_t rebindTime;
    uint32_t renewTime;
    bool ipValidated;
} dhcpClientState;

dhcpClientState dhcpState = { .state = INIT, .serverMacAddress = { 0xFF, 0xFF,
                                                                   0xFF, 0xFF,
                                                                   0xFF, 0xFF },
                              .serverIpAddress = { 255, 255, 255, 255 },
                              .leaseTime = 0, .ipValidated = false };

typedef struct _dhcpOfferData
{
    uint32_t leaseTime;
    uint8_t offeredIp[IP_ADD_LENGTH];
    uint8_t serverIdentifier[IP_ADD_LENGTH];
    uint8_t serverHWAddress[HW_ADD_LENGTH];
    uint8_t subnet[IP_ADD_LENGTH];
    uint8_t router[IP_ADD_LENGTH];
    uint8_t dns[IP_ADD_LENGTH];
    bool ipInUse
} dhcpOfferData;

dhcpOfferData offerData = { .leaseTime = 0, .offeredIp = { 0, 0, 0, 0 },
                            .serverIdentifier = { 0, 0, 0, 0 },
                            .serverHWAddress = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                                 0xFF },
                            .subnet = { 0, 0, 0, 0 }, .router = { 0, 0, 0, 0 },
                            .dns = { 0, 0, 0, 0 }, .ipInUse = false };

typedef struct _dhcpRetryCounter
{
    uint8_t discoveryMessageCounter;
    uint8_t dhcpArpProbeCounter;
} dhcpRetryCounter;

dhcpRetryCounter dhcpCounters = { .discoveryMessageCounter = 0,
                                  .dhcpArpProbeCounter = 0 };

void resetDhcpState()
{
    dhcpState.state = INIT;
    uint8_t i = 0;
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        dhcpState.serverMacAddress[i] = 0xFF;
    }
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        dhcpState.serverIpAddress[i] = 255;
    }
    dhcpState.leaseTime = 0;
    dhcpState.ipValidated = false;
}

uint8_t appendDhcpOption(uint8_t *packet, uint8_t code, uint8_t *data,
                         uint8_t len)
{
    packet[0] = code;
    packet[1] = len;
    memcpy(&packet[2], data, len);
    return len + (sizeof(uint8_t) * 2);
}

void processReceivedDhcpPacket(uint8_t packet[], uint8_t* dhcpMessageType)
{
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    udpFrame* udp = (udpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    dhcpFrame* dhcp = (dhcpFrame*) &udp->data;

    uint8_t optionType = 0, optionLength = 0;
    uint8_t dhcpMsgType = 0;
    uint32_t leasetime = 0;
    //parse the options to figure out the received message type and associated option values
    uint16_t i, j;
    for (i = 0; i < MAX_OPTIONS_SIZE;)
    {

        // break out of loop in case of option end value of 255 or padding at the end of options
        if (dhcp->options[i] == DHCP_END || dhcp->options[i] == 0)
            break;

        optionType = dhcp->options[i++];

        optionLength = dhcp->options[i++];

        if (optionType == DHCP_MESSAGETYPE)
        {
            memcpy(&dhcpMsgType, &dhcp->options[i], sizeof(dhcpMsgType));
        }
        if (optionType == DHCP_SERVER_IDENTIFIER)
        {
            memcpy(&offerData.serverIdentifier, &dhcp->options[i],
            IP_ADD_LENGTH);
        }
        if (optionType == DHCP_LEASE_TIME)
        {
            memcpy(&leasetime, &dhcp->options[i], sizeof(leasetime));
            leasetime = ntohl(leasetime);
            offerData.leaseTime = leasetime;
        }
        if (optionType == DHCP_SUBNET_MASK)
        {
            memcpy(&offerData.subnet, &dhcp->options[i],
            IP_ADD_LENGTH);
        }
        if (optionType == DHCP_ROUTER)
        {
            memcpy(&offerData.router, &dhcp->options[i],
            IP_ADD_LENGTH);
        }
        if (optionType == DHCP_DOMAIN_NAME_SERVER)
        {
            memcpy(&offerData.dns, &dhcp->options[i],
            IP_ADD_LENGTH);
        }
        i += optionLength;
    }
    memcpy(&offerData.offeredIp, &dhcp->yiaddr, IP_ADD_LENGTH);
    memcpy(&offerData.serverHWAddress, &ether->sourceAddress, HW_ADD_LENGTH);
    *dhcpMessageType = dhcpMsgType;
    offerData.ipInUse = false;
}

void sendDhcpMessage(uint8_t packet[], uint8_t dhcpMessageType)
{
    uint8_t i;
    uint8_t tmp8;
    uint16_t tmp16;
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    ip->revSize = 0x45;
    udpFrame* udp = (udpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    dhcpFrame* dhcp = (dhcpFrame*) &udp->data;

    //everything dhcp
    memset(dhcp, 0, sizeof(dhcpFrame));
    dhcp->op = 1;
    dhcp->htype = 1;
    dhcp->hlen = 6;
    dhcp->hops = 0;
    dhcp->secs = htons(0);
    dhcp->flags = htons(0x00); // unicast
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        dhcp->ciaddr[i] = 0;
        dhcp->yiaddr[i] = 0;
        dhcp->siaddr[i] = 0;
        dhcp->giaddr[i] = 0;
    }
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        dhcp->chaddr[i] = macAddress[i];
    }

    if (dhcpState.state == BOUND || dhcpState.state == RENEWING
            || dhcpState.state == REBINDING || dhcpMessageType == DHCPRELEASE)
    {
        for (i = 0; i < IP_ADD_LENGTH; i++)
        {
            dhcp->ciaddr[i] = ipAddress[i];
        }
    }

    memset(dhcp->data, 0, 192);
    dhcp->xid = htonl(TRANSACTION_ID);
    dhcp->magicCookie = htonl(DHCP_MAGIC_COOKIE);

    uint16_t len = 0;
    uint16_t dhcpdatasize = 0;
    uint8_t req_ip[] = { 192, 168, 0, 131 };
    uint8_t parameter_req_list[] = { DHCP_SUBNET_MASK, DHCP_ROUTER,
    DHCP_DOMAIN_NAME_SERVER,
                                     DHCP_DOMAIN_NAME };
    uint8_t option;
    uint8_t tmpClientidentifier[HW_ADD_LENGTH + 1] = { 01, 2, 3, 4, 5, 6, 131 };
    uint8_t tmpHostIdentifier[8] = "IoT Node";

    option = dhcpMessageType;

    switch (dhcpMessageType)
    {
    case DHCPDISCOVER:

        len += appendDhcpOption(&dhcp->options[len], DHCP_MESSAGETYPE, &option,
                                sizeof(option));
        len += appendDhcpOption(&dhcp->options[len], DHCP_CLIENT_IDENTIFIER,
                                (uint8_t *) &tmpClientidentifier,
                                sizeof(tmpClientidentifier));
        len += appendDhcpOption(&dhcp->options[len], DHCP_REQUESTED_IP,
                                (uint8_t *) &req_ip, sizeof(req_ip));
        len += appendDhcpOption(&dhcp->options[len], DHCP_PARAMETER_REQUESTLIST,
                                (uint8_t *) &parameter_req_list,
                                sizeof(parameter_req_list));
        option = 0;
        len += appendDhcpOption(&dhcp->options[len], DHCP_END, &option,
                                sizeof(option));
        break;

    case DHCPREQUEST:
        len += appendDhcpOption(&dhcp->options[len], DHCP_MESSAGETYPE, &option,
                                sizeof(option));
        len += appendDhcpOption(&dhcp->options[len], DHCP_CLIENT_IDENTIFIER,
                                (uint8_t *) &tmpClientidentifier,
                                sizeof(tmpClientidentifier));
        len += appendDhcpOption(&dhcp->options[len], DHCP_REQUESTED_IP,
                                (uint8_t *) &offerData.offeredIp,
                                IP_ADD_LENGTH);
        if (dhcpState.state != BOUND || dhcpState.state != RENEWING
                || dhcpState.state != REBINDING)
        {
            len += appendDhcpOption(&dhcp->options[len], DHCP_SERVER_IDENTIFIER,
                                    (uint8_t *) &offerData.serverIdentifier,
                                    IP_ADD_LENGTH);
        }
        len += appendDhcpOption(&dhcp->options[len], DHCP_HOST_NAME,
                                (uint8_t *) &tmpHostIdentifier,
                                sizeof(tmpHostIdentifier));
        len += appendDhcpOption(&dhcp->options[len], DHCP_PARAMETER_REQUESTLIST,
                                (uint8_t *) &parameter_req_list,
                                sizeof(parameter_req_list));
        option = 0;
        len += appendDhcpOption(&dhcp->options[len], DHCP_END, &option,
                                sizeof(option));
        break;
    case DHCPRELEASE:
        len += appendDhcpOption(&dhcp->options[len], DHCP_MESSAGETYPE, &option,
                                sizeof(option));
        uint8_t tmpserver[] = { 192, 168, 0, 199 };
        len += appendDhcpOption(&dhcp->options[len], DHCP_SERVER_IDENTIFIER,
                                (uint8_t *) &offerData.serverIdentifier,
                                IP_ADD_LENGTH);
        len += appendDhcpOption(&dhcp->options[len], DHCP_CLIENT_IDENTIFIER,
                                (uint8_t *) &tmpClientidentifier,
                                sizeof(tmpClientidentifier));
        option = 0;
        len += appendDhcpOption(&dhcp->options[len], DHCP_END, &option,
                                sizeof(option));
        break;
    case DHCPDECLINE:
        len += appendDhcpOption(&dhcp->options[len], DHCP_REQUESTED_IP,
                                (uint8_t *) &req_ip, sizeof(req_ip));
        len += appendDhcpOption(&dhcp->options[len], DHCP_SERVER_IDENTIFIER,
                                (uint8_t *) &offerData.serverIdentifier,
                                IP_ADD_LENGTH);
        option = 0;
        len += appendDhcpOption(&dhcp->options[len], DHCP_END, &option,
                                sizeof(option));
        break;

    default:

        option = 0;
        len += appendDhcpOption(&dhcp->options[len], DHCP_END, &option,
                                sizeof(option));
        break;

    }

    dhcpdatasize = sizeof(dhcpFrame) + len - 2;

    //everything udp
    uint16_t udpdatasize = dhcpdatasize + 8;
    udp->sourcePort = htons(DHCP_CLIENT_PORT);
    udp->destPort = htons(DHCP_SERVER_PORT);
    udp->length = htons(udpdatasize);

    ip->flagsAndOffset = htons(0);
    ip->length = htons(20);
    ip->protocol = 17;
    ip->typeOfService = 0x00;
    ip->ttl = 128;
    memcpy(&ether->sourceAddress, &macAddress, HW_ADD_LENGTH);
    memcpy(&ip->sourceIp, &ipAddress, IP_ADD_LENGTH);

    memcpy(&ether->destAddress, &dhcpState.serverMacAddress, HW_ADD_LENGTH);
    memcpy(&ip->destIp, &dhcpState.serverIpAddress, IP_ADD_LENGTH);

    ether->frameType = htons(0x0800);
    ip->length = htons(((ip->revSize & 0xF) * 4) + udpdatasize);

    // 32-bit sum over ip header
    sum = 0;
    etherSumWords(&ip->revSize, 10);
    etherSumWords(ip->sourceIp, ((ip->revSize & 0xF) * 4) - 12);
    ip->headerChecksum = getEtherChecksum();

    sum = 0;
    etherSumWords(ip->sourceIp, 8);
    tmp16 = ip->protocol;
    sum += (tmp16 & 0xff) << 8;
    etherSumWords(&udp->length, 2);
    // add udp header except crc
    etherSumWords(udp, 6);
    etherSumWords(&udp->data, dhcpdatasize);
    udp->check = getEtherChecksum();
    // send packet with size = ether + udp hdr + ip header + dhcp header + udp_size
    etherPutPacket(ether, 14 + ((ip->revSize & 0xF) * 4) + udpdatasize);
}

void sendDhcpDiscoveryPacket()
{
    uint8_t data[MAX_PACKET_SIZE];
    sendDhcpMessage(data, DHCPDISCOVER);
}

void sendDhcpReleasePacket()
{
    uint8_t data[MAX_PACKET_SIZE];
    sendDhcpMessage(data, DHCPRELEASE);
}

void sendDhcpRequestPacket()
{
    uint8_t data[MAX_PACKET_SIZE];
    sendDhcpMessage(data, DHCPREQUEST);
}

void sendDhcpDeclinePacket()
{
    uint8_t data[MAX_PACKET_SIZE];
    sendDhcpMessage(data, DHCPDECLINE);
}

bool etherIsDhcp(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    udpFrame* udp = (udpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    dhcpFrame* dhcp = (dhcpFrame*) &udp->data;
    bool ok = false;
    uint16_t tmp16;

    if ((ip->protocol == 17) && (ntohs(udp->sourcePort) == DHCP_SERVER_PORT)
            && (ntohl(dhcp->xid) == TRANSACTION_ID))
    {
        ok = true;
    }

    return ok;
}

void dhcpStateMachineReceivedPacketHandler(uint8_t packet[])
{
    uint8_t rcvdMsgType = 0;
    processReceivedDhcpPacket(packet, &rcvdMsgType);

    if (rcvdMsgType == DHCPOFFER && dhcpState.state == INIT)
    {
        stopTimer(sendDhcpDiscoveryPacket);
        transitionToState(SELECTING);
        transitionToState(REQUESTING);
    }
    if (rcvdMsgType == DHCPACK
            && (dhcpState.state == REQUESTING || dhcpState.state == REBINDING
                    || dhcpState.state == RENEWING))
    {
        stopTimer(sendDhcpRequestPacket);
        transitionToState(IPVALIDATING);
    }
}

void transitionToState(uint8_t newState)
{
    uint8_t i;
    switch (newState)
    {
    case INIT:
        resetDhcpState();
        startPeriodicTimer(sendDhcpDiscoveryPacket, 20);
        break;
    case SELECTING:
        stopTimer(sendDhcpDiscoveryPacket);
        break;
    case REQUESTING:
        startPeriodicTimer(sendDhcpRequestPacket, 20);
        break;
    case BOUND:

        etherSetIpAddress(offerData.offeredIp[0], offerData.offeredIp[1],
                          offerData.offeredIp[2], offerData.offeredIp[3]);
        etherSetIpSubnetMask(offerData.subnet[0], offerData.subnet[1],
                             offerData.subnet[2], offerData.subnet[3]);
        etherSetIpGatewayAddress(offerData.router[0], offerData.router[1],
                                 offerData.router[2], offerData.router[3]);
        etherSetDns(offerData.dns[0], offerData.dns[0], offerData.dns[0], offerData.dns[0]);

        startStateTransitionTimers();
        break;
    case RENEWING:
        for (i = 0; i < HW_ADD_LENGTH; i++)
        {
            dhcpState.serverMacAddress[i] = offerData.serverHWAddress[i];
        }
        for (i = 0; i < IP_ADD_LENGTH; i++)
        {
            dhcpState.serverIpAddress[i] = offerData.serverIdentifier[i];
        }

        startPeriodicTimer(sendDhcpRequestPacket, 20);
        break;
    case REBINDING:
        for (i = 0; i < HW_ADD_LENGTH; i++)
        {
            dhcpState.serverMacAddress[i] = 0xFF;
        }
        for (i = 0; i < IP_ADD_LENGTH; i++)
        {
            dhcpState.serverIpAddress[i] = 255;
        }
        startPeriodicTimer(sendDhcpRequestPacket, 20);
        break;
    case IPVALIDATING:
        startPeriodicTimer(sendArpProbe, 5);
        startOneshotTimer(arpProbeFinished, 15);
        break;
    default:
        break;
    }
    dhcpState.state = newState;
}

void sendArpProbe()
{
    uint8_t data[MAX_PACKET_SIZE];
    //etherSendArpRequest(data, offerData.offeredIp, true);
}

void sendArpAnnouncement()
{
    uint8_t data[MAX_PACKET_SIZE];
   // etherSendArpRequest(data, ipAddress, false);
}

void arpProbeFinished()
{
    stopTimer(sendArpProbe);
    if (offerData.ipInUse == true) // offered IP is in use. send a decline message to server, switch to requesting state and start sending request message again.
    {
        sendDhcpDeclinePacket();
        transitionToState(REQUESTING);
    }
    else
    {
        transitionToState(BOUND);
        // start using offered IP
        sendArpAnnouncement();
    }
}

void renewingT1Timer()
{
    transitionToState(RENEWING);
}

void rebindingT2Timer()
{
    transitionToState(REBINDING);
}

void startStateTransitionTimers()
{
    stopTimer(renewingT1Timer);
    stopTimer(rebindingT2Timer);
    stopTimer(leaseExpired);
    startOneshotTimer(renewingT1Timer, offerData.leaseTime / 2);
    startOneshotTimer(rebindingT2Timer, (offerData.leaseTime * 7) / 8);
    startOneshotTimer(leaseExpired, offerData.leaseTime);
}

void leaseExpired()
{
    transitionToState(INIT);
}

void etherArpReceivedRequestIp(uint8_t requestIp[])
{
    bool sameIp = true;
    uint8_t i = 0;
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        if (requestIp[i] != offerData.offeredIp[i])
        {
            sameIp = false;
            break;
        }
    }
    offerData.ipInUse = sameIp;
}

void turnOffDhcpTimers()
{
    stopTimer(sendDhcpDiscoveryPacket);
    stopTimer(sendDhcpRequestPacket);
    stopTimer(sendArpProbe);
    stopTimer(arpProbeFinished);
    stopTimer(renewingT1Timer);
    stopTimer(rebindingT2Timer);
    stopTimer(leaseExpired);
}
