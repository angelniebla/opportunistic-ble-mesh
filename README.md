# Opportunistic BLE Mesh (testbed scenarios)



## Introduction

This project allows to perform different test beds to evaluate the performance of an opportunistic communications system (named as ORBALLO) based on BLE Mesh with the following modifications in the protocol.

1. The friend node has been replaced by a distributed cache via libp2p. 

2. Is possible to use specific Bluetooth 5 modulations within the mesh communication. 

- Long Range (done)
- 2M (not yet)

Why this changes?

1. Currently the BLE Mesh protocol has a feature that allows a node (friend) to store the data of a low power node that is inactive for a certain period of time (LPN) when this node wakes up receives the data from the cache of the friend node. 

    This approach is intended for static nodes (sensors or actuators in a building for example) and when an LPN is not in range with the friend node that has stored its cache, the cache is lost.

    In an opportunistic system the nodes will be in movement and therefore the cache must be distributed among the different nodes of the mesh, libp2p provides a solution to this problem. 

2. The BLE Mesh protocol uses Bluetooth 4.x standard so it does not officially support any Bluetooth 5 modulation in the transmission, however since the standard is open source it can be modified and with hardware that supports Bluetooth 5 it is possible to use these modulations to obtain certain benefits in the communication in specific scenarios.

When

- Long range. For nodes located far away from each other or in harsh enviroments without intensive data transfer requirements

- 2M. For nodes in close range with high bandwidth demand (video, photos, ...)

When not

- Long range. When there are many nodes in range or the transfer rates are high. It must be taken into account that for increasing the range the datarate is reduced to 125 kbps and the time on air of the frame increases by 8x and too much use of the channels would lead to a saturation of the communication. 

- 2M. When communications are unreliable or have high bit error rate, considering that for this modulation the sensitivity is reduced, also are not suitable for very small messages 

## Requirements

The hardware has been coded using the Nordic Semiconductors SDK. So at least 3 boards with an nrf5x based SoC that can be coded using this SDK, if want to try specific Bluetooth 5 modulation (not mandatory) nrf52840 or nrf52833 are good options.

Since porting the libp2p code to a nrf5x based board would be tedious and since the mesh nodes that work as relays need a continuous power supply, the best option for this case is use a Raspebrry Pi or similiar with one of the Bluetooth SoC connected to the serial port, but any ARM device with a serial connection that can be programmed in golang could work :)

In order to provisionate and configure nodes within the Mesh the best option is to use the nordic nRF Mesh [app](https://play.google.com/store/apps/details?id=no.nordicsemi.android.nrfmeshprovisioner&gl=US) 

- [Nordic SDK](https://www.nordicsemi.com/Products/Development-software/nrf5-sdk/download)
- [SDK for Mesh](https://www.nordicsemi.com/Products/Development-software/nRF5-SDK-for-Mesh)
- [libp2p](https://github.com/libp2p/libp2p)

## Installation

## Usage

## Contributing

Opportunistic Edge Computing Based on Mobile and Low-Power IoT Devices (ORBALLO) is a project funded by Ministerio de Ciencia e Innovación through grant PID2020-118857RA-I00.
