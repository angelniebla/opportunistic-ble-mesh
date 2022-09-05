package main

import (
	"bufio"
	"context"
	"crypto/rand"
	"encoding/binary"
	"flag"
	"fmt"
	"log"
	"strconv"
	"strings"
	"sync"

	"github.com/libp2p/go-libp2p"
	"gitlab.com/orballo_project/opportunistic-ble-mesh/libp2p/util"

	crypto "github.com/libp2p/go-libp2p-core/crypto"
	"github.com/libp2p/go-libp2p-core/peer"
	discovery "github.com/libp2p/go-libp2p-discovery"
	host "github.com/libp2p/go-libp2p-host"
	pubsub "github.com/libp2p/go-libp2p-pubsub"
	router "github.com/libp2p/go-libp2p-pubsub-router"
	"github.com/libp2p/go-tcp-transport"

	record "github.com/libp2p/go-libp2p-record"
	swarm "github.com/libp2p/go-libp2p-swarm"

	"go.uber.org/zap"

	dht "github.com/libp2p/go-libp2p-kad-dht"
	"github.com/multiformats/go-multiaddr"
	ma "github.com/multiformats/go-multiaddr"

	logv2 "github.com/ipfs/go-log/v2"
	"github.com/jacobsa/go-serial/serial"
	//tls "github.com/libp2p/go-libp2p-tls"
	//util "./util"
)

var kademliaDHT *dht.IpfsDHT
var protocolBridge *Bridge
var pubsubb *router.PubsubValueStore

var (
	device = flag.String("device", "/dev/ttyACM0", "serial device name")
	baud   = flag.Int("baud", 115200, "default serial device baud")
	listen = flag.String("listen", "/ip4/0.0.0.0/tcp/0", "The listen address")
)

type blankValidator struct{}

func (blankValidator) Validate(_ string, _ []byte) error        { return nil }
func (blankValidator) Select(_ string, _ [][]byte) (int, error) { return 0, nil }

func main() {
	config, err := util.ParseFlags()

	host, kademliaDHT, _ := newHost()

	if err != nil {
		panic(err)
	}

	run(config, host, kademliaDHT)

}

func run(config util.Config, host host.Host, kademliaDHT *dht.IpfsDHT) {
	logv2.SetAllLoggers(logv2.LevelWarn)
	logv2.SetLogLevel("rendezvous", "info")
	/*
		help := flag.Bool("h", false, "Display Help")

		if *help {
			fmt.Println("This program demonstrates a simple p2p chat application using libp2p")
			fmt.Println()
			fmt.Println("Usage: Run './chat in two different terminals. Let them connect to the bootstrap nodes, announce themselves and connect to the peers")
			flag.PrintDefaults()
			return
		}
	*/

	wg_bridge := &sync.WaitGroup{}

	openBridge(wg_bridge)

	ctx := context.Background()

	//ps, err := pubsub.NewGossipSub(context.Background(), host)

	fs, err := pubsub.NewFloodSub(ctx, host)
	if err != nil {
		panic(err)
	}

	pubsubb, err = router.NewPubsubValueStore(ctx, host, fs, blankValidator{})
	if err != nil {
		panic(err)
	}

	pubsubb.Subscribe("/v/test")

	peer_relay := connectBootstrap(kademliaDHT, host, config)

	// Set a function as stream handler. This function is called when a peer
	// initiates a connection and starts a stream with this peer.
	kademliaDHT.Host().SetStreamHandler("/chat/1.1.0", protocolBridge.handleStream)

	// Start a DHT, for use in peer discovery.

	log.Printf("Announcing ourselves...")
	routingDiscovery := discovery.NewRoutingDiscovery(kademliaDHT)
	discovery.Advertise(context.Background(), routingDiscovery, config.RendezvousString)
	log.Printf("Successfully announced!")

	// Now, look for others who have announced
	// This is like your friend telling you the location to meet you.
	log.Printf("Searching for other peers...")
	peerChan, err := routingDiscovery.FindPeers(context.Background(), config.RendezvousString)
	if err != nil {
		panic(err)
	}

	for p := range peerChan {

		relayaddr, err := circuitRelay(peer_relay, p.ID.Pretty())
		if err != nil {
			log.Println(err)
			return
		}

		// Creates a relay address to h3 using h2 as the relay
		/*
			relayaddr, err := ma.NewMultiaddr("/p2p/" + peer_relay + "/p2p-circuit/ipfs/" + p.ID.Pretty())
			if err != nil {
				log.Println(err)
				return
			}
		*/

		// Since we just tried and failed to dial, the dialer system will, by default
		// prevent us from redialing again so quickly. Since we know what we're doing, we
		// can use this ugly hack (it's on our TODO list to make it a little cleaner)
		// to tell the dialer "no, its okay, let's try this again"
		kademliaDHT.Host().Network().(*swarm.Swarm).Backoff().Clear(p.ID)

		h3relayInfo := peer.AddrInfo{
			ID:    p.ID,
			Addrs: []ma.Multiaddr{relayaddr},
		}

		if err := kademliaDHT.Host().Connect(context.Background(), h3relayInfo); err != nil {
			//log.Printf("Failed to connect peer: %v", err)
			continue
		}

		//we're connected!
		s, err := kademliaDHT.Host().NewStream(context.Background(), p.ID, "/chat/1.1.0")
		if err != nil {
			//log.Println("huh, this should have worked: ", err)
			continue
		}

		rw := bufio.NewReadWriter(bufio.NewReader(s), bufio.NewWriter(s))

		//go protocolBridge.writeData(rw)

		lat := []byte{0x01, 0xBA, 0xBE, 0xCA}
		lon := []byte{0x0F, 0xCA, 0xFE, 0x0A}

		latToUint := binary.BigEndian.Uint32(lat)

		latToString := strconv.FormatUint(uint64(latToUint), 10)

		lonToUint := binary.BigEndian.Uint32(lon)

		lonToString := strconv.FormatUint(uint64(lonToUint), 10)

		coodinate := latToString + "/" + lonToString

		protocolBridge.writeData2(rw, coodinate)

		/*thousandBytes := make([]byte, 100)

		rand.Read(thousandBytes)
		//copy(thousandBytes[:], "1234")

		protocolBridge.writeData2(rw, string(thousandBytes))
		*/

		//go protocolBridge.writeData(rw)

		//go protocolBridge.readData(rw)

	}
	select {}
}

func circuitRelay(target string, id string) (multiaddr.Multiaddr, error) {
	return multiaddr.NewMultiaddr("/p2p/" + target + "/p2p-circuit/ipfs/" + id)
}

func contains(slice []multiaddr.Multiaddr, item string) bool {

	set := make(map[string]struct{}, len(slice))
	for _, s := range slice {
		ss := strings.SplitAfter(s.String(), "/tcp")[0]
		//fmt.Println(ss)
		set[ss] = struct{}{}
	}

	_, ok := set[item]
	return ok
}

func newHost() (host.Host, *dht.IpfsDHT, error) {

	listenAddress := libp2p.ListenAddrStrings(*listen)

	//PSK := make([]byte, 32)
	//copy(PSK[:], "password")

	//priv := libp2p.PrivateNetwork(PSK)

	priv, _, err := crypto.GenerateKeyPairWithReader(crypto.RSA, 2048, rand.Reader)
	if err != nil {
		panic(err)
	}

	identity := libp2p.Identity(priv)

	//security := libp2p.Security(noise.ID, noise.New)

	h, err := libp2p.New(
		listenAddress,
		libp2p.ListenAddrs(),
		libp2p.EnableRelay(),
		libp2p.Transport(tcp.NewTCPTransport),
		//libp2p.NoSecurity,
		//priv,
		identity,
		//security,
	)
	if err != nil {
		log.Printf("Failed to create h1: %v", err)
		panic(err)
	}

	fmt.Println("Host Created")
	for _, addr := range h.Addrs() {
		fmt.Printf("Address: %s/%v\n", addr, h.ID().Pretty())
	}

	baseOpts := []dht.Option{dht.Mode(dht.ModeAutoServer), dht.DisableAutoRefresh()}

	//baseOpts = append(baseOpts, dht.Mode(dht.ModeAutoServer))

	kademliaDHT, err = dht.New(context.Background(), h, baseOpts...)
	if err != nil {
		panic(err)
	}

	if err != nil {
		log.Fatal(err)
	}

	kademliaDHT.Validator.(record.NamespacedValidator)["v"] = blankValidator{}

	return h, kademliaDHT, err

}

func connectBootstrap(ddht *dht.IpfsDHT, host host.Host, config util.Config) string {
	// connect to the bootstrap peers

	ma, err := multiaddr.NewMultiaddr(config.BootstrapPeers.String())
	if err != nil {
		panic(err)
	}

	peerInfo, err := peer.AddrInfoFromP2pAddr(ma)
	if err != nil {
		panic(err)
	}

	if err := ddht.Bootstrap(context.Background()); err != nil {
		panic(err)
	}

	if err := host.Connect(context.Background(), *peerInfo); err != nil {
		panic(err)
	}

	fmt.Println("we are connected to the bootstrap peer")

	fmt.Println("DHT in a bootstrapped state")

	//time.Sleep(time.Second * 5)

	return peerInfo.ID.Pretty()
}

func openBridge(wg *sync.WaitGroup) *Bridge {

	logger := zap.NewExample()

	options := serial.OpenOptions{
		PortName:        "/dev/ttyACM0",
		BaudRate:        115200,
		DataBits:        8,
		StopBits:        1,
		MinimumReadSize: 4,
	}

	port, err := serial.Open(options)
	//trm, err := serial.Open(*device, term.Speed(*baud))
	if err != nil {
		log.Fatal(err)
	}
	protocolBridge, err = NewBridge(context.Background(), wg, logger, port, Opts{})
	if err != nil {
		log.Fatal(err)
	}

	return protocolBridge

}
