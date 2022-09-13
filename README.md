# Opportunistic BLE Mesh (testbed scenarios)

The code of this repository has been used for performing the experiments of the following papers: "A Bluetooth 5 Opportunistic Edge Computing System for Vehicular Scenarios" (submited to ECSA-9), "Practical Latency Analysis of a Bluetooth 5 Decentralized IoT Opportunistic Edge Computing System for Low-Cost SBCs" (submited to Sensors). You can find in such papers further information on the experiments and on the proposed opportunistic architectures.

## Introduction

The code provided in this project allows for creating different testbeds to evaluate the performance of an Opportunistic Edge Computing (OEC) system based on Bluetooth 5 (specifically, on BLE Mesh) with the following modifications with respect to the original protocol:

1. The friend node has been replaced by a distributed cache via libp2p.

2. It is possible to use specific Bluetooth 5 modulations for mesh communications.

Why theses changes?

1. Currently, the BLE Mesh protocol has a feature that allows a node (called friend node) to store the data of a low-power node (LPN) that is inactive for a certain period of time. When this node wakes up, it receives the data from the cache memory of the friend node. This approach is intended for static nodes (for example, sensors or actuators in a building), so when an LPN is not within range of the friend node that has stored its cache, such a cache is lost.

    In an opportunistic system the nodes will be in movement, therefore the cache must be distributed among the different nodes of the mesh. In this situation, libp2p provides a good solution.


2. The BLE Mesh protocol uses the Bluetooth 4.x standard, so it does not officially support any Bluetooth 5 modulation for data transmission. However, since the standard is open source, it can be modified and, with hardware that supports Bluetooth 5, it is possible to make use of the Bluetooth 5 modulations to obtain certain benefits in specific scenarios. Specifically, Bluetooth 5 adds two major modulations: one for long range communications (LE Coded) and one for high bandwidth data exchanges (2M).

When are these modulation changes useful?

- LE Coded: for nodes located far away from each other or in harsh environments that do not require intensive data transfers.

- 2M: for nodes that communicate at short ranges and with high bandwidth demands (for instance, for transmitting video or photos).

When is it counterproductive to use these modulations?

- LE Coded: when there are many nodes in range or when transfer rates need to be high. It must be taken into account that in order to increase the communications range, the data rate is reduced to 125 kbps and the time on air of the frame increases by 8x, so the wireless media is occupied more time, which can lead to its saturation. 

- 2M: when communications are unreliable or have a high bit error rate. It is important to consider that the use of this modulation reduces sensitivity and that it is not suitable for sending very small messages.

## Requirements

The developed firmware has been implemented using the Nordic Semiconductors SDK. At least 3 boards with a nrf5x based SoC that can be programmed using the mentioned SDK are required. If you want to test a specific Bluetooth 5 modulation (not mandatory), the best option are boards based on the nrf52840 or nrf52833.

Since porting the libp2p code to an nrf5x based board is not a viable solution in the short term and because of the mesh nodes working as relays need a continuous power supply, a Raspberry Pi was used with one of the Bluetooth SoCs connected to its serial port (any ARM device with a serial connection that can be programmed in golang is also valid).

In order to provision and to configure the nodes within the Mesh, the best option is to use the nordic nRF Mesh [app](https://play.google.com/store/apps/details?id=no.nordicsemi.android.nrfmeshprovisioner&gl=US) 

Useful links:

- [Nordic SDK](https://www.nordicsemi.com/Products/Development-software/nrf5-sdk/download)
- [SDK for Mesh](https://www.nordicsemi.com/Products/Development-software/nRF5-SDK-for-Mesh)
- [libp2p](https://github.com/libp2p/libp2p)

## Installation

The installation is divided into several stages. The first one consists in flashing the Nordic SDK firmware needed into each board. In this case, the light_switch example of the Nordic mesh SDK was adapted. Two nodes will work as clients (LPNs) and one as server. The server node will also work as relay and friend node of the two LPNs. Such a server node needs to have go-libp2p installed.

For the Nordic firmware, the project includes the two Nordic SDKs needed to compile the software and to generate the firmware of the boards. It is necessary to install the SEGGER Embedded Studio IDE or a similar tool.

In the case of go-libp2p, it is necessary to have the golang compiler (at least with version 1.16).

To provision the nodes that will belong to the mesh, it is necessary to use a smartphone with BLE support and the Nordic nRF Mesh app.


## Usage

Open in Segger IDE the projects light_switch client and server, and build and run on the selected boards. Then, it is necessary to provision all the nodes with the app. For the client nodes, once provisioned, it is necessary to configure the "Generic On/Off Client" element binding an application key and defining a publishing address that can be the unicast address of the server node or a group address.

In the same way, the "Generic On/Off Server" server node element is configured, if the unicast address of the server is used in the client, it is not necessary to configure a subscription address.

The behavior of these nodes is as follows: the client nodes publish a message in ASCII (payload) to the addresses to which they are subscribed through the serial port with a destination address. Then, the message will be redirected through the relay to the destination node.

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
In the repository, there are two types of tests:

- The “load_test” is made up of “generate-nodes”. When the code is executed, it adds 100 nodes to the network. “node-load” generates a node that repeats the process of connecting to the network 50 times, discovering the rest of nodes and saving a value in a decentralized way in the DHT.

- The “latency_test” is composed of two gateways (chat-gatewayA, chat-gatewayB), a relay node (chat-node-relay) and an IoT node (chat-node). To carry out this test, the relay node must be executed on a IP-capable machine. The output of the execution will return an IP together with the peerID that we must add in the “utils” directory in the following line:

```
config.BootstrapPeers.Set("/ip4/x.x.x.x/tcp/<port>/p2p/<PeerID>")
``` 

Next, we must execute the code of the gateways, one for each SBC device, where we have previously flashed the Nordic nRF Mesh app code. Finally, we must execute the code of the IoT nodes in other SBC devices, also with the Nordic nRF code Mesh app flashed. For both nodes to communicate, the unicast address of the destination node must be modified in the "chat-node" code of the sending node, in the following line:

```
sendData := "chat private <GatewayID> #" + "PACK:" + strconv.Itoa(int(i)) + "(<NodeDestID>)" + "@ \n"
```

Where the <GatewayID> has to be the address of "chat-GatewayA" and the <NodeDestID> has to be the address of the IoT node that has to receive the data. From the execution, a loop of 1000 iterations will be executed, where the emitting node will send a message to the destination node.

## Contributing

Opportunistic Edge Computing Based on Mobile and Low-Power IoT Devices (ORBALLO) is a project funded by Ministerio de Ciencia e Innovación through grant PID2020-118857RA-I00.

## License
Shield: [![CC BY 4.0][cc-by-shield]][cc-by]

This work is licensed under a
[Creative Commons Attribution 4.0 International License][cc-by].

[![CC BY 4.0][cc-by-image]][cc-by]

[cc-by]: http://creativecommons.org/licenses/by/4.0/
[cc-by-image]: https://i.creativecommons.org/l/by/4.0/88x31.png
[cc-by-shield]: https://img.shields.io/badge/License-CC%20BY%204.0-lightgrey.svg
