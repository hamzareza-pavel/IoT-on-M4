#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "tm4c123gh6pm.h"
#include "gpio.h"
#include "spi0.h"
#include "wait.h"
#include "timer.h"
#include "tcp.h"

tcpServerState tcpState = { .state = LISTEN, .runningSeqn = 0, .myPort = 5771,
                            .serverPort = 0, .ackToSend = 0, };

void resetTcpStateTimer()
{
    tcpState.runningSeqn = 0;
    tcpState.myPort = random32();
    tcpState.serverPort = 1883;
    tcpState.ackToSend = 0;
    tcpState.state = LISTEN;
}

bool etherIsTcp(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    bool ok = false;
    uint16_t tmp16;

    if ((ip->protocol == 0x06)
            && (ntohs(tcp->destPort) == HTTP_PORT
                    || ntohs(tcp->destPort) == TELNET_PORT) || ntohs(tcp->sourcePort) == 1883)
    {
        ok = true;
    }

    return ok;
}

void processTcpMessage(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    bool bEtherIsMqtt = false;

    switch (tcpState.state)
    {
    case LISTEN:
        resetTcpStateTimer();
        if ((tcp->flags & SYN) > 0)
        {
            etherSendTcpResponse(packet, 0, 0, (SYN | ACK));
            tcpState.state = ESTABLISHED;
            //startOneshotTimer(resetTcpStateTimer, 120);
        }
        if ((tcp->flags & SYN) > 0 && (tcp->flags & ACK) > 0)
        {
            etherSendTcpResponse(packet, 0, 0, (ACK));
            tcpState.state = ESTABLISHED;
           // startOneshotTimer(resetTcpStateTimer, 120);
        }
        break;
    case SYN_SENT:
        if ((tcp->flags & SYN) > 0 && (tcp->flags & ACK) > 0)
        {
            etherSendTcpResponse(packet, 0, 0, (ACK));
            tcpState.state = ESTABLISHED;
           // startOneshotTimer(resetTcpStateTimer, 120);
        }
        break;
    case ESTABLISHED:
        if ((tcp->flags & ACK) > 0)
        {
            //    etherSendTcpPacket(packet, 0, 0, ACK|FIN);
            //    tcpState.state = FIN_WAIT_1;
            //    startOneshotTimer(resetTcpStateTimer, 15);
        }
        if ((tcp->flags & FIN) > 0 && (tcp->flags & ACK) > 0)
        {
            tcpState.state = LISTEN;
            etherSendTcpResponse(packet, 0, 0,  ACK);
        }
        if ((tcp->flags & FIN) > 0)
        {
            tcpState.state = LISTEN;
            etherSendTcpResponse(packet, 0, 0, FIN | ACK);
        }
        if ((tcp->flags & PUSH) > 0)
        {
            //see if the packet contains mqtt payload, if it does, process the reply
            if(etherIsMqtt(packet))
            {
                uint32_t receivedTcpSize = ntohs(ip->length) - 20; //deduct the ipframe size
                uint32_t mqttPayloadSize = receivedTcpSize - tcp->off * 4;
                if(mqttPayloadSize > 0)
                {
                    bEtherIsMqtt= true;
                }
            }
            etherSendTcpResponse(packet, 0, 0, ACK);
        }
        break;
    default:
        break;
    }

    if(bEtherIsMqtt == true)
    {
        processMqttMessage(packet);
    }

    return;
}

void etherSendTcpResponse(uint8_t packet[], uint8_t* tcpData,
                          uint8_t tcpDataSize, uint8_t flags)
{
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    uint8_t *copyData;
    uint8_t i, tmp8;
    uint16_t tmp16;
    uint32_t receivedTcpSize = ntohs(ip->length) - 20; //deduct the ipframe size
    uint32_t receivedPayloadSize = receivedTcpSize - tcp->off * 4;

    // swap source and destination fields
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        tmp8 = ether->destAddress[i];
        ether->destAddress[i] = ether->sourceAddress[i];
        ether->sourceAddress[i] = tmp8;
    }
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        tmp8 = ip->destIp[i];
        ip->destIp[i] = ip->sourceIp[i];
        ip->sourceIp[i] = tmp8;
    }

    tmp16 = tcp->sourcePort;
    tcp->sourcePort = tcp->destPort;
    tcp->destPort = tmp16;
    tcp->reservedNS = 0;
    if ((tcp->flags & PUSH) > 0)
    {
        tcpState.ackToSend = ntohl(tcp->sequenceNumber) + receivedPayloadSize;
    }
    else if ((tcp->flags & SYN) > 0)
    {
        tcpState.ackToSend = ntohl(tcp->sequenceNumber) + 1;
    }
    else if ((tcp->flags & FIN) > 0)
    {
        tcpState.ackToSend = ntohl(tcp->sequenceNumber) + 1;
    }

    if ((flags & PUSH) > 0)
    {
        tcp->sequenceNumber = htonl(tcpState.runningSeqn);
        tcpState.runningSeqn =  tcpState.runningSeqn + tcpDataSize;
    }
    else if ((flags & SYN) > 0)
    {
        tcp->sequenceNumber = htonl(tcpState.runningSeqn++);
    }
    else if ((flags & FIN) > 0)
    {
        tcp->sequenceNumber = htonl(tcpState.runningSeqn++);
    }
    else
    {
        tcp->sequenceNumber = htonl(tcpState.runningSeqn);
    }

     if((flags & ACK) > 0)
     {
         tcp->ackNumber = htonl(tcpState.ackToSend);
     }
     else
     {
         tcp->ackNumber = htonl(tcpState.ackToSend);
     }

    tcp->off = 0x5;
    tcp->flags = flags;
    tcp->win = htons(1280); // 1 MSS
    tcp->sum = 0;
    tcp->urp = 0;

    uint8_t tcpHederSize = 20;
    // adjust lengths
    ip->length = htons(((ip->revSize & 0xF) * 4) + tcpHederSize + tcpDataSize);
    // 32-bit sum over ip header
    sum = 0;
    etherSumWords(&ip->revSize, 10);
    etherSumWords(ip->sourceIp, ((ip->revSize & 0xF) * 4) - 12);
    ip->headerChecksum = getEtherChecksum();

    // copy data
    if (tcpDataSize > 0)
    {
        copyData = &tcp->data;
        for (i = 0; i < tcpDataSize; i++)
            copyData[i] = tcpData[i];
    }
    // 32-bit sum over pseudo-header
    sum = 0;
    etherSumWords(ip->sourceIp, 8);
    tmp16 = ip->protocol;
    sum += (tmp16 & 0xff) << 8;
    uint16_t tcpLength = htons(tcpHederSize + tcpDataSize);
    etherSumWords(&tcpLength, 2);
    // add tcp header except checksum
    etherSumWords(tcp, tcpHederSize);
    if (tcpDataSize > 0)
    {
        etherSumWords(&tcp->data, tcpDataSize);
    }
    tcp->sum = getEtherChecksum();

    etherPutPacket(ether,
                   14 + ((ip->revSize & 0xF) * 4) + tcpHederSize + tcpDataSize);
}

void sendTcpPacket(uint8_t* tcpData, uint8_t tcpDataSize, uint8_t flags,
                   uint8_t* serverMac, uint8_t* serverIP, uint16_t destPort)
{
    uint8_t packet[MAX_PACKET_SIZE];
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    ip->revSize = 0x45;
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    uint8_t *copyData;
    uint16_t tmp16;


    tcp->off = 0x5;
    tcp->sourcePort = htons(tcpState.myPort);
    tcp->destPort = htons(destPort);
    tcp->reservedNS = 0;

    if ((flags & PUSH) > 0)
    {
        tcp->sequenceNumber = htonl(tcpState.runningSeqn);
        tcpState.runningSeqn =  tcpState.runningSeqn + tcpDataSize;
    }
    else if ((flags & SYN) > 0)
    {
        tcp->sequenceNumber = htonl(tcpState.runningSeqn++);
    }
    else if ((flags & FIN) > 0)
    {
        tcp->sequenceNumber = htonl(tcpState.runningSeqn++);
    }
    else
    {
        tcp->sequenceNumber = htonl(tcpState.runningSeqn);
    }



     if((flags & ACK) > 0)
     {
         tcp->ackNumber = htonl(tcpState.ackToSend);
     }
     else
     {
         tcp->ackNumber = htonl(tcpState.ackToSend);
     }

    tcp->flags = flags;
    tcp->win = htons(1280); // 1 MSS
    tcp->sum = 0;
    tcp->urp = 0;

    //populate ipframe and ether frame
    ip->flagsAndOffset = htons(0);
    ip->length = htons(20);
    ip->protocol = 6; //for tcp
    ip->typeOfService = 0x00;
    ip->ttl = 128;
    // swap source and destination fields
    uint8_t i = 0;
     for (i = 0; i < HW_ADD_LENGTH; i++)
     {
         ether->destAddress[i] = serverMac[i];
         ether->sourceAddress[i] = macAddress[i];
     }
     for (i = 0; i < IP_ADD_LENGTH; i++)
     {
         ip->destIp[i] = serverIP[i];
         ip->sourceIp[i] = ipAddress[i];
     }

    uint8_t tcpHederSize = 20;
    ether->frameType = htons(0x0800);

    // adjust lengths
    ip->length = htons(((ip->revSize & 0xF) * 4) + tcpHederSize + tcpDataSize);
    // 32-bit sum over ip header
    sum = 0;
    etherSumWords(&ip->revSize, 10);
    etherSumWords(ip->sourceIp, ((ip->revSize & 0xF) * 4) - 12);
    ip->headerChecksum = getEtherChecksum();

    // copy data
    if (tcpDataSize > 0)
    {
        copyData = &tcp->data;
        for (i = 0; i < tcpDataSize; i++)
            copyData[i] = tcpData[i];
    }
    // 32-bit sum over pseudo-header
    sum = 0;
    etherSumWords(ip->sourceIp, 8);
    tmp16 = ip->protocol;
    sum += (tmp16 & 0xff) << 8;
    uint16_t tcpLength = htons(tcpHederSize + tcpDataSize);
    etherSumWords(&tcpLength, 2);
    // add tcp header except checksum
    etherSumWords(tcp, tcpHederSize);
    if (tcpDataSize > 0)
    {
        etherSumWords(&tcp->data, tcpDataSize);
    }
    tcp->sum = getEtherChecksum();

    etherPutPacket(ether,
                   14 + ((ip->revSize & 0xF) * 4) + tcpHederSize + tcpDataSize);

}

uint8_t getTcpConnectionState()
{
    return tcpState.state;
}


void establishConnection(uint8_t* serverMac, uint8_t* serverIP, uint16_t destPort)
{
    resetTcpStateTimer();
    tcpState.state = SYN_SENT;
    tcpState.serverPort = destPort;
    sendTcpPacket(0, 0, SYN, serverMac, serverIP, destPort);

}
