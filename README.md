# Opportunistic BLE Mesh (testbed scenarios)



## Introduction

This project allows to perform different test beds to evaluate the performance of an opportunistic communications system (named as ORBALLO) based on the BLE Mesh standard with the following modifications in the original protocol.

1. The friend node has been replaced by a distributed cache via libp2p. 

2. Is possible to use specific Bluetooth 5 modulations within the mesh communication. 

Why this changes?

1. Currently the BLE Mesh protocol has a feature that allows a node (friend) to store the data of a low power node that is inactive for a certain period of time (LPN) when this node wakes up receives the data from the cache of the friend node. 

    This approach is intended for static nodes (sensors or actuators in a building for example) and when an LPN is not in range with the friend node that has stored its cache, the cache is lost.

    In an opportunistic system the nodes will be in movement and therefore the cache must be distributed among the different nodes of the mesh, libp2p provides a solution to this problem. 

2. The BLE Mesh protocol uses Bluetooth 4.x standard so it does not officially support any Bluetooth 5 modulation in the transmission, however since the standard is open source it can be modified and with hardware that supports Bluetooth 5 it is possible to use these modulations to obtain certain benefits in the communication in specific scenarios, specifically Bluetooth 5 adds two major modulations, one for long range (Coded) and one for high bandwidth (2M).

When are these modulation changes relevant?

- Coded: for nodes located far away from each other or in harsh enviroments without intensive data transfer requirements

- 2M: for nodes in close range with high bandwidth demand (transmission of video or photos for instance)

When is it counterproductive to use these modulations?

- Coded: when there are many nodes in range or the transfer rates are high. It must be taken into account that for increasing the range the datarate is reduced to 125 kbps and the time on air of the frame increases by 8x and too much use of the channels would lead to a saturation of the communication. 

- 2M: when communications are unreliable or have high bit error rate, considering that for this modulation the sensitivity is reduced, also are not suitable for very small messages 

## Requirements

The hardware has been coded using the Nordic Semiconductors SDK. At least 3 boards with an nrf5x based SoC that can be coded using this SDK are required, if you want to test a specific Bluetooth 5 modulation (not mandatory) the best option are boards based on the nrf52840 or nrf52833 microprocessors.

Porting the libp2p code to an nrf5x based board is not a viable solution in the short term and since the mesh nodes working as relays need a continuous power supply, a Raspebrry Pi was used with one of the Bluetooth SoCs connected to the serial port, any ARM device with serial connection that can be programmed in golang is also valid.

In order to provisionate and configure nodes within the Mesh the best option is to use the nordic nRF Mesh [app](https://play.google.com/store/apps/details?id=no.nordicsemi.android.nrfmeshprovisioner&gl=US) 

- [Nordic SDK](https://www.nordicsemi.com/Products/Development-software/nrf5-sdk/download)
- [SDK for Mesh](https://www.nordicsemi.com/Products/Development-software/nRF5-SDK-for-Mesh)
- [libp2p](https://github.com/libp2p/libp2p)

## Installation

Divided in several stages, the first one consists in flashing the nordic sdk firmware needed in each board, in this case the light_switch example of the nordic mesh sdk was adapted, two nodes will work as client (LPN) and one as server, the server node will also work as relay and friend node of the two LPN, this node needs to have go-libp2p installed.

For the nordic firmware, the project includes the two nordic SDKs needed to compile the software and generate the firmware of the boards, it is necessary to have installed the SEGGER Embedded Studio IDE or a similar tool.

In the case of go-libp2p it is necessary to have the golang compiler at least version 1.16.

To provision the nodes inside the mesh it is necessary to use a smartphone with BLE and the nordic nRF Mesh app.

## Usage

Open in Segger IDE the projects light_switch client and server, build and run on the selected boards, then it is necessary to provision with the app all the nodes, for the client nodes once provisioned it is necessary to configure the "Generic On/Off Client" element binding an application key and defining a publishing address that can be the unicast address of the server node or a group address. 

In the same way the "Generic On/Off Server" server node element is configured, if the unicast address of the server is used in the client it is not necessary to configure a subscription address.

The behavior of these nodes is as follows: the client nodes publish a message in ASCII (payload) to the addresses to which they are subscribed through the serial port with a destination address, then this is redirected through the relay to the destination node. 

Example.

3 nodes: A, B and C

node A: publish/subscribe address - virtual group gateways (0xC003)
```
"hello from A - dst 0x0078" -> uart -> node A -> BLE Mesh -> 0xC003 
```

node B (gateway): subscriber address - virtual gateways group (0xC003)
```
"hello from A - dst 0x0078" <- uart -> node B -> libp2p cache add "hello from A" dst: 0x0078 -> BLE Mesh -> 0x0078
```

node C: publish/subscribe address - virtual group gateways (0xC003)
```
"hello from A" <- uart -> node C
```


## Contributing

Opportunistic Edge Computing Based on Mobile and Low-Power IoT Devices (ORBALLO) is a project funded by Ministerio de Ciencia e Innovación through grant PID2020-118857RA-I00.
