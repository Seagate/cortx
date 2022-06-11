package drpold

import (
	"crypto/tls"
	"encoding/json"
	"log"
	"strconv"

	"github.com/gorilla/websocket"
)

// DRPEndpointMethod - DRP Endpoint method
type DRPEndpointMethod func(DRPCmdParams, *websocket.Conn, *string) interface{}

// DRPCmdParams - DRP Cmd parameters
type DRPCmdParams map[string]interface{}

// Endpoint - DRP endpoint
type Endpoint struct {
	EndpointCmds      map[string]DRPEndpointMethod
	ReplyHandlerQueue map[string](chan DRPPacketIn)
	TokenNum          int
	sendChan          chan interface{}
}

func (e *Endpoint) Init() {
	e.EndpointCmds = make(map[string]DRPEndpointMethod)
	e.ReplyHandlerQueue = make(map[string](chan DRPPacketIn))
	e.sendChan = make(chan interface{}, 100)
	e.TokenNum = 1
}

func (e *Endpoint) GetToken() string {
	returnToken := strconv.Itoa(e.TokenNum)
	e.TokenNum++
	return returnToken
}

func (e *Endpoint) AddReplyHandler() string {
	replyToken := e.GetToken()
	e.ReplyHandlerQueue[replyToken] = make(chan DRPPacketIn)
	return replyToken
}

func (e *Endpoint) DeleteReplyHandler() {
}

func (e *Endpoint) RegisterCmd(methodName string, method DRPEndpointMethod) {
	e.EndpointCmds[methodName] = method
}

// SendCmd
func (e *Endpoint) SendCmd(methodName string, cmdParams DRPCmdParams) string {
	token := e.AddReplyHandler()

	sendCmd := &DRPCmd{}
	sendCmd.Type = "cmd"
	sendCmd.Method = &methodName
	sendCmd.Params = cmdParams
	sendCmd.Token = &token
	e.sendChan <- *sendCmd
	return token
}

// SendCmdAwait
func (e *Endpoint) SendCmdAwait(cmdName string, cmdParams DRPCmdParams) DRPPacketIn {
	replyToken := e.SendCmd(cmdName, cmdParams)
	responseData := <-e.ReplyHandlerQueue[replyToken]

	return responseData
}

// SendCmdSubscribe
func (e *Endpoint) SendCmdSubscribe(topicName string, scope string, returnChan chan DRPPacketIn) {
	tokenID := e.GetToken()
	cmdParams := DRPCmdParams{"topicName": &topicName, "scope": &scope, "streamToken": &tokenID}

	// Queue response chan
	e.ReplyHandlerQueue[tokenID] = returnChan

	// Send subscribe command
	response := e.SendCmdAwait("subscribe", cmdParams)

	if response.Status == 0 {
		delete(e.ReplyHandlerQueue, tokenID)
	}
}

func (e *Endpoint) SendReply(wsConn *websocket.Conn, replyToken *string, returnStatus int, returnPayload interface{}) {
	replyCmd := &DRPReply{}
	replyCmd.Type = "reply"
	replyCmd.Token = replyToken
	replyCmd.Status = returnStatus
	replyCmd.Payload = returnPayload
	e.sendChan <- *replyCmd
}

func (e *Endpoint) SendStream() {
}

func (e *Endpoint) ProcessCmd(wsConn *websocket.Conn, msgIn DRPPacketIn) {
	cmdResults := make(map[string]interface{})
	cmdResults["status"] = 0
	cmdResults["output"] = nil
	if _, ok := e.EndpointCmds[*msgIn.Method]; ok {
		// Execute command
		cmdResults["output"] = e.EndpointCmds[*msgIn.Method](msgIn.Params, wsConn, msgIn.Token)
		cmdResults["status"] = 1
	} else {
		cmdResults["output"] = "Endpoint does not have this method"
	}
	e.SendReply(wsConn, msgIn.Token, cmdResults["status"].(int), cmdResults["output"])
}

func (e *Endpoint) ProcessReply(msgIn DRPPacketIn) {
	e.ReplyHandlerQueue[*msgIn.Token] <- msgIn

	// Add logic to delete from handler queue!
	if msgIn.Status < 2 {
		delete(e.ReplyHandlerQueue, *msgIn.Token)
	}
}

func (e *Endpoint) ReceiveMessage(wsConn *websocket.Conn, msgIn DRPPacketIn) {
	switch msgIn.Type {
	case "cmd":
		e.ProcessCmd(wsConn, msgIn)
	case "reply":
		e.ProcessReply(msgIn)
	}
}

func (e *Endpoint) GetCmds(DRPCmdParams, *websocket.Conn, *string) interface{} {
	keys := make([]string, 0)
	for key := range e.EndpointCmds {
		keys = append(keys, key)
	}
	return keys
}

func (e *Endpoint) OpenHandler() {
}

func (e *Endpoint) CloseHandler() {
}

func (e *Endpoint) ErrorHandler() {
}

// Client
type Client struct {
	Endpoint
	wsTarget string
	user     string
	pass     string
	wsConn   *websocket.Conn
	DoneChan chan bool
}

// Open - Establish session
func (dc *Client) Open(wsTarget string, user string, pass string) error {
	dc.DoneChan = make(chan bool)
	dc.wsTarget = wsTarget
	dc.user = user
	dc.pass = pass

	log.Printf("connecting to %s", dc.wsTarget)

	// Bypass web proxy
	var dialer = websocket.Dialer{
		Subprotocols: []string{"drp"},
		Proxy:        nil,
	}

	// Disable TLS Checking - need to address before production!
	dialer.TLSClientConfig = &tls.Config{InsecureSkipVerify: true}

	//w, _, err := websocket.DefaultDialer.Dial(w.wsTarget, nil)
	w, _, err := dialer.Dial(dc.wsTarget, nil)
	if err != nil {
		log.Fatal("dial:", err)
	}
	//defer w.Close()
	dc.wsConn = w

	done := make(chan struct{})

	dc.RegisterCmd("getCmds", dc.GetCmds)

	// Output Loop
	go func() {
		for {
			select {
			case <-done:
				return
			case sendCmd := <-dc.sendChan:
				sendBytes, marshalErr := json.Marshal(sendCmd)
				if marshalErr != nil {
					log.Println("error marshalling json:", marshalErr)
					//return marshalErr
				} else {
					wsSendErr := w.WriteMessage(websocket.TextMessage, sendBytes)
					if wsSendErr != nil {
						log.Println("error writing message to WS channel:", wsSendErr)
						//return wsSendErr
					}
				}

			}
		}
	}()

	// Input Loop
	go func() {
		for {
			inputJSON := DRPPacketIn{}
			err := dc.wsConn.ReadJSON(&inputJSON)
			if err != nil {
				log.Println("WSCLIENT - Could not parse JSON cmd: ", err)
			} else {
				dc.ReceiveMessage(dc.wsConn, inputJSON)
			}
		}
	}()

	dc.drpNode.

	// Send Hello
	dc.SendCmdAwait("hello", DRPCmdParams{"userAgent": "go", "user": dc.user, "pass": dc.pass})

	// Execute OnOpen
	dc.OpenHandler()

	return nil
}

// DRPPacketIn Message in; could be a Cmd or Reply
type DRPPacketIn struct {
	Type    string       `json:"type"`
	Method  *string      `json:"method"`
	Params  DRPCmdParams `json:"params"`
	Token   *string      `json:"token"`
	Status  int          `json:"status"`
	Payload interface{}  `json:"payload"`
}

type DRPPacket struct {
	Type string `json:"type"`
}

type DRPCmd struct {
	DRPPacket
	Method *string      `json:"method"`
	Params DRPCmdParams `json:"params"`
	Token  *string      `json:"token"`
}

type DRPReply struct {
	DRPPacket
	Token   *string     `json:"token"`
	Status  int         `json:"status"`
	Payload interface{} `json:"payload"`
}
