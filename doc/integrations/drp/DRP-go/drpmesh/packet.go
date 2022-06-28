package drpmesh

import (
	"encoding/json"
)

// CreateCmd returns a Cmd object
func CreateCmd(method *string, params *CmdParams, serviceName *string, token *int, serviceInstanceID *string, routeOptions *RouteOptions) *Cmd {
	drpCmd := &Cmd{}
	drpCmd.Type = "cmd"
	drpCmd.RouteOptions = routeOptions
	drpCmd.Token = token
	drpCmd.Method = method
	drpCmd.Params = params
	drpCmd.ServiceName = serviceName
	drpCmd.ServiceInstanceID = serviceInstanceID
	return drpCmd
}

// CreateReply returns a Reply object
func CreateReply(status int, payload interface{}, token *int, routeOptions *RouteOptions) *ReplyOut {
	drpReply := &ReplyOut{}
	drpReply.Type = "reply"
	drpReply.RouteOptions = routeOptions
	drpReply.Token = token
	drpReply.Status = status
	drpReply.Payload = payload
	return drpReply
}

// BasePacket describes the base attributes common in all DRP packets
type BasePacket struct {
	Type         string        `json:"type"`
	RouteOptions *RouteOptions `json:"routeOptions"`
	Token        *int          `json:"token"`
}

// PacketIn includes all possible attributes necessary to unmarshal inbound packets
type PacketIn struct {
	BasePacket
	Method            *string          `json:"method"`
	Params            *CmdParams       `json:"params"`
	ServiceName       *string          `json:"serviceName"`
	ServiceInstanceID *string          `json:"serviceInstanceID"`
	Status            int              `json:"status"`
	Payload           *json.RawMessage `json:"payload"`
}

// Cmd is a DRP packet sent when issuing a command
type Cmd struct {
	BasePacket
	Method            *string    `json:"method"`
	Params            *CmdParams `json:"params"`
	ServiceName       *string    `json:"serviceName"`
	ServiceInstanceID *string    `json:"serviceInstanceID"`
}

// CmdParams - DRP Cmd parameters
type CmdParams map[string]*json.RawMessage

// ToJSON converts the packet to a JSON byte array
func (dc *Cmd) ToJSON() []byte {
	buff, _ := json.Marshal(dc)
	return buff
}

// CmdOut is a DRP packet sent when issuing a command
type CmdOut struct {
	BasePacket
	Method            *string     `json:"method"`
	Params            interface{} `json:"params"`
	ServiceName       *string     `json:"serviceName"`
	ServiceInstanceID *string     `json:"serviceInstanceID"`
}

// ToJSON converts the packet to a JSON byte array
func (dc *CmdOut) ToJSON() []byte {
	buff, _ := json.Marshal(dc)
	return buff
}

// ReplyOut is a DRP packet sent when replying to a command
type ReplyOut struct {
	BasePacket
	Status  int         `json:"status"`
	Payload interface{} `json:"payload"`
}

// ToJSON converts the packet to a JSON byte array
func (dr *ReplyOut) ToJSON() []byte {
	buff, _ := json.Marshal(dr)
	return buff
}

// ReplyIn is used to unmarshal Reply packets we get back after sending a command
type ReplyIn struct {
	BasePacket
	Status  int              `json:"status"`
	Payload *json.RawMessage `json:"payload"`
}

// ToJSON converts the packet to a JSON byte array
func (dri *ReplyIn) ToJSON() []byte {
	buff, _ := json.Marshal(dri)
	return buff
}

// RouteOptions is an optional Packet parameter used to take advantage of control plane routing
type RouteOptions struct {
	SrcNodeID    *string  `json:"srcNodeID"`
	TgtNodeID    *string  `json:"tgtNodeID"`
	RouteHistory []string `json:"routeHistory"`
}
