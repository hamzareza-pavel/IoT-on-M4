## Lightweight OS-less Network support library for ARM cortex M4-based microcontroller

# Description
The aim of the project is to provide ethernet communication support for a M4-based microcontroller. The framework supports DHCP, TCP/IP, and MQTT protocols. A Texas Instrument TM4C123GH6PM microcontroller is connected to an ENC28J60 ethernet interface via the SPI interface which is used by the microcontroller to read ethernet packets. The microcontroller and ethernet interface can be used as an IoT node for various use cases such as reading sensor data, automating home devices, etc. 

# Disclaimer
The code and framework to read packets from the ethernet interface were given by the course instructor [Dr. Jason Losh](https://ranger.uta.edu/~jlosh/). I have implemented the DHCP, TCP/IP, and MQTT protocols. If you are a student at The University of Texas at Arlington, please take prior permission from Dr.Jason Losh and the author of this repository before using any part of the source code in your project.
