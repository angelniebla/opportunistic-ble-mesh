## Introduction

The following instructions describe how to replicate the experiments carried out:

- Experiment 1, Section 4.2 - Latency when running the bootstrap node on the edge and on the cloud.
- Experiment 2, Section 4.3 - IoT OEC device performance with different hardware.
- Experiment 3, Section 4.4 - End-to-end latency between nodes in different opportunistic networks.
- Experiment 4, Section 4.5 - Security impact on OEC communications latency.

The following is the hardware involved in the experiments:

- Experiment 1: a local host on the edge (Raspberry Pi 3) and a host in a cloud server.
- Experiment 2: a local host on the edge (Raspberry Pi Zero) and a host in a cloud server.
- Experiment 3: two nrf5x boards, a local host on the edge (Raspberry Pi 3) of one network, a second local host on the edge of another network and a host in a cloud server.
- Experiment 4: a local host on the edge (Raspberry Pi 3) and a host in a cloud server.

## Experiments

### **Experiment 1** - Latency when running the bootstrap node on the Cloud/Edge with a different number of connected peers (further information can be obtained in Section 4.2 of the article).

This experiment is aimed at measuring the latency differences between local/edge and cloud communications under different load scenarios (e.g., connected peers).

Steps to be performed:
The node running on the cloud is called bootstrap node and it is a node within a swarm of nodes that works as a relay connection. This node is listening for incoming connections. The node running on the edge is the one that measures the latency respect to the bootstrap node.  

- 1 - Execute the script ```chat-node-relay``` located under folder ```/libp2p/latency_test/chat_node_relay/``` associated to the source file ```chat.go```. Run this script and copy the multiaddr addressing format and put it inside ```utils.go```. This is used to start the bootstrap node that will act as a relay.
 
- 2 - Execute the script ```generates-nodes``` located in folder ```/libp2p/load_test/generate-nodes``` associated to the source file ```test.go``` in the bootstrap node (i.e., related to the same node related to the previous step). This example will create 100 nodes connected to the bootstrap node. In order to test with different values to check how the number of peers connected to a bootstrap node impact the latency values, this value can be easily changed in line 69 and compiled again with ```go build -o generates-nodes```.    

- 3 - Next, on the edge host execute the script ```chat-gatewayA``` under ```/libp2p/latency_test/chat-gatewayA``` associated to the source file ```chat.go```.

- 4 - Get the latency times and repeat the previous step with the bootstrap node on a local host to compare the latency values.
 
### **Experiment 2** - Latency when running the bootstrap node on the Cloud/Edge with a different number of connected peers with different hardware (explained in detailed in Section 4.3 of the article).

Repeat the steps of the previous section but use a different hardware for the edge host. For instance, a Raspberry Pi zero was used in the article. 

### **Experiment 3** - End-to-end latency between nodes in different opportunistic networks (detailed in Section 4.4 of the article)

This experiment allows for measuring the total latency in an end-to-end IoT node communication. Thus, it covers the full scope of the designed communications protocol: the connection and communications between two IoT nodes with libp2p, and manages the BLE Mesh message exchange.

In this case, apart from the hosts, it is necessary to have Bluetooth connectivity based on the nrf5x chipset boards connected through a serial port. For this purpose, it is necessary to upload the firmware on these bluetooth boards: 

Open the files ```/nrf5_SDK_for_Mesh_v5.0.0_src_/examples/light_switch/client/light_switch_client_nrf52833_xxAA_s113_7_2_0.emProject``` or ```light_switch_client_nrf52840_xxAA_s140_7_2_0.emProject``` with the Segger IDE. 
Go to "Build", execute "Build and Run" to flash the client. Repeat the same procedure for the server on the other board under ```examples/light_switch/server``` folder.

After provisioning both nrf5x boards as described in the main README, the Bluetooth communication wil be ready.

Steps to be performed for carrying out the experiment:

- 1 - Execute the script ```chat-node-relay``` located under folder ```/libp2p/latency_test/chat_node_relay/``` associated to the source file ```chat.go```.

Run this script and copy the multiaddr and put it inside ```utils.go```, as it is also mentioned in the main README of this project. This is used to start the bootstrap node that will act as a relay.

It is also possible to check how latency is affected by multiple connections to the swarm in the case of an end-to-end communication.

- 2 - In order to test this feature, execute the script ```generates-nodes``` located in folder ```/libp2p/load_test/generate-nodes``` associated with the source file  ```test.go``` in the bootstrap node. Remember that this example will create 100 nodes connected to the swarm. In order to test with different values, it can be easily changed in line 69 and compiled again with ```go build -o generates-nodes```.    

- 3 - On a local host launch chat-gatewayA script (connect to bootstrap multiaddr) and chat-node script (under /libp2p/latency_test). Then, on the edge host (called A) execute the script ```chat-gatewayA``` under ```/libp2p/latency_test/chat-gatewayA``` associated with the source file ```chat.go``` and the chat-node script under ```/libp2p/latency_test/chat_node``` associated with the source file ```chat.go```.

- 4 - On another host execute the ```chat-gatewayB``` script (connect to bootstrap multiaddr) and the ```chat-node``` script (under ```/libp2p/latency_test```). Then, on the other edge host (called B), execute the script ```chat-gatewayB``` under ```/libp2p/latency_test/chat-gatewayB``` associated with the source file ```chat.go``` and the ```chat-node``` script under ```/libp2p/latency_test/chat_node``` associated with the source file ```chat.go```.

- 5 - Start sending messages between the end-device nodes.

### **Experiment 4** - Latency when running the bootstrap node on the Cloud/Edge with a different number of connected peers with/without encryption (described in detail in Section 4.5 of the article).

Repeat the steps for Experiment 1  but, in order to perform the experiments with or without encryption, comment or uncomment ```libp2p.NoSecurity``` in line 58 of chat.go for ```chat-node-relay``` located under folder ```/libp2p/latency_test/chat_node_relay/```

