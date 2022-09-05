package main

import (
	"bufio"
	"context"
	"fmt"
	"io"
	"log"
	"os"
	"regexp"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/libp2p/go-libp2p-core/network"
	peer "github.com/libp2p/go-libp2p-core/peer"
	"github.com/libp2p/go-libp2p-core/protocol"
	"github.com/pkg/term"
	"go.uber.org/zap"
	//"github.com/jacobsa/go-serial/serial"
)

// ProtocolID is the protocol ID of the liblora bridge
var ProtocolID = protocol.ID("/liblora-bridge/0.0.1")
var recover_value = ""

// ensure term.Term always satisfies this interface
var _ Serial = (*term.Term)(nil)

// Serial is an interface to provide easy testing of the lora protocol
type Serial interface {
	Write([]byte) (int, error)
	Available() (int, error)
	Read([]byte) (int, error)
	Flush() error
	Close() error
}

// Bridge allows authorized peers to open a stream
// and read/write data through the LoRa bridge
type Bridge struct {
	serial          io.ReadWriteCloser
	ctx             context.Context
	wg              *sync.WaitGroup
	authorizedPeers map[peer.ID]bool
	readChan        chan []byte
	writeChan       chan []byte
	logger          *zap.Logger
}

// Opts allows configuring the bridge
type Opts struct {
	AuthorizedPeers map[peer.ID]bool // empty means allow all
}

// NewBridge returns an initialized bridge, suitable for use a LibP2P protocol
func NewBridge(ctx context.Context, wg *sync.WaitGroup, logger *zap.Logger, serial io.ReadWriteCloser, opt Opts) (*Bridge, error) {
	bridge := &Bridge{
		serial:          serial,
		ctx:             ctx,
		authorizedPeers: opt.AuthorizedPeers,
		logger:          logger.Named("lora.bridge"),
		readChan:        make(chan []byte, 1000),
		writeChan:       make(chan []byte, 1000),
		wg:              wg,
	}
	bridge.serialDumper()
	return bridge, nil
}

var data_parse []byte

var continue_data bool = false
var continue_sender bool = true
var waiting_ack bool = false

var message_full string = ""
var sender_user string = ""
var receiver_user string = ""

// serialDumper allows any number of libp2p streams to write
// into the LoRa bridge, or read from it. Reads are sent to any
// active streams
func (b *Bridge) serialDumper() {
	b.wg.Add(1)
	go func() {
		defer b.wg.Done()
		for {
			select {
			case <-b.ctx.Done():
				return
			default:
				data := make([]byte, 1000)

				s, err := b.serial.Read(data)
				if err != nil && err != io.EOF {
					b.logger.Error("error reading serial data", zap.Error(err))
					return
				}
				if s < 1 {
					continue
				}

				if !waiting_ack {

					if len(sender_user) <= 0 || continue_sender {

						sender_user, continue_sender = parse_message(data, `\<.*\>`, `\<.*`, `.*\>`)

						if continue_sender {
							continue
						}

						sender_user = parse_value(sender_user, "<", ">")
						sender_user = strings.ReplaceAll(sender_user, "\u0000", "")
						sender_user = strings.ReplaceAll(sender_user, " ", "")

					}

					if sender_user != "you" {

						message_full, continue_data = parse_message(data, `\#.*\@`, `\ \#.*`, `.*\@`)

						if continue_data {
							continue
						}
						message_parse := parse_value(string(message_full), "#", "@")

						receiver_user = parse_value(string(message_parse), "(", ")")

						receiver_user = strings.ReplaceAll(receiver_user, "\u0000", "")

						message_parse = strings.ReplaceAll(message_parse, receiver_user, "")

						message_parse = strings.ReplaceAll(message_parse, "()", "")

						message_parse = strings.ReplaceAll(message_parse, "\u0000", "")

						b.logger.Info("received serial data")
						b.logger.Info(message_parse)
						println(message_parse)

						if kademliaDHT != nil && len(message_full) > 0 {

							if receiver_user != "" {
								fmt.Println("The message is send by: ", sender_user, " and is send to: ", receiver_user)

								//saveValue(message_parse, receiver_user)
								//b.forwardMessage(message_parse, receiver_user)

							} else {
								fmt.Println("The message is send by: ", sender_user, " and is send to me")
								//saveValue(message_parse, "0x0000")
							}

						}

						data_parse = nil
						sender_user = ""
						continue_sender = true
						message_full = ""
						continue_data = true

					} else {
						fmt.Println("The message is yours")

						sender_user = ""
						continue_sender = true
					}
				} else {
					log.Printf("Waiting to ACK")
					//fmt.Println(string(data))
					ack_message, continue_ack := parse_message(data, `\&.*\%`, `\&.*`, `.*\%`)

					if continue_ack {
						continue
					}

					ack_message = parse_value(string(ack_message), "&", "%")
					ack_message = strings.ReplaceAll(ack_message, "\u0000", "")

					println(ack_message)

					//if !strings.Contains(ack_message, "timeout") && !strings.Contains(ack_message, "received") {
					//	continue
					//}
					//b.logger.Info("received serial data")
					b.logger.Info(ack_message)
					println(ack_message)

					waiting_ack = false
				}
			}
		}
	}()
}

func saveValue(mymessage string, key string) {
	ctx, cancel := context.WithCancel(context.Background())
	e := pubsubb.PutValue(ctx, "/v/"+"test", []byte(mymessage))
	defer cancel()
	if e != nil {
		log.Fatalf("not value save: %v", e)
	}

	recover_value, e := pubsubb.GetValue(ctx, "/v/"+"test")
	if e != nil {
		fmt.Println("not value: ", e)
	}
	fmt.Println("value saved: ", string(recover_value), " whith this key: ", key)

}

/*

func (b *Bridge) monitorDHT(key string) {

	for {
		time.Sleep(10 * time.Second)
		recover_value, e := kademliaDHT.GetValue(context.Background(), "/v/"+key)
		if e != nil {
			log.Printf("not value for user %v", key)
			continue
		}
		fmt.Println(string(recover_value))
		b.forwardMessage(string(recover_value), sender_user)
	}

}
*/

func (b *Bridge) forwardMessage(mymessage string, key string) {
	b.logger.Info("writing data to serial interface:")
	//sendData = "chat msg #" + sendData + "@" + "<>" + "\n"
	sendData := "chat private " + key + " #" + mymessage + "@ \n"
	log.Println(sendData)
	//waiting_ack = true
	//time.Sleep(10 * time.Second)
	_, err := b.serial.Write([]byte(sendData))
	if err != nil && err != io.EOF {
		b.logger.Info("failed to write into serial interface", zap.Error(err))
		return
	}
	b.logger.Info("finished writing serial data")
}

// Close is used to shutdown the bridge serial interface
func (b *Bridge) Close() error {
	return b.serial.Close()
}

func (b *Bridge) handleStream(s network.Stream) {
	b.logger.Info("Got a new stream!")

	// Create a buffer stream for non blocking read and write.
	rw := bufio.NewReadWriter(bufio.NewReader(s), bufio.NewWriter(s))

	go b.readData(rw)
	go b.writeData(rw)

	// stream 's' will stay open until you close it (or the other side closes it).
}

func (b *Bridge) writeData(rw *bufio.ReadWriter) {
	stdReader := bufio.NewReader(os.Stdin)

	for {
		fmt.Print("> ")
		sendData, err := stdReader.ReadString('\n')
		sendData = strings.TrimSuffix(sendData, "\n")
		if err != nil {
			log.Println(err)
			return
		}

		fmt.Println("you sent this: ", sender_user)

		if sendData == "load2" {
			ctx := context.Background()

			recover_value, e := kademliaDHT.GetValue(ctx, "/v/0x0004")
			if e != nil {
				fmt.Println("not value: ", e)
				return
			}
			fmt.Println(string(recover_value))
			return
		}

		b.logger.Info("writing data to serial interface:")
		sendDataFormat := "chat msg #" + sendData + "@ \n"
		log.Println(sendDataFormat)

		start := time.Now()
		_, err = b.serial.Write([]byte(sendDataFormat))
		if err != nil && err != io.EOF {
			b.logger.Error("failed to write into serial interface", zap.Error(err))
			return
		}
		elapsed := time.Since(start)

		log.Println("DELIVERY TIME: ", elapsed.Seconds())
		subscribe(sendData)
	}
}

func findIP(input string) string {

	re := regexp.MustCompile(`([0-9]?)(x(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?))([0-9]?)`)
	return re.FindString(input)
}

func (b *Bridge) writeData2(rw *bufio.ReadWriter, data string) {
	//stdReader := bufio.NewReader(os.Stdin)

	for i := 0; i < 1000; i++ {

		b.logger.Info("writing data to serial interface:")
		//sendData := "chat private 0x0010 #" + data + "(0x0015)" + "@ \n"

		sendData := "chat private 0x0010 #" + "PACK:" + strconv.Itoa(int(i)) + "(0x0011)" + "@ \n"

		//start := time.Now()

		n, err := b.serial.Write([]byte(sendData))
		if err != nil && err != io.EOF {
			b.logger.Error("failed to write into serial interface", zap.Error(err))
			return
		}

		log.Println("BYTES SEND: ", n)

		//elapsed := time.Since(start)

		log.Println("DELIVERY TIME: ", time.Now())

		time.Sleep(5 * time.Second)

		/*

			start2 := time.Now()
			subscribe(data)
			elapsed2 := time.Since(start2)
			log.Println("PROPAGATION TIME: ", elapsed2.Seconds())

			time.Sleep(5 * time.Second)
			//pubsubb.PutValue(context.Background(), "/v/test", []byte(""))
			//start3 := time.Now()
			pubsubb.PutValue(context.Background(), "/v/test", []byte(""))
			//elapsed3 := time.Since(start3)
			//log.Println("PUT TIME: ", elapsed3.Seconds())
		*/
	}
}

func (b *Bridge) readData(rw *bufio.ReadWriter) {
	for {
		str, _ := rw.ReadString('\n')

		if str == "" {
			return
		}
		if str != "\n" {
			// Green console colour: 	\x1b[32m
			// Reset console colour: 	\x1b[0m
			fmt.Printf("\x1b[32m%s\x1b[0m> ", str)

			if kademliaDHT != nil {

				ctx, cancel := context.WithCancel(context.Background())
				defer cancel()

				e := kademliaDHT.PutValue(ctx, "/v/hello", []byte("valid"))
				if e != nil {
					fmt.Println("not value save: ", e)
				}

				recover_value, e := kademliaDHT.GetValue(ctx, "/v/hello")
				if e != nil {
					fmt.Println("not value: ", e)
				}
				fmt.Println(string(recover_value))
			}
		}

	}
}

func subscribe(data string) {
	for {

		ch, err := pubsubb.SearchValue(context.Background(), "/v/test")
		//log.Println("PUT TIME: ", string(<-ch))

		/*
			start2 := time.Now()
			pubsubb.PutValue(context.Background(), "/v/test", []byte(""))
			elapsed2 := time.Since(start2)
			log.Println("PUT TIME: ", elapsed2.Seconds())
		*/

		if err != nil {
			continue
		}

		value := string(<-ch)

		if value != data {
			//log.Println("Value find is not the search: ", value)
			continue
		}

		fmt.Println("GOOOOT VALUE: ", value)

		//pubsubb.PutValue(context.Background(), "/v/test", []byte(""))

		break
	}

}

func parse_message(message []byte, ex1 string, ex2 string, ex3 string) (string, bool) {

	r1, _ := regexp.Compile(ex1)

	matched1 := r1.MatchString(string(message))

	if !matched1 {

		r2, _ := regexp.Compile(ex2)

		r3, _ := regexp.Compile(ex3)

		matched2 := r2.MatchString(string(message))
		matched3 := r3.MatchString(string(message))

		if !matched3 {
			if matched2 {
				//fmt.Println("part1", string(message))
				data_parse = r2.Find(message)
			} else if data_parse != nil {
				//fmt.Println("part2", string(message))
				data_parse = append(data_parse, message...)
			}
			return string(data_parse), true
		} else if len(data_parse) == 0 {
			return string(data_parse), true
		}

		//fmt.Println(r2.FindString(string(data)))

		data_parse = append(data_parse, r3.Find(message)...)

	} else {
		data_parse = r1.Find(message)
	}

	return string(data_parse), false

}

func parse_value(value string, a string, b string) string {
	// Get substring between two strings.
	posFirst := strings.Index(value, a)
	if posFirst == -1 {
		return ""
	}
	posLast := strings.Index(value, b)
	if posLast == -1 {
		return ""
	}
	posFirstAdjusted := posFirst + len(a)
	if posFirstAdjusted >= posLast {
		return ""
	}
	return value[posFirstAdjusted:posLast]
}
