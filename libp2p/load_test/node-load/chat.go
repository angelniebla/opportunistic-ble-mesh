package main

import (
	"bufio"
	"context"
	"crypto/rand"
	"flag"
	"fmt"
	"io"
	"log"
	"strings"
	"sync"
	"time"

	"github.com/libp2p/go-libp2p"
	crypto "github.com/libp2p/go-libp2p-core/crypto"
	discovery2 "github.com/libp2p/go-libp2p-core/discovery"
	"github.com/libp2p/go-libp2p-core/peer"
	discovery "github.com/libp2p/go-libp2p-discovery"
	host "github.com/libp2p/go-libp2p-host"
	"gitlab.com/orballo_project/opportunistic-ble-mesh/libp2p/util"

	record "github.com/libp2p/go-libp2p-record"
	swarm "github.com/libp2p/go-libp2p-swarm"

	//"github.com/pkg/term"
	"go.uber.org/zap"

	dht "github.com/libp2p/go-libp2p-kad-dht"
	"github.com/multiformats/go-multiaddr"
	ma "github.com/multiformats/go-multiaddr"

	logv2 "github.com/ipfs/go-log/v2"
	//bhost "github.com/libp2p/go-libp2p/p2p/host/basic"
	"github.com/jacobsa/go-serial/serial"
)

var kademliaDHT *dht.IpfsDHT
var protocolBridge *Bridge

var (
	device = flag.String("device", "/dev/ttyACM0", "serial device name")
	baud   = flag.Int("baud", 115200, "default serial device baud")
)

type blankValidator struct{}

func (blankValidator) Validate(_ string, _ []byte) error        { return nil }
func (blankValidator) Select(_ string, _ [][]byte) (int, error) { return 0, nil }

func main() {
	config, err := util.ParseFlags()

	host, kademliaDHT, _ := newHost()

	for i := 0; i < 50; i++ {

		if err != nil {
			panic(err)
		}
		start := time.Now()
		run(config, host, kademliaDHT)
		elapsed := time.Since(start)

		log.Println("TOTAL TIME: ", elapsed)
	}

}

func run(config util.Config, host host.Host, kademliaDHT *dht.IpfsDHT) {
	logv2.SetAllLoggers(logv2.LevelWarn)
	logv2.SetLogLevel("rendezvous", "info")
	/*help := flag.Bool("h", false, "Display Help")

	if *help {
		fmt.Println("This program demonstrates a simple p2p chat application using libp2p")
		fmt.Println()
		fmt.Println("Usage: Run './chat in two different terminals. Let them connect to the bootstrap nodes, announce themselves and connect to the peers")
		flag.PrintDefaults()
		return
	}*/

	wg_bridge := &sync.WaitGroup{}

	openBridge(wg_bridge)

	//host, kademliaDHT, _ := newHost()
	start := time.Now()

	peer_relay := connectBootstrap(kademliaDHT, host, config)

	elapsed := time.Since(start)

	log.Println("BOOTSTRAP TIME: ", elapsed)

	// Set a function as stream handler. This function is called when a peer
	// initiates a connection and starts a stream with this peer.
	kademliaDHT.Host().SetStreamHandler("/chat/1.1.0", protocolBridge.handleStream)

	// Start a DHT, for use in peer discovery.

	//log.Printf("Announcing ourselves...")
	routingDiscovery := discovery.NewRoutingDiscovery(kademliaDHT)
	discovery.Advertise(context.Background(), routingDiscovery, config.RendezvousString, discovery2.Limit(1000))
	//log.Printf("Successfully announced!")

	// Now, look for others who have announced
	// This is like your friend telling you the location to meet you.
	//log.Printf("Searching for other peers...")
	peerChan, err := routingDiscovery.FindPeers(context.Background(), config.RendezvousString, discovery2.Limit(1000))
	if err != nil {
		panic(err)
	}

	//log.Println("DISCOVERY SIZE: ", cap(peerChan))

	//log.Println("Numero de peers anunciados: ",  len(peerChan))

	i := 0

	start = time.Now()

	for p := range peerChan {

		i = i + 1

		//log.Println("Numero de peers anunciados: ", i)

		relayaddr, err := circuitRelay(peer_relay, p.ID.Pretty())
		if err != nil {
			//log.Println(err)
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

		//kademliaDHT.Host().Network().Close()

		rw := bufio.NewReadWriter(bufio.NewReader(s), bufio.NewWriter(s))

		go protocolBridge.writeData(rw)
		go protocolBridge.readData(rw)

		//go protocolBridge.readData(rw)

	}

	log.Println("PEERS: ", i)

	elapsed = time.Since(start)

	log.Println("CONNECT TIME: ", elapsed.Seconds())

	saveValue("43.5033672,-8.2277297", "0x0000")

	protocolBridge.Close()

	//select {}
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

	var r io.Reader

	r = rand.Reader

	// Generate a key pair for this host. We will use it at least
	// to obtain a valid host ID.
	priv, _, err := crypto.GenerateKeyPairWithReader(crypto.RSA, 2048, r)

	h, err := libp2p.New(
		libp2p.ListenAddrs(),
		libp2p.Identity(priv),

		libp2p.EnableRelay(),
		libp2p.NoSecurity,
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

	//fmt.Println("we are connected to the bootstrap peer")

	//fmt.Println("DHT in a bootstrapped state")

	time.Sleep(time.Second * 5)

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
