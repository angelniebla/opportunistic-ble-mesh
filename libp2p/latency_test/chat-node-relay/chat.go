package main

import (
	context2 "context"
	"crypto/rand"
	"flag"
	"fmt"

	"github.com/libp2p/go-libp2p"
	crypto "github.com/libp2p/go-libp2p-core/crypto"
	dht "github.com/libp2p/go-libp2p-kad-dht"
	"github.com/libp2p/go-tcp-transport"

	"github.com/libp2p/go-libp2p-core/host"
	"github.com/libp2p/go-libp2p-core/routing"

	//quic "github.com/libp2p/go-libp2p-quic-transport"

	relayv1 "github.com/libp2p/go-libp2p/p2p/protocol/circuitv1/relay"
)

func main() {
	listen := flag.String("listen", "/ip4/0.0.0.0/tcp/5000", "The listen address")
	flag.Parse()

	ctx, cancel := context2.WithCancel(context2.Background())
	defer cancel()

	baseOpts := []dht.Option{}

	baseOpts = append(baseOpts, dht.Mode(dht.ModeServer))

	routing := libp2p.Routing(func(host host.Host) (routing.PeerRouting, error) {
		return dht.New(ctx, host, baseOpts...)
	})

	listenAddress := libp2p.ListenAddrStrings(*listen)

	priv, _, err := crypto.GenerateKeyPairWithReader(crypto.RSA, 2048, rand.Reader)
	if err != nil {
		panic(err)
	}

	identity := libp2p.Identity(priv)

	//security := libp2p.Security(tls.ID, tls.New)

	//testPSK := make([]byte, 32)
	//copy(testPSK[:], "password")
	//testPSK = []byte("password")

	//pass := libp2p.PrivateNetwork(testPSK)

	host, err := libp2p.New(
		routing,
		listenAddress,
		libp2p.DisableRelay(),
		libp2p.NoSecurity,
		//libp2p.NATPortMap(),
		//libp2p.DefaultTransports,
		//circuitv2.AddTransport(host),
		libp2p.Transport(tcp.NewTCPTransport),
		//libp2p.Transport(quic.NewTransport(priv, nil, nil)),
		identity,
		//pass,
		//security,
	)

	//upgrader.ConnGater = cfg.ConnectionGater

	//circuitv2.AddTransport(host, upgrader)

	if err != nil {
		panic(err)
	}

	_, err = relayv1.NewRelay(host)

	for _, addr := range host.Addrs() {
		fmt.Printf("Addr: %s/p2p/%s\n", addr, host.ID().Pretty())
	}

	select {}
}
