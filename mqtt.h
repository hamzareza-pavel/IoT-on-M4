#ifndef MQTT_H
#define MQTT_H
#include "common.h"
#include "stdint.h"

#define MAX_TOPIC_NAME_SIZE 30
#define MAX_SUBSCRIBED_TOPIC 10

typedef struct _fixedMqttHeader{
    uint8_t flags:4;
    uint8_t packetType:4;
    uint8_t msglen;
} fixedMqttHeader;

typedef struct _mqttFrameConnect
{
    uint16_t protocolLen;
    char protocolName[4];
    uint8_t version;
    uint8_t flag;
    uint16_t keepalive;
    uint16_t clen;
} mqttFrameConnect;

typedef struct _mqttFrameSubscribe{
    uint16_t packetIdentifier;
    uint16_t topicnamelen;
    //topic filter
    //qos
}mqttFrameSubscribe;

typedef struct _mqttFrameUnsubscribe{
    uint16_t packetIdentifier;
    uint16_t topicnamelen;
    //topic filter
}mqttFrameUnsubscribe;

typedef struct _storedTopicStruct{
    uint16_t topicId;
    char topicName[MAX_TOPIC_NAME_SIZE];
}mqttSubscribedTopic;

// mqtt control packet types
#define MQTT_CONNECT 1
#define MQTT_CONNACK 2
#define MQTT_PUBLISH 3
#define MQTT_PUBACK  4
#define MQTT_PUBREC  5
#define MQTT_PUBREL  6
#define MQTT_PUBCOMP 7
#define MQTT_SUBSCRIBE   8
#define MQTT_SUBACK      9
#define MQTT_UNSUBSCRIBE 10
#define MQTT_UNSUBACK    11
#define MQTT_PINGREQ     12
#define MQTT_PINGRESP    13
#define MQTT_DISCONNECT  14


//core mqtt packets
void mqttConnect(uint8_t* serverIP, uint8_t* serverMacAddress, uint8_t qos);
void mqttDisconnect();
void mqttPing();
void mqttSubscribe(char* topicFilter, uint16_t topicNameLen);
void mqttUnsubscribe(char* topicFilter, uint16_t topicNameLen);

uint16_t getNewGuid();
uint8_t appendToPayload(uint8_t *buffer, uint8_t *data, uint8_t len);
void retryMqttMsgResend();
bool etherIsMqtt(uint8_t packet[]);
void processMqttMessage(uint8_t packet[]);

void storeSubscribedTopic(topicFilter, topicId);
void removeUnsubscribedTopic(topicFilter, topicId);
uint16_t getTopicIdByName(topicFilter);
void setMqttBrokerIp(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3);
void mqttGetIpAddress(uint8_t ip[4]);

mqttSubscribedTopic* getMqttSubscribedTopics();


#endif
