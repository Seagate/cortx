package drpmesh

import (
	"encoding/json"
	"fmt"

	"github.com/gorilla/websocket"
)

// ConnectionStats provides latency and uptime stats
type ConnectionStats struct {
	pingTimeMs    int
	uptimeSeconds int
}

// EndpointMethod defines the interface for a DRP Endpoint method
type EndpointMethod func(*CmdParams, EndpointInterface, *int) interface{}

// EndpointAuthInfo tracks the auth info provided by a remote Node
type EndpointAuthInfo struct {
	Type     string
	Value    string
	UserInfo interface{}
}

// EndpointInterface declares the set of functions that should be implemented for any Endpoint object
type EndpointInterface interface {
	GetID() *string
	GetType() string
	GetToken() int
	AddReplyHandler() int
	DeleteReplyHandler(int)
	RegisterMethod(string, EndpointMethod)
	SendPacketBytes([]byte)
	SendCmd(string, string, interface{}, *int, *RouteOptions, *string)
	SendCmdAwait(string, string, interface{}, *RouteOptions, *string) *ReplyIn
	IsServer() bool
	ConnectionStats() ConnectionStats
	GetEndpointCmds() map[string]EndpointMethod
	IsReady() bool
	IsConnecting() bool
}

// Endpoint - DRP endpoint
type Endpoint struct {
	wsConn            *websocket.Conn
	drpNode           *Node
	EndpointID        *string
	EndpointType      string
	EndpointCmds      map[string]EndpointMethod
	AuthInfo          EndpointAuthInfo
	ReplyHandlerQueue map[int](chan *ReplyIn)
	TokenNum          int
	Subscriptions     interface{}
	openCallback      *func()
	closeCallback     *func()
	closeChan         chan bool

	//sendChan chan interface{}
}

// Init initializes Endpoint attributes
func (e *Endpoint) Init() {
	e.EndpointCmds = make(map[string]EndpointMethod)
	e.ReplyHandlerQueue = make(map[int](chan *ReplyIn))
	e.closeChan = make(chan bool)
	//e.sendChan = make(chan interface{}, 100)
	e.TokenNum = 1
}

// GetID returns the ID of the Endpoint
func (e *Endpoint) GetID() *string {
	return e.EndpointID
}

// GetType returns the type of the Endpoint
func (e *Endpoint) GetType() string {
	return e.EndpointType
}

// GetToken returns the next token to be used for the Endpoint
func (e *Endpoint) GetToken() int {
	//returnToken := strconv.Itoa(e.TokenNum)
	returnToken := e.TokenNum
	e.TokenNum++
	return returnToken
}

// AddReplyHandler is used to track responses for a command
func (e *Endpoint) AddReplyHandler() int {
	replyToken := e.GetToken()
	e.ReplyHandlerQueue[replyToken] = make(chan *ReplyIn)
	return replyToken
}

// DeleteReplyHandler removes a reply handler
func (e *Endpoint) DeleteReplyHandler(handlerToken int) {
	delete(e.ReplyHandlerQueue, handlerToken)
	return
}

// RegisterMethod adds a command that is allowed to be executed by the remote Endpoint
func (e *Endpoint) RegisterMethod(methodName string, method EndpointMethod) {
	e.EndpointCmds[methodName] = method
}

// SendPacketBytes abstracts sending functions from the communication channel functions
func (e *Endpoint) SendPacketBytes(drpPacketBytes []byte) {
	// This function listend to e.sendChan and sends commands
	wsSendErr := e.wsConn.WriteMessage(websocket.TextMessage, drpPacketBytes)
	if wsSendErr != nil {
		e.drpNode.Log(fmt.Sprint("error writing message to WS channel:", wsSendErr), false)
		//return wsSendErr
	}
}

// SendCmd sends a command to a remote Endpoint
func (e *Endpoint) SendCmd(serviceName string, methodName string, cmdParams interface{}, token *int, routeOptions *RouteOptions, serviceInstanceID *string) {
	sendCmd := &CmdOut{}
	sendCmd.ServiceName = &serviceName
	sendCmd.Type = "cmd"
	sendCmd.Method = &methodName
	sendCmd.Params = cmdParams
	sendCmd.Token = token
	sendCmd.RouteOptions = routeOptions
	sendCmd.ServiceInstanceID = serviceInstanceID

	packetBytes := sendCmd.ToJSON()
	e.SendPacketBytes(packetBytes)
	//e.sendChan <- *sendCmd
}

// SendCmdAwait sends a command to a remote Endpoint and awaits a response
func (e *Endpoint) SendCmdAwait(serviceName string, cmdName string, cmdParams interface{}, routeOptions *RouteOptions, serviceInstanceID *string) *ReplyIn {
	replyToken := e.AddReplyHandler()
	e.SendCmd(serviceName, cmdName, cmdParams, &replyToken, routeOptions, serviceInstanceID)
	responseData := <-e.ReplyHandlerQueue[replyToken]

	return responseData
}

// SendReply returns data to a remote Endpoint which originally executed a command
func (e *Endpoint) SendReply(replyToken *int, returnStatus int, returnPayload interface{}) {
	replyCmd := &ReplyOut{}
	replyCmd.Type = "reply"
	replyCmd.Token = replyToken
	replyCmd.Status = returnStatus
	replyCmd.Payload = returnPayload

	packetBytes := replyCmd.ToJSON()
	e.drpNode.Log(fmt.Sprintf("SendReply -> %s", string(packetBytes)), true)
	e.SendPacketBytes(packetBytes)
	//e.sendChan <- *replyCmd
}

// ProcessCmd processes an inbound packet as a Cmd
func (e *Endpoint) ProcessCmd(msgIn *Cmd) {
	cmdResults := make(map[string]interface{})
	cmdResults["status"] = 0
	cmdResults["output"] = nil
	if _, ok := e.EndpointCmds[*msgIn.Method]; ok {
		// Execute command
		e.drpNode.Log(fmt.Sprintf("Executing method '%s'...", *msgIn.Method), true)
		cmdResults["output"] = e.EndpointCmds[*msgIn.Method](msgIn.Params, e, msgIn.Token)
		cmdResults["status"] = 1
	} else {
		cmdResults["output"] = "Endpoint does not have this method"
		e.drpNode.Log(fmt.Sprintf("Remote Endpoint tried to execute unrecognized method '%s'", *msgIn.Method), true)
	}
	e.SendReply(msgIn.Token, cmdResults["status"].(int), cmdResults["output"])
}

// ProcessReply processes an inbound packet as a Reply
func (e *Endpoint) ProcessReply(msgIn *ReplyIn) {
	e.ReplyHandlerQueue[*msgIn.Token] <- msgIn

	// If the receive is complete, delete handler
	if msgIn.Status < 2 {
		e.DeleteReplyHandler(*msgIn.Token)
	}
}

// ShouldRelay determines whether or not an inbound packet should be relayed
func (e *Endpoint) ShouldRelay(msgIn *PacketIn) bool {
	var shouldForward = false
	if msgIn.RouteOptions != nil && msgIn.RouteOptions.TgtNodeID != nil && *msgIn.RouteOptions.TgtNodeID != e.drpNode.NodeID {
		shouldForward = true
	}
	return shouldForward
}

// ReceiveMessage determines whether a packet should be relayed or processed locally
func (e *Endpoint) ReceiveMessage(rawMessage []byte) {

	packetIn := &PacketIn{}
	err := json.Unmarshal(rawMessage, packetIn)
	if err != nil {
		e.drpNode.Log(fmt.Sprintf("ReceiveMessage Packet unmarshal error: %s", err), false)
		return
	}

	if e.ShouldRelay(packetIn) {
		e.RelayPacket(packetIn)
		return
	}

	switch packetIn.Type {
	case "cmd":
		cmdPacket := &Cmd{}
		err := json.Unmarshal(rawMessage, cmdPacket)
		if err != nil {
			e.drpNode.Log(fmt.Sprintf("ReceiveMessage Cmd unmarshal error: %s", err), false)
			return
		}
		e.ProcessCmd(cmdPacket)
	case "reply":
		replyPacket := &ReplyIn{}
		err := json.Unmarshal(rawMessage, replyPacket)
		if err != nil {
			e.drpNode.Log(fmt.Sprintf("ReceiveMessage Reply unmarshal error: %s", err), false)
			return
		}
		e.ProcessReply(replyPacket)
	}
}

// RelayPacket routes a packet to another Node
func (e *Endpoint) RelayPacket(packetIn *PacketIn) {
	thisEndpoint := e
	var errMsg *string = nil

	// Validate sending endpoint
	if thisEndpoint.EndpointID == nil {

		// Sending endpoint has not authenticated
		tmpErr := "sending endpoint has not authenticated"
		errMsg = &tmpErr

		// Validate source node
	} else if !thisEndpoint.drpNode.TopologyTracker.ValidateNodeID(*packetIn.RouteOptions.SrcNodeID) {

		// Source NodeID is invalid
		tmpErr := fmt.Sprintf("srcNodeID %s not found", *packetIn.RouteOptions.SrcNodeID)
		errMsg = &tmpErr

		// Validate destination node
	} else if !thisEndpoint.drpNode.TopologyTracker.ValidateNodeID(*packetIn.RouteOptions.TgtNodeID) {
		// Target NodeID is invalid
		tmpErr := fmt.Sprintf("tgtNodeID %s not found", *packetIn.RouteOptions.TgtNodeID)
		errMsg = &tmpErr
	}

	if errMsg != nil {
		thisEndpoint.drpNode.Log(fmt.Sprintf("Could not relay message: %s", *errMsg), false)
		return
	}

	nextHopNodeID := thisEndpoint.drpNode.TopologyTracker.GetNextHop(*packetIn.RouteOptions.TgtNodeID)
	targetNodeEndpoint := thisEndpoint.drpNode.VerifyNodeConnection(*nextHopNodeID)

	// Add this node to the routing history
	packetIn.RouteOptions.RouteHistory = append(packetIn.RouteOptions.RouteHistory, thisEndpoint.drpNode.NodeID)

	// Repackage
	packetBytesOut := []byte{}
	switch packetIn.Type {
	case "cmd":
		cmdPacket := &Cmd{
			BasePacket{
				packetIn.Type,
				packetIn.RouteOptions,
				packetIn.Token,
			},
			packetIn.Method,
			packetIn.Params,
			packetIn.ServiceName,
			packetIn.ServiceInstanceID,
		}
		packetBytesOut = cmdPacket.ToJSON()
	case "reply":
		replyPacket := &ReplyIn{
			BasePacket{
				packetIn.Type,
				packetIn.RouteOptions,
				packetIn.Token,
			},
			packetIn.Status,
			packetIn.Payload,
		}
		packetBytesOut = replyPacket.ToJSON()
	}

	// Send packet to next hop
	targetNodeEndpoint.SendPacketBytes(packetBytesOut)

	// Increment local Node's PacketRelayCount
	thisEndpoint.drpNode.PacketRelayCount++

	return
}

// GetCmds returns the list of method available for the peer Endpoint to execute
func (e *Endpoint) GetCmds() interface{} {
	keys := make([]string, 0)
	for key := range e.EndpointCmds {
		keys = append(keys, key)
	}
	return keys
}

// OpenHandler specifies actions to be taken after a connection is opened
func (e *Endpoint) OpenHandler() {
}

// CloseHandler specifies actions to be taken after a connection is closed
func (e *Endpoint) CloseHandler(code int, text string) error {
	e.closeChan <- true
	return nil
}

// ErrorHandler specifies actions to be taken after a connection encounters an error
func (e *Endpoint) ErrorHandler() {
}

// StartListening begins listening loop on wsConn
func (e *Endpoint) StartListening(wsConn *websocket.Conn) {
	e.wsConn = wsConn
	e.wsConn.SetCloseHandler(e.CloseHandler)

	// Start receive loop
	go func() {
		for {
			_, p, err := e.wsConn.ReadMessage()
			if err != nil {
				e.drpNode.Log(fmt.Sprintf("Could not read from wsConn: %s", err), true)
				break
			} else {
				// Need to update this so the receive loop isn't tied up processing inbound commands
				e.ReceiveMessage(p)
			}
		}
	}()
}

// IsServer tells whether or not this endpoint is the server side of the connection
func (e *Endpoint) IsServer() bool {
	return false
}

// PingTime TO DO - IMPLEMENT
func (e *Endpoint) PingTime() int {
	return 0
}

// UpTime TO DO - IMPLEMENT
func (e *Endpoint) UpTime() int {
	return 0
}

// IsReady tells whether or not the Endpoint's socket connection is ready to communicate
// TO DO - IMPLEMENT
func (e *Endpoint) IsReady() bool {
	return true
}

// IsConnecting tells whether or not the Endpoint's socket connection is attempting to establish a connection
// TO DO - IMPLEMENT
func (e *Endpoint) IsConnecting() bool {
	return false
}

// ConnectionStats returns uptime and latency info
func (e *Endpoint) ConnectionStats() ConnectionStats {
	return ConnectionStats{
		e.PingTime(),
		e.UpTime(),
	}
}

// GetEndpointCmds returns the commands available to be executed on this endpoint
func (e *Endpoint) GetEndpointCmds() map[string]EndpointMethod {
	return e.EndpointCmds
}
