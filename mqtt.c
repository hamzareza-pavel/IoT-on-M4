#include "mqtt.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "tm4c123gh6pm.h"
#include "gpio.h"
#include "spi0.h"
#include "wait.h"
#include "timer.h"
#include "tcp.h"


#define MQTT_BROKER_PORT 1883
#define MQTT_MAX_MSGSIZE 110

#define MQTT_CONNECTED 1
#define MQTT_DISCONNECTED 2

uint16_t guid = 1;
mqttSubscribedTopic subscribedTopics[MAX_SUBSCRIBED_TOPIC];

typedef struct _mqttClientState
{
    uint8_t qos;
    uint8_t brokerIP[IP_ADD_LENGTH];
    uint8_t brokerMac[HW_ADD_LENGTH];
    uint8_t connectionState;
} mqttClientState;
typedef struct _mqttMessageBuffer
{
    bool isEmpty;
    uint16_t msgLen;
    uint8_t buff[MQTT_MAX_MSGSIZE];
} mqttMessageBuffer;

mqttClientState clientState = { .qos = 0, .brokerIP = { 0, 0, 0, 0 },
                                .brokerMac = { 0xf, 0xff, 0xff, 0xff },
                                .connectionState = MQTT_DISCONNECTED };

mqttMessageBuffer msgBuff = {.isEmpty = true, .msgLen = 0};


uint8_t appendToPayload(uint8_t *buffer, uint8_t *data, uint8_t len)
{
    memcpy(&buffer[0], data, len);
    return len;
}

void mqttConnect(uint8_t* serverIP, uint8_t* serverMacAddress, uint8_t qos)
{
    memcpy(&clientState.brokerMac, serverMacAddress, HW_ADD_LENGTH);
    memcpy(&clientState.brokerIP, serverIP, IP_ADD_LENGTH);

    uint8_t i;
    for(i = 0; i < MAX_SUBSCRIBED_TOPIC; i++)
    {
        subscribedTopics[i].topicId = 0xff;
    }


    clientState.qos = qos;
    mqttFrameConnect mqtt ;
    fixedMqttHeader mqttfh;
    char* clientName = "hello";
    uint8_t clientSize = strlen(clientName);
    uint8_t sizeofclient = sizeof(clientName);
    mqttfh.packetType = MQTT_CONNECT;
    mqttfh.flags = 0;
    mqtt.protocolLen = htons(4);
    mqtt.protocolName[0] = 'M';
    mqtt.protocolName[1] = 'Q';
    mqtt.protocolName[2] = 'T';
    mqtt.protocolName[3] = 'T';
    mqtt.version = 0x04;
    mqtt.flag = 0x02;
    mqtt.keepalive = htons(60);
    mqtt.clen = htons(clientSize);//htons(strlen(clientName));

    uint8_t totalMsgSize = sizeof(mqttFrameConnect) + clientSize;
    mqttfh.msglen = totalMsgSize;

    //memset(msgBuff, 0, sizeof(mqttMessageBuffer));

    uint8_t len = 0;
    //add stuff to message buffer
    len += appendToPayload(&msgBuff.buff[len], &mqttfh, sizeof(mqttfh));
    len += appendToPayload(&msgBuff.buff[len], &mqtt, sizeof(mqtt));
    //adding string data. no pointer for str types in the header as it makes calculating the size of payload difficult
    len += appendToPayload(&msgBuff.buff[len], clientName, clientSize);
    msgBuff.msgLen = len;
    msgBuff.isEmpty = false;

    if(getTcpConnectionState() == ESTABLISHED)
    {
        sendMqttPayload();
    }
    else
    {
        establishConnection(clientState.brokerMac, clientState.brokerIP, MQTT_BROKER_PORT);
        startPeriodicTimer(retryMqttMsgResend, 15);
    }
}

void mqttDisconnect()
{
    fixedMqttHeader mqtt;
    mqtt.packetType = MQTT_DISCONNECT;
    mqtt.flags = 0;
    mqtt.msglen = 0;
    stopTimer(mqttPing);
    stopTimer(retryMqttMsgResend);
    clientState.connectionState = MQTT_DISCONNECTED;
    sendTcpPacket(&mqtt, sizeof(mqtt), ACK|PUSH, clientState.brokerMac, clientState.brokerIP, MQTT_BROKER_PORT);
}

void mqttPing()
{
    fixedMqttHeader mqtt;
    mqtt.packetType = MQTT_PINGREQ;
    mqtt.flags = 0;
    mqtt.msglen = 0;
    sendTcpPacket(&mqtt, sizeof(mqtt), ACK|PUSH, clientState.brokerMac, clientState.brokerIP, MQTT_BROKER_PORT);
}

void mqttSubscribe(char* topicFilter, uint16_t topicNameLen)
{
    fixedMqttHeader mqttfh;
    mqttfh.packetType = MQTT_SUBSCRIBE;
    mqttfh.flags = 0x02;
    mqttfh.msglen = 0;
    mqttFrameSubscribe subs;
    uint16_t topicId = getNewGuid();
    subs.packetIdentifier = htons(topicId);
    subs.topicnamelen = htons(topicNameLen);

    mqttfh.msglen = sizeof(mqttFrameSubscribe) + topicNameLen + 1; //1 for qos field

    uint8_t len = 0;
    //add stuff to message buffer
    len += appendToPayload(&msgBuff.buff[len], &mqttfh, sizeof(mqttfh));
    len += appendToPayload(&msgBuff.buff[len], &subs, sizeof(subs));
    //adding string data. no pointer for str types in the header as it makes calculating the size of payload difficult
    len += appendToPayload(&msgBuff.buff[len], topicFilter, topicNameLen);
    len += appendToPayload(&msgBuff.buff[len], &clientState.qos, 1);
    msgBuff.msgLen = len;
    msgBuff.isEmpty = false;

    sendMqttPayload();
    storeSubscribedTopic(topicFilter, topicId);

}

void mqttUnsubscribe(char* topicFilter, uint16_t topicNameLen)
{
    fixedMqttHeader mqttfh;
    mqttfh.packetType = MQTT_UNSUBSCRIBE;
    mqttfh.flags = 0x02;
    mqttfh.msglen = 0;
    mqttFrameUnsubscribe unsubs;
    uint16_t topicId = getTopicIdByName(topicFilter);

    unsubs.packetIdentifier = htons(topicId);
    unsubs.topicnamelen = htons(topicNameLen);

    mqttfh.msglen = sizeof(mqttFrameUnsubscribe) + topicNameLen; //1 for qos fiel

    uint8_t len = 0;
    //add stuff to message buffer
    len += appendToPayload(&msgBuff.buff[len], &mqttfh, sizeof(mqttfh));
    len += appendToPayload(&msgBuff.buff[len], &unsubs, sizeof(unsubs));
    //adding string data. no pointer for str types in the header as it makes calculating the size of payload difficult
    len += appendToPayload(&msgBuff.buff[len], topicFilter, topicNameLen);
    msgBuff.msgLen = len;
    msgBuff.isEmpty = false;

    removeUnsubscribedTopic(topicFilter, topicId);

    sendMqttPayload();
}

void mqttPublish(char* topicFilter, uint16_t topicNameLen, char* topicValue,
                 uint16_t topicValueLen)
{
    fixedMqttHeader mqttfh;
    mqttfh.packetType = MQTT_PUBLISH;
    if (clientState.qos == 0)
    {
        mqttfh.flags = 0;
    }
    else if (clientState.qos == 1)
    {
        mqttfh.flags = 0b0010;
    }
    else if (clientState.qos == 2)
    {
        mqttfh.flags = 0b0100;
    }

    if (clientState.qos == 0)
    {
        mqttfh.msglen = strlen(topicFilter) + strlen(topicValue) + 2; //topic name size+ payload size+ topic length size
    }
    else
    {
        mqttfh.msglen = strlen(topicFilter) + strlen(topicValue) + 2 + 2; //qos 0 size + packet identifier 2 byte
    }

    uint8_t len = 0;
    //add stuff to message buffer
    len += appendToPayload(&msgBuff.buff[len], &mqttfh, sizeof(mqttfh));
    uint16_t tmp16TopicNamelen = htons(topicNameLen);
    len += appendToPayload(&msgBuff.buff[len], &tmp16TopicNamelen,
                           sizeof(tmp16TopicNamelen));
    //adding string data. no pointer for str types in the header as it makes calculating the size of payload difficult
    len += appendToPayload(&msgBuff.buff[len], topicFilter, topicNameLen);
    if (clientState.qos == 1 || clientState.qos == 2)
    {
        uint16_t packetID = htons(getNewGuid());
        len += appendToPayload(&msgBuff.buff[len], &packetID, sizeof(packetID));
    }

    len += appendToPayload(&msgBuff.buff[len], topicValue, topicValueLen);

    msgBuff.msgLen = len;
    msgBuff.isEmpty = false;
    sendMqttPayload();

}

void retryMqttMsgResend()
{
    sendMqttPayload();
}

void sendMqttPayload()
{
    if(msgBuff.isEmpty == false)
    {
        //stopTimer(retryMqttMsgResend);
        sendTcpPacket(msgBuff.buff, msgBuff.msgLen, ACK|PUSH, clientState.brokerMac, clientState.brokerIP, MQTT_BROKER_PORT);
     //   memset(msgBuff, 0, sizeof(mqttMessageBuffer));
        msgBuff.isEmpty = true;
    }
}

void processMqttMessage(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    fixedMqttHeader* mqttFxHdr = (fixedMqttHeader*) (&tcp->data);

    switch (mqttFxHdr->packetType)
    {
    case MQTT_CONNACK:
        startPeriodicTimer(mqttPing, 50);
        clientState.connectionState = MQTT_CONNECTED;
        break;
    case MQTT_PINGRESP:
        flashBlue();
        break;
    default:
        break;
    }
    return;
}

bool etherIsMqtt(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    bool ok = false;
    uint16_t tmp16;

    if ((ip->protocol == 0x06) && (ntohs(tcp->sourcePort) == MQTT_BROKER_PORT))
    {
        ok = true;
    }

    return ok;
}

uint16_t getNewGuid()
{
    return guid++;
}

void storeSubscribedTopic(topicFilter, topicId)
{
    uint8_t i;
    for (i = 0; i < MAX_SUBSCRIBED_TOPIC; i++)
    {
        if (subscribedTopics[i].topicId == 0xff)
        {
            subscribedTopics[i].topicId = topicId;
            strcpy(subscribedTopics[i].topicName, topicFilter);
            break;
        }
    }
}
void removeUnsubscribedTopic(topicFilter, topicId)
{
    uint8_t i;
    for (i = 0; i < MAX_SUBSCRIBED_TOPIC; i++)
    {
        if (subscribedTopics[i].topicId == topicId)
        {
            subscribedTopics[i].topicId = 0xff;
            strcpy(subscribedTopics[i].topicName, "");
            break;
        }
    }
}
uint16_t getTopicIdByName(topicFilter)
{
    uint8_t i;
    for (i = 0; i < MAX_SUBSCRIBED_TOPIC; i++)
    {
        if (strcmp(subscribedTopics[i].topicName, topicFilter) == 0)
        {
            return subscribedTopics[i].topicId;
        }
    }
}

void setMqttBrokerIp(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3)
{
    clientState.brokerIP[0] = ip0;
    clientState.brokerIP[1] = ip1;
    clientState.brokerIP[2] = ip2;
    clientState.brokerIP[3] = ip3;
}

void mqttGetIpAddress(uint8_t ip[4])
{
    uint8_t i;
    for (i = 0; i < 4; i++)
        ip[i] = clientState.brokerIP[i];
}

mqttSubscribedTopic* getMqttSubscribedTopics()
{
    return subscribedTopics;
}














