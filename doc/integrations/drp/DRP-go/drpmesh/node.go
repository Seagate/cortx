package drpmesh

import (
	"encoding/json"
	"fmt"
	"math/rand"
	"os"
	"time"
)

// CreateNode instantiates and returns a new node
func CreateNode(nodeRoles []string, hostID string, domainName string, meshKey string, zone string, scope string, listeningName *string, webServerConfig interface{}, drpRoute *string, debug bool) *Node {
	nodeHostname, _ := os.Hostname()
	nodePID := os.Getpid()

	newNode := &Node{}
	newNode.NodeRoles = nodeRoles
	newNode.HostID = hostID
	newNode.DomainName = domainName
	newNode.meshKey = meshKey
	newNode.Zone = zone
	newNode.Scope = &scope
	newNode.RegistryURL = nil
	newNode.listeningName = listeningName
	newNode.webServerConfig = webServerConfig
	newNode.drpRoute = drpRoute
	newNode.NodeID = fmt.Sprintf("%s-%d", nodeHostname, nodePID)
	newNode.Debug = debug
	newNode.ConnectedToControlPlane = false
	newNode.HasConnectedToMesh = false
	newNode.PacketRelayCount = 0

	newNode.NodeDeclaration = &NodeDeclaration{newNode.NodeID, newNode.NodeRoles, newNode.HostID, newNode.listeningName, newNode.DomainName, newNode.meshKey, newNode.Zone, newNode.Scope}

	newNode.NodeEndpoints = make(map[string]EndpointInterface)
	newNode.ConsumerEndpoints = make(map[string]EndpointInterface)
	newNode.Services = make(map[string]Service)
	newNode.TopologyTracker = &TopologyTracker{}
	newNode.TopologyTracker.Initialize(newNode)

	var localDRPEndpoint = &Endpoint{}
	localDRPEndpoint.Init()
	newNode.ApplyNodeEndpointMethods(localDRPEndpoint)
	var DRPService = Service{"DRP", newNode, "DRP", "", false, 10, 10, newNode.Zone, "local", []string{}, []string{}, 1, localDRPEndpoint.EndpointCmds, nil}
	newNode.AddService(DRPService)

	return newNode
}

// NodeDeclaration objects are traded between Node Endpoints, currently used for mesh auth
type NodeDeclaration struct {
	NodeID     string
	NodeRoles  []string
	HostID     string
	NodeURL    *string
	DomainName string
	MeshKey    string
	Zone       string
	Scope      *string
}

// Node is the base object for DRP operations; service and endpoints are bound to this
type Node struct {
	HostID                  string
	NodeID                  string
	DomainName              string
	meshKey                 string
	Zone                    string
	Scope                   *string
	RegistryURL             *string
	webServerConfig         interface{}
	listeningName           *string
	drpRoute                *string
	NodeRoles               []string
	NodeDeclaration         *NodeDeclaration
	Services                map[string]Service
	TopicManager            interface{}
	TopologyTracker         *TopologyTracker
	NodeEndpoints           map[string]EndpointInterface
	ConsumerEndpoints       map[string]EndpointInterface
	Debug                   bool
	ConnectedToControlPlane bool
	HasConnectedToMesh      bool
	PacketRelayCount        uint
	onControlPlaneConnect   *func()
}

// Log data to console using standard format
func (dn *Node) Log(logMessage string, isDebugMsg bool) {
	if isDebugMsg && !dn.Debug {
		return
	}
	timestamp := dn.GetTimestamp()
	fmt.Printf("%s [%14s] -> %s\n", timestamp, dn.NodeID, logMessage)
}

// GetTimestamp returns the timestamp in a fixed format
func (dn *Node) GetTimestamp() string {
	t := time.Now()
	timestamp := t.Format("20060102150405")
	return timestamp
}

/*
TO DO - IMPLEMENT THESE FUNCTIONS
GetConsumerToken
GetLastTokenForUser
EnableREST
AddSwaggerRouter
ListClassInstances
GetServiceDefinition
GetServiceDefinitions
GetClassRecords
SendPathCmdToNode
GetBaseObj
FindProvidersForStream
EvalPath
GetObjFromPath
VerifyConsumerConnection
*/

// ServiceCmd is used to execute a command against a local or remote Service
func (dn *Node) ServiceCmd(serviceName string, method string, params interface{}, targetNodeID *string, targetServiceInstanceID *string, useControlPlane bool, awaitResponse bool, callingEndpoint EndpointInterface) interface{} {
	thisNode := dn
	baseErrMsg := "ERROR - "

	// If if no service or command is provided, return null
	if serviceName == "" || method == "" {
		return "ServiceCmd: must provide serviceName and method"
	}

	// If no targetNodeID was provided, we need to find a record in the ServiceTable
	if targetNodeID == nil {

		// If no targetServiceInstanceID was provided, we should attempt to locate a service instance

		var targetServiceRecord *ServiceTableEntry = nil

		if targetServiceInstanceID == nil {
			// Update to use the DRP_TopologyTracker object
			targetServiceRecord = thisNode.TopologyTracker.FindInstanceOfService(&serviceName, nil, nil, nil)

			// If no match is found then return null
			if targetServiceRecord == nil {
				return nil
			}

			// Assign target Node & Instance IDs
			targetServiceInstanceID = targetServiceRecord.InstanceID
			targetNodeID = targetServiceRecord.NodeID

			thisNode.Log(fmt.Sprintf("Best instance of service [%s] is [%s] on node [%s]", serviceName, *targetServiceRecord.InstanceID, *targetServiceRecord.NodeID), true)
		} else {
			targetServiceRecord = thisNode.TopologyTracker.ServiceTable.GetEntry(*targetServiceInstanceID).(*ServiceTableEntry)

			// If no match is found then return null
			if targetServiceRecord == nil {
				return nil
			}

			// Assign target Node
			targetNodeID = targetServiceRecord.NodeID
		}
	}

	// We don't have a target NodeID
	if targetNodeID == nil || !thisNode.TopologyTracker.NodeTable.HasEntry(*targetNodeID) {
		return nil
	}

	// Where is the service?
	if *targetNodeID == thisNode.NodeID {
		// Execute locally
		var localServiceProvider map[string]EndpointMethod = nil
		if serviceName == "DRP" && callingEndpoint != nil {
			// If the service is DRP and the caller is a remote endpoint, execute from that caller's EndpointCmds
			localServiceProvider = callingEndpoint.GetEndpointCmds()
		} else {
			if _, ok := thisNode.Services[serviceName]; ok {
				localServiceProvider = thisNode.Services[serviceName].ClientCmds
			}
		}

		if localServiceProvider == nil {
			thisNode.Log(fmt.Sprintf("%s service %s does not exist", baseErrMsg, serviceName), true)
			return nil
		}

		if _, ok := localServiceProvider[method]; !ok {
			thisNode.Log(fmt.Sprintf("%s service %s does not have method %s", baseErrMsg, serviceName, method), true)
			return nil
		}

		if awaitResponse {
			results := localServiceProvider[method](params.(*CmdParams), callingEndpoint, nil)
			return results
		}

		localServiceProvider[method](params.(*CmdParams), callingEndpoint, nil)
		return nil
	}

	// Execute on another Node
	routeNodeID := targetNodeID
	routeOptions := RouteOptions{}

	if !useControlPlane {
		// Make sure either the local Node or remote Node are listening; if not, route via control plane
		localNodeEntry := thisNode.TopologyTracker.NodeTable.GetEntry(thisNode.NodeID).(*NodeTableEntry)
		remoteNodeEntry := thisNode.TopologyTracker.NodeTable.GetEntry(*targetNodeID).(*NodeTableEntry)

		if remoteNodeEntry == nil {
			return fmt.Sprintf("Tried to contact Node[%s], not in NodeTable", *targetNodeID)
		}

		if localNodeEntry.NodeURL == nil && remoteNodeEntry.NodeURL == nil {
			// Neither the local node nor the remote node are listening, use control plane
			useControlPlane = true
		}
	}

	if useControlPlane {
		// We want to use to use the control plane instead of connecting directly to the target
		if thisNode.ConnectedToControlPlane {
			routeNodeID = thisNode.TopologyTracker.GetNextHop(*targetNodeID)
			routeOptions = RouteOptions{&thisNode.NodeID, targetNodeID, []string{}}
		} else {
			// We're not connected to a Registry; fallback to VerifyNodeConnection
			routeNodeID = targetNodeID
		}
	}

	routeNodeConnection := thisNode.VerifyNodeConnection(*routeNodeID)

	if routeNodeConnection == nil {
		errMsg := fmt.Sprintf("Could not establish connection from Node[%s] to Node[%s]", thisNode.NodeID, *routeNodeID)
		thisNode.Log(fmt.Sprintf("ERROR - %s", errMsg), false)
		return errMsg
	}

	if awaitResponse {
		cmdResponse := routeNodeConnection.SendCmdAwait(serviceName, method, params, &routeOptions, targetServiceInstanceID)
		return cmdResponse.Payload
	}
	routeNodeConnection.SendCmd(serviceName, method, params, nil, &routeOptions, targetServiceInstanceID)
	return nil
}

// TCPPingResults contains the TCP ping results to a given host and port
type TCPPingResults struct {
	name     string
	port     uint
	pingInfo TCPPingMetrics
}

// TCPPingMetrics contains the metrics from a TCP ping
type TCPPingMetrics struct {
	min uint
	max uint
	avg uint
}

// PingDomainRegistries returns the SRV records for a domain
func (dn *Node) PingDomainRegistries() map[string]TCPPingResults {
	returnMap := make(map[string]TCPPingResults)

	// Look up the SRV records
	// TO DO - IMPLEMENT TCPPING LOGIC!

	return returnMap
}

// RegistryClientHandler handles connection logic when making an outbound connection to a Registry Node
func (dn *Node) RegistryClientHandler(nodeClient *Client) {
	thisNode := dn
	// Get peer info
	getDeclarationResponse := nodeClient.SendCmdAwait("DRP", "getNodeDeclaration", nil, nil, nil)
	remoteNodeDeclaration := &NodeDeclaration{}
	if getDeclarationResponse != nil && getDeclarationResponse.Payload != nil {
		err := json.Unmarshal(*getDeclarationResponse.Payload, remoteNodeDeclaration)
		if err != nil {
			thisNode.Log(fmt.Sprintf("RegistryClientHandler Payload unmarshal error: %s", err), false)
			return
		}
		registryNodeID := remoteNodeDeclaration.NodeID
		nodeClient.EndpointID = &registryNodeID
		thisNode.NodeEndpoints[registryNodeID] = nodeClient
	} else {
		return
	}

	// Get Registry
	thisNode.TopologyTracker.ProcessNodeConnect(nodeClient, remoteNodeDeclaration, false)
}

// ConnectToRegistry attempts a connection to a specific Registry Node URL
func (dn *Node) ConnectToRegistry(registryURL string, openCallback *func(), closeCallback *func()) {
	retryOnClose := true
	newRegistryClient := &Client{}
	regClientOpenCallback := func() {
		dn.RegistryClientHandler(newRegistryClient)
		if openCallback != nil {
			(*openCallback)()
		}
	}
	if closeCallback != nil {
		retryOnClose = false
	}
	newRegistryClient.Connect(registryURL, nil, dn, nil, retryOnClose, &regClientOpenCallback, closeCallback)
}

// ConnectToRegistryByDomain locates a Registry for a given domain
func (dn *Node) ConnectToRegistryByDomain() {
	// TO DO - IMPLEMENT
}

// ConnectToOtherRegistries locates other Registries for a given domain
func (dn *Node) ConnectToOtherRegistries() {
	// TO DO - IMPLEMENT
}

// ConnectToMesh attempts to locate and connect to a Registry in the Node's domain
func (dn *Node) ConnectToMesh(onControlPlaneConnect func()) {
	thisNode := dn
	if onControlPlaneConnect != nil {
		*thisNode.onControlPlaneConnect = onControlPlaneConnect
	}
	// If this is a Registry, seed the Registry with it's own declaration
	if thisNode.IsRegistry() {
		if thisNode.DomainName != "" {
			// A domain name was provided; attempt to cluster with other registry hosts
			thisNode.Log(fmt.Sprintf("This node is a Registry for %s, attempting to contact other Registry nodes", thisNode.DomainName), false)
			thisNode.ConnectToOtherRegistries()
		}
		if thisNode.onControlPlaneConnect != nil {
			(*thisNode.onControlPlaneConnect)()
		}
	} else {
		if thisNode.RegistryURL != nil {
			// A specific Registry URL was provided
			thisNode.ConnectToRegistry(*thisNode.RegistryURL, nil, nil)
		} else if thisNode.DomainName != "" {
			// A domain name was provided; attempt to connect to a registry host
			thisNode.ConnectToRegistryByDomain()
		} else {
			// No Registry URL or domain provided
			panic("No DomainName or RegistryURL provided!")
		}
	}
}

// ConnectToNode attempts a connection to a specific Registry Node URL
func (dn *Node) ConnectToNode(targetNodeID string, targetURL string) {
	thisNode := dn
	// Initiate Node Connection
	targetEndpoint := thisNode.NodeEndpoints[targetNodeID]
	if targetEndpoint != nil && targetEndpoint.IsConnecting() {
		// We already have this NodeEndpoint registered and the wsConn is opening or open
		thisNode.Log(fmt.Sprintf("Received back request, already have NodeEndpoints[%s]", targetNodeID), true)
	} else {
		thisNode.Log(fmt.Sprintf("Received back request, connecting to [%s] @ %s", targetNodeID, targetURL), true)
		thisNodeEndpoint := &Client{}
		thisNode.NodeEndpoints[targetNodeID] = thisNodeEndpoint
		thisNodeEndpoint.Connect(targetURL, nil, dn, &targetNodeID, false, nil, nil)
	}
}

// ConnectToBroker attempts a connection to a specific Registry Node URL
/*
func (dn *Node) ConnectToBroker(wsTarget string, proxy *string) *Client {
	newBrokerClient := &Client{}
	newBrokerClient.Connect(wsTarget, proxy, dn, "someEndpointID", "Broker")
	return newBrokerClient
}
*/

// AddService registers a new Service object to the local Node
func (dn *Node) AddService(serviceObj Service) {
	thisNode := dn

	newInstanceID := fmt.Sprintf("%s-%s-%d", dn.NodeID, serviceObj.ServiceName, rand.Intn(9999))
	serviceObj.InstanceID = newInstanceID

	thisNode.Services[serviceObj.ServiceName] = serviceObj

	newServiceEntry := ServiceTableEntry{}
	newServiceEntry.NodeID = &thisNode.NodeID
	newServiceEntry.ProxyNodeID = nil
	newServiceEntry.Scope = &serviceObj.Scope
	newServiceEntry.Zone = &serviceObj.Zone
	newServiceEntry.LearnedFrom = nil
	newServiceEntry.LastModified = nil
	newServiceEntry.Name = &serviceObj.ServiceName
	newServiceEntry.Type = &serviceObj.Type
	newServiceEntry.InstanceID = &serviceObj.InstanceID
	newServiceEntry.Sticky = serviceObj.Sticky
	newServiceEntry.Priority = serviceObj.Priority
	newServiceEntry.Weight = serviceObj.Weight
	newServiceEntry.Dependencies = serviceObj.Dependencies
	newServiceEntry.Streams = serviceObj.Streams
	newServiceEntry.Status = serviceObj.Status

	addServicePacket := TopologyPacket{thisNode.NodeID, "add", "service", *newServiceEntry.InstanceID, *newServiceEntry.Scope, *newServiceEntry.Zone, newServiceEntry.ToJSON()}
	thisNode.TopologyTracker.ProcessPacket(addServicePacket, thisNode.NodeID, false)
}

// RemoveService TO DO - IMPLEMENT
func (dn *Node) RemoveService() {}

// ApplyGenericEndpointMethods applies a mandatory set of methods to an Endpoint
// TO DO - REGISTER METHODS AS FUNCTIONS ARE PORTED
func (dn *Node) ApplyGenericEndpointMethods(targetEndpoint EndpointInterface) {
	thisNode := dn
	//type EndpointMethod func(*CmdParams, *websocket.Conn, *int) interface{}
	targetEndpoint.RegisterMethod("getEndpointID", func(params *CmdParams, callingEndpoint EndpointInterface, token *int) interface{} {
		return targetEndpoint.GetID()
	})

	targetEndpoint.RegisterMethod("getNodeDeclaration", func(params *CmdParams, callingEndpoint EndpointInterface, token *int) interface{} {
		return thisNode.NodeDeclaration
	})
	/*
		targetEndpoint.RegisterMethod("pathCmd", async (params, srcEndpoint, token) => {
			return await thisNode.GetObjFromPath(params, thisNode.GetBaseObj(), srcEndpoint);
		});
	*/
	targetEndpoint.RegisterMethod("getRegistry", func(params *CmdParams, callingEndpoint EndpointInterface, token *int) interface{} {
		var reqNodeID *string = nil
		if params != nil {
			valueJSON := (*params)["reqNodeID"]
			if valueJSON != nil {
				json.Unmarshal(*valueJSON, reqNodeID)
			}
		}
		return thisNode.TopologyTracker.GetRegistry(reqNodeID)
	})
	/*
		targetEndpoint.RegisterMethod("getServiceDefinition", (...args) => {
			return thisNode.GetServiceDefinition(...args);
		});

		targetEndpoint.RegisterMethod("getServiceDefinitions", async function (...args) {
			return await thisNode.GetServiceDefinitions(...args);
		});
	*/
	targetEndpoint.RegisterMethod("getLocalServiceDefinitions", func(params *CmdParams, callingEndpoint EndpointInterface, token *int) interface{} {
		var serviceName *string = nil
		if params != nil {
			valueJSON := (*params)["serviceName"]
			if valueJSON != nil {
				json.Unmarshal(*valueJSON, serviceName)
			}
		}
		var clientConnectionData = thisNode.GetLocalServiceDefinitions(serviceName)
		return clientConnectionData
	})

	/*

		targetEndpoint.RegisterMethod("getClassRecords", async (...args) => {
			return await thisNode.GetClassRecords(...args);
		});

		targetEndpoint.RegisterMethod("listClassInstances", async (...args) => {
			return await thisNode.ListClassInstances(...args);
		});

		targetEndpoint.RegisterMethod("sendToTopic", function (params, srcEndpoint, token) {
			thisNode.TopicManager.SendToTopic(params.topicName, params.topicData);
		});

		targetEndpoint.RegisterMethod("getTopology", async function (...args) {
			return await thisNode.GetTopology(...args);
		});
	*/
	targetEndpoint.RegisterMethod("listClientConnections", func(params *CmdParams, callingEndpoint EndpointInterface, token *int) interface{} {
		var clientConnectionData = thisNode.ListClientConnections()
		return clientConnectionData
	})
	/*
		targetEndpoint.RegisterMethod("tcpPing", async (...args) => {
			return thisNode.TCPPing(...args);
		});

		targetEndpoint.RegisterMethod("findInstanceOfService", async (params) => {
			return thisNode.TopologyTracker.FindInstanceOfService(params.serviceName, params.serviceType, params.zone);
		});

		targetEndpoint.RegisterMethod("listServices", async (params) => {
			return thisNode.TopologyTracker.ListServices(params.serviceName, params.serviceType, params.zone);
		});

		targetEndpoint.RegisterMethod("subscribe", async function (params, srcEndpoint, token) {
			// Only allow if the scope is local or this Node is a Broker
			if (params.scope !== "local" && !thisNode.IsBroker()) return null;

			let sendFunction = async (message) => {
				// Returns send status; error if not null
				return await srcEndpoint.SendReply(params.streamToken, 2, message);
			};
			let sendFailCallback = async (sendFailMsg) => {
				// Failed to send; may have already disconnected, take no further action
			};
			let thisSubscription = new DRP_Subscriber(params.topicName, params.scope, params.filter, sendFunction, sendFailCallback);
			srcEndpoint.Subscriptions[params.streamToken] = thisSubscription;
			return await thisNode.Subscribe(thisSubscription);
		});

		targetEndpoint.RegisterMethod("unsubscribe", async function (params, srcEndpoint, token) {
			let response = false;
			let thisSubscription = srcEndpoint.Subscriptions[params.streamToken];
			if (thisSubscription) {
				thisSubscription.Terminate();
				thisNode.SubscriptionManager.Subscribers.delete(thisSubscription);
				response = true;
			}
			return response;
		});

		targetEndpoint.RegisterMethod("refreshSwaggerRouter", async function (params, srcEndpoint, token) {
			let serviceName = null;
			if (params && params.serviceName) {
				// params was passed from cliGetPath
				serviceName = params.serviceName;

			} else if (params && params.pathList && params.pathList.length > 0) {
				// params was passed from cliGetPath
				serviceName = params.pathList.shift();
			} else {
				if (params && params.pathList) return `Format \\refreshSwaggerRouter\\{serviceName}`;
				else return `FAIL - serviceName not defined`;
			}

			if (thisNode.SwaggerRouters[serviceName]) {
				delete thisNode.SwaggerRouters[serviceName];
				let serviceInstance = thisNode.TopologyTracker.FindInstanceOfService(serviceName);
				if (!serviceInstance) return `FAIL - Service [${serviceName}] does not exist`;
				await thisNode.AddSwaggerRouter(serviceName, serviceInstance.NodeID);
				return `OK - Refreshed SwaggerRouters[${serviceName}]`;
			} else {
				return `FAIL - SwaggerRouters[${serviceName}] does not exist`;
			}
		});
	*/
}

// ApplyNodeEndpointMethods applies a set of methods to an Endpoint if the peer is a Node
// TO DO - REGISTER METHODS AS FUNCTIONS ARE PORTED
func (dn *Node) ApplyNodeEndpointMethods(targetEndpoint EndpointInterface) {
	thisNode := dn

	thisNode.ApplyGenericEndpointMethods(targetEndpoint)
	/*
		targetEndpoint.RegisterMethod("topologyUpdate", async function (...args) {
			return thisNode.TopologyUpdate(...args);
		});
	*/
	targetEndpoint.RegisterMethod("connectToNode", func(params *CmdParams, callingEndpoint EndpointInterface, token *int) interface{} {
		var targetNodeID *string = nil
		var targetURL *string = nil
		if params != nil {
			targetNodeIDJSON := (*params)["targetNodeID"]
			if targetNodeIDJSON != nil {
				json.Unmarshal(*targetNodeIDJSON, targetNodeID)
			}
			targetURLJSON := (*params)["targetURL"]
			if targetURLJSON != nil {
				json.Unmarshal(*targetURLJSON, targetURL)
			}
		}
		thisNode.ConnectToNode(*targetNodeID, *targetURL)
		return nil
	})
	/*
		targetEndpoint.RegisterMethod("addConsumerToken", async function (params, srcEndpoint, token) {
			if (params.tokenPacket) {
				thisNode.ConsumerTokens[params.tokenPacket.Token] = params.tokenPacket;
			}
			return;
		});

		if (targetEndpoint.IsServer && !targetEndpoint.IsServer()) {
			// Add this command for DRP_Client endpoints
			targetEndpoint.RegisterMethod("connectToRegistryInList", async function (...args) {
				return await thisNode.ConnectToRegistryInList(...args);
			});
		}
	*/
}

// RawMessageToString converts a json.RawMessage ([]byte) to a string for debug output
func (dn *Node) RawMessageToString(rawMessage *json.RawMessage) *string {
	j, err := json.Marshal(rawMessage)
	if err != nil {
		return nil
	}
	jsonString := string(j)
	return &jsonString
}

// IsRegistry tells whether or not the local Node holds the Registry role
func (dn *Node) IsRegistry() bool {
	for _, a := range dn.NodeRoles {
		if a == "Registry" {
			return true
		}
	}
	return false
}

// IsBroker tells whether or not the local Node hold the Broker role
func (dn *Node) IsBroker() bool {
	for _, a := range dn.NodeRoles {
		if a == "Broker" {
			return true
		}
	}
	return false
}

// IsConnectedTo tells whether or not the local Node is directly connected to another Node
func (dn *Node) IsConnectedTo(checkNodeID string) bool {
	_, ok := dn.NodeEndpoints[checkNodeID]
	return ok
}

// ListClientConnections tells whether or not the local Node hold the Broker role
func (dn *Node) ListClientConnections() map[string]map[string]interface{} {
	nodeClientConnections := make(map[string]map[string]interface{})

	nodeClientConnections["nodeClients"] = make(map[string]interface{})
	nodeClientConnections["consumerClients"] = make(map[string]interface{})

	// Loop over Node Endpoints
	for nodeID, thisEndpoint := range dn.NodeEndpoints {
		if thisEndpoint.IsServer() {
			nodeClientConnections["nodeClients"][nodeID] = thisEndpoint.ConnectionStats()
		}
	}

	// Loop over Client Endpoints
	for consumerID, thisEndpoint := range dn.ConsumerEndpoints {
		nodeClientConnections["consumerClients"][consumerID] = thisEndpoint.ConnectionStats()
	}

	return nodeClientConnections
}

// GetLocalServiceDefinitions return local service definitions
func (dn *Node) GetLocalServiceDefinitions(checkServiceName *string) map[string]ServiceDefinition {
	serviceDefinitions := make(map[string]ServiceDefinition)

	for serviceName, localServiceObj := range dn.Services {
		if serviceName == "DRP" || checkServiceName != nil && *checkServiceName != serviceName {
			continue
		}
		serviceDefinition := localServiceObj.GetDefinition()
		serviceDefinitions[serviceName] = serviceDefinition
	}

	return serviceDefinitions
}

// VerifyNodeConnection tells whether or not the local Node hold the Broker role
func (dn *Node) VerifyNodeConnection(remoteNodeID string) EndpointInterface {

	thisNode := dn

	thisNodeEntry := thisNode.TopologyTracker.NodeTable.GetEntry(remoteNodeID).(*NodeTableEntry)
	if thisNodeEntry == nil {
		return nil
	}

	thisNodeEndpoint := thisNode.NodeEndpoints[remoteNodeID]

	// Is the remote node listening?  If so, try to connect
	if thisNodeEndpoint == nil && thisNodeEntry.NodeURL != nil {
		targetNodeURL := thisNodeEntry.NodeURL

		// We have a target URL, wait a few seconds for connection to initiate
		thisNode.Log(fmt.Sprintf("Connecting to Node [%s] @ '%s'", remoteNodeID, *targetNodeURL), true)

		thisNodeEndpoint := &Client{}
		thisNodeEndpoint.Connect(*targetNodeURL, nil, dn, &remoteNodeID, false, nil, nil)

		thisNode.NodeEndpoints[remoteNodeID] = thisNodeEndpoint

		for i := 0; i < 50; i++ {

			// Are we still trying?
			if thisNodeEndpoint.IsConnecting() {
				// Yes - wait
				time.Sleep(100 * time.Millisecond)
			} else {
				// No - break the for loop
				break
			}
		}
	}

	// If this node is listening, try sending a back connection request to the remote node via the registry
	if (thisNodeEndpoint != nil || !thisNodeEndpoint.IsReady()) && thisNode.listeningName != nil {

		thisNode.Log("Sending back request...", true)
		// Let's try having the Provider call us; send command through Registry

		// Get next hop
		nextHopNodeID := thisNode.TopologyTracker.GetNextHop(remoteNodeID)

		if nextHopNodeID != nil {
			// Found the next hop
			thisNode.Log(fmt.Sprintf("Sending back request to %s, relaying to [%s]", remoteNodeID, *nextHopNodeID), true)
			routeOptions := RouteOptions{
				&thisNode.NodeID,
				&remoteNodeID,
				[]string{},
			}
			cmdParams := make(map[string]string)
			cmdParams["targetNodeID"] = thisNode.NodeID
			cmdParams["targetURL"] = *thisNode.listeningName
			thisNode.NodeEndpoints[*nextHopNodeID].SendCmd("DRP", "connectToNode", cmdParams, nil, &routeOptions, nil)
		} else {
			// Could not find the next hop
			thisNode.Log(fmt.Sprintf("Could not find next hop to [%s]", remoteNodeID), false)
		}

		thisNode.Log("Starting wait...", true)
		// Wait a few seconds
		for i := 0; i < 50; i++ {

			// Are we still trying?
			newEndpoint, ok := thisNode.NodeEndpoints[remoteNodeID]
			if !ok || !newEndpoint.IsReady() {
				// Yes - wait
				time.Sleep(100 * time.Millisecond)
			} else {
				// No - break the for loop
				thisNode.Log(fmt.Sprintf("Received back connection from remote node [%s]", remoteNodeID), true)
				i = 50
			}
		}

		// If still not successful, delete DRP_NodeClient
		newEndpoint, ok := thisNode.NodeEndpoints[remoteNodeID]
		if !ok || !newEndpoint.IsReady() {
			thisNode.Log(fmt.Sprintf("Could not open connection to Node [%s]", remoteNodeID), true)
			if ok {
				delete(thisNode.NodeEndpoints, remoteNodeID)
			}
			//throw new Error(`Could not get connection to Provider ${remoteNodeID}`);
		} else {
			thisNodeEndpoint = thisNode.NodeEndpoints[remoteNodeID]
		}
	}

	return thisNodeEndpoint
}
