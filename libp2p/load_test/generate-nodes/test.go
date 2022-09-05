package main

import (
	"context"
	"crypto/rand"
	"fmt"
	"io"
	"log"
	"time"

	"github.com/libp2p/go-libp2p"
	"github.com/libp2p/go-libp2p-core/peer"
	crypto "github.com/libp2p/go-libp2p-core/crypto"
	discovery "github.com/libp2p/go-libp2p-discovery"
	host "github.com/libp2p/go-libp2p-host"

	record "github.com/libp2p/go-libp2p-record"
	//"github.com/pkg/term"

	dht "github.com/libp2p/go-libp2p-kad-dht"
	"github.com/multiformats/go-multiaddr"

	logv2 "github.com/ipfs/go-log/v2"
	//bhost "github.com/libp2p/go-libp2p/p2p/host/basic"
)

var kademliaDHT *dht.IpfsDHT

type blankValidator struct{}

func (blankValidator) Validate(_ string, _ []byte) error        { return nil }
func (blankValidator) Select(_ string, _ [][]byte) (int, error) { return 0, nil }

func main() {
	config, err := ParseFlags()

	if err != nil {
		panic(err)
	}

	for i := 0; i < 1; i++ {
		run(config)
	}

}

func run(config Config) {
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

		config, err := ParseFlags()
		if err != nil {
			panic(err)
		}
	*/

	for i := 0; i < 100; i++ {
		host, kademliaDHT, _ := newHost()

		fmt.Println("Host ", i, " Created")

		connectBootstrap(kademliaDHT, host, config)

		log.Printf("Announcing ourselves...")
		routingDiscovery := discovery.NewRoutingDiscovery(kademliaDHT)
		discovery.Advertise(context.Background(), routingDiscovery, config.RendezvousString)
		log.Printf("Successfully announced!")

	}

	//log.Printf("Created", 25, "hosts")

	//select {}

}

func newHost() (host.Host, *dht.IpfsDHT, error) {

	var r io.Reader

	r = rand.Reader

	// Generate a key pair for this host. We will use it at least
	// to obtain a valid host ID.
	priv, _, err := crypto.GenerateKeyPairWithReader(crypto.RSA, 2048, r)

	h, err := libp2p.New(
		libp2p.ListenAddrs(),
		libp2p.EnableRelay(),
		libp2p.NoSecurity,
		libp2p.Identity(priv),
	)
	if err != nil {
		log.Printf("Failed to create h1: %v", err)
		panic(err)
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

func connectBootstrap(ddht *dht.IpfsDHT, host host.Host, config Config) string {
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

	time.Sleep(time.Second * 5)

	return peerInfo.ID.Pretty()
}
