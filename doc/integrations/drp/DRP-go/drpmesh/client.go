package drpmesh

import (
	"crypto/tls"
	"fmt"
	"log"

	"github.com/gorilla/websocket"
)

// Client module is used to establish outbound connection to a Node
type Client struct {
	Endpoint
	wsTarget string
	proxy    string
}

// Connect makes an outbound connection to a Node
func (dc *Client) Connect(wsTarget string, proxy *string, drpNode *Node, endpointID *string, retryOnClose bool, openCallback *func(), closeCallback *func()) {
	dc.Init()
	dc.wsConn = nil
	dc.wsTarget = wsTarget
	dc.drpNode = drpNode
	dc.EndpointID = endpointID
	dc.EndpointType = "Node"

	dc.openCallback = openCallback
	dc.closeCallback = closeCallback

	// TODO - UPDATE CLIENT CONNECT LOGIC COPIED FROM drpclient.go
	dc.drpNode.Log(fmt.Sprintf("connecting to %s", dc.wsTarget), false)

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

	drpNode.ApplyNodeEndpointMethods(dc)

	dc.StartListening(w)
	//fmt.Printf("%+v\n", responsePacket)

	w.SetCloseHandler(func(code int, text string) error {
		if closeCallback != nil {
			(*closeCallback)()
		}
		return nil
	})

	dc.drpNode.Log("Sending hello...", false)
	//responsePacket := dc.SendCmdAwait("DRP", "hello", &CmdParams{"userAgent": "go", "user": "Gopher", "pass": "supersecret"})
	responsePacket := dc.SendCmdAwait("DRP", "hello", dc.drpNode.NodeDeclaration, nil, nil)
	dc.drpNode.Log("Received response from hello", false)
	dc.drpNode.Log(string(responsePacket.ToJSON()), false)

	if openCallback != nil {
		(*openCallback)()
	}
}

// RetryConnection implements retry logic
func (dc *Client) RetryConnection() {
	return
}

// IsServer tells whether or not this endpoint is the server side of the connection
func (dc *Client) IsServer() bool {
	return false
}
