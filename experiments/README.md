## Introduction

The steps to replicate the experiments carried out in the first article cited in this repository are detailed as follows. In such article there are 4 different types of experiments associated to different sections.

- Experiment 1, Section 4.2 - Latency when running the bootstrap node on the edge and on the cloud.
- Experiment 2, Section 4.3 - IoT OEC device performance with different hardware.
- Experiment 3, Section 4.4 - End-to-end latency between nodes in different opportunistic networks.
- Experiment 4, Security impact on OEC communications latency.

The hardware involved to perform these experiments is as follows.

- Experiment 1: a host in local/edge (Raspberry Pi 3 was used) and a host in a cloud.
- Experiment 2: a host in local/edge (Raspberry Pi Zero was used) and a host in a cloud.
- Experiment 3: two nrf5x boards, a host in local/edge (Raspberry Pi 3 was used), another host in local/edge and a host in a cloud.
- Experiment 4: a host in local/edge (Raspberry Pi 3 was used) and a host in a cloud.

## Structure of the experiments

The following steps are required to be able to replicate such testbeds explained in the article.      

**Experiment 1** - Latency when running the bootstrap node on the Cloud/Edge with a different number of connected peers (further explained in Section 4.2 of the article).

This test aims to check the latency differences between local/edge and cloud communications under different load scenarios (e.g., connected peers).
Steps to be performed:
The node running in the cloud is the bootstrap node, a node within a swarm of nodes that will work as a relay connection. This node is listening for incoming connections.
 
The node running on the edge is the one that performs the experiments against the node located in the cloud.  

- 1 - Execute the script ```chat-node-relay``` located under folder ```/libp2p/latency_test/chat_node_relay/``` associated to the source file ```chat.go```. Run this script and copy the multiaddr addressing format and put it inside ```utils.go```, this is used to start the bootstrap node that will function as relay, as it was previously mentioned in the README file of the project.
 
- 2 - Execute the script ```generates-nodes``` located in folder ```/libp2p/load_test/generate-nodes``` associated to the source file ```test.go``` in the bootstrap node (the same of the previous step). This example will create 100 nodes connected to the bootstrap node. In order to test with different values to check how the number of peers connected to a boot to a bootstrap node impact the latency values, this value can be easily changed in line 69 and compiled again with ```go build -o generates-nodes```.    

- 3 - Now on the edge host execute the script ```chat-gatewayA``` under ```/libp2p/latency_test/chat-gatewayA``` associate to the source file ```chat.go```.

- 4 - Get the latency times and repeat the previous step with the bootstrap node on a local host to compare the latency values.
 
**Experiment 2** - Latency when running the bootstrap node on the Cloud/Edge with a different number of connected peers with different hardware (further explained in Section 4.3 of the article).
Repeat the steps of the previous section but use a different hardware for the edge host. For instance, a Raspberry Pi zero was used in the article. 

**Experiment 3** - Latency when running the bootstrap node on the Cloud/Edge with a different number of connected peers with/without encryption (further explained in Section 4.5 of the article).
Repeat the steps of Experiment 1  but in order to repeat the experiments with or without encryption comment or uncomment ```libp2p.NoSecurity``` in line 58 of chat.go for ```chat-node-relay``` located under folder ```/libp2p/latency_test/chat_node_relay/```

**Experiment 4** - End-to-end latency between nodes in different opportunistic networks (further explained in Section 4.4 of the article)
This test measures the total latency in an end-to-end IoT node communication. Thus, it covers the full scope of the designed communications protocol: the connection and communications between two IoT nodes with libp2p, and manages the BLE Mesh message exchange.

In this case, apart from the hosts, it is necessary to have bluetooth based on the nrf5x chipset boards connected by serial for bluetooth communication. 
For this purpose, it is necessary to upload the firmware on these bluetooth boards. 

Open the files ```/nrf5_SDK_for_Mesh_v5.0.0_src_/examples/light_switch/client/light_switch_client_nrf52833_xxAA_s113_7_2_0.emProject``` or ```light_switch_client_nrf52840_xxAA_s140_7_2_0.emProject``` with the Segger IDE. 
Go to "Build", execute "Build and Run" to flash the client. Repeat the same procedure for the server on the other board under ```examples/light_switch/server``` folder.

After that provisionate both boards as described in the main README and the Bluetooth communication wil be ready.

Steps to be performed:
- 1 - Execute the script ```chat-node-relay``` located under folder ```/libp2p/latency_test/chat_node_relay/``` associated to the source file ```chat.go```.

Run this script and copy the multiaddr and put it inside ```utils.go``` as it is also mentioned in the README of the project, this is used as the start the bootstrap node that will function as relay.

It is also possible to check how latency is affected by multiple connections to the swarm in the case of an end-to-end communication.

- 2 - In order to test this feature, execute the script ```generates-nodes``` located in folder ```/libp2p/load_test/generate-nodes``` associated to the source file  ```test.go``` in the bootstrap node. Remember that this example will create 100 nodes connected to the swarm. In order to test with different values, it can be easily changed in line 69 and compiled again with ```go build -o generates-nodes```.    

- 3 - On a local host launch chat-gatewayA script (connect to bootstrap multiaddr) and chat-node script (under /libp2p/latency_test)
Now on the edge host (called as A). execute the script ```chat-gatewayA``` under ```/libp2p/latency_test/chat-gatewayA``` associated to the source file ```chat.go``` and the chat-node script under ```/libp2p/latency_test/chat_node``` associated to the source file ```chat.go```.

- 4 - On another host launch ```chat-gatewayB``` script (connect to bootstrap multiaddr) and ```chat-node``` script (under ```/libp2p/latency_test```)
Now on the other edge host (called as B), execute the script ```chat-gatewayB``` under ```/libp2p/latency_test/chat-gatewayB``` associated to the source file ```chat.go``` and the ```chat-node``` script under ```/libp2p/latency_test/chat_node``` associated to the source file ```chat.go```.

- 5 - Start sending messages between end-device nodes
