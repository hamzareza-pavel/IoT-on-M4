#ifndef EEPROM_H
#define EEPROM_H

#include <stdint.h>

#define DHCPENABLED 0x0
#define IP 0x01
#define GW 0x02
#define SN 0x03
#define DNS 0x04
#define EEPROM_MQTT_BROKER 0x04

void initEeprom();
void writeEeprom(uint16_t add, uint32_t data);
uint32_t readEeprom(uint16_t add);

#endif
