package drpmesh

// Service is used to define a DRP service
type Service struct {
	ServiceName  string
	drpNode      *Node
	Type         string
	InstanceID   string
	Sticky       bool
	Priority     uint
	Weight       uint
	Zone         string
	Scope        string
	Dependencies []string
	Streams      []string
	Status       int
	ClientCmds   map[string]EndpointMethod
	Classes      map[string]UMLClass
}

// AddClass add a new UMLClass to a Service
func (ds Service) AddClass(newClass UMLClass) {
	ds.Classes[newClass.Name] = newClass
}

// GetDefinition returns information about this Service for discovery
func (ds Service) GetDefinition() ServiceDefinition {
	classList := GetKeys(ds.Classes)
	clientCmdList := GetKeys(ds.ClientCmds)
	returnDef := ServiceDefinition{
		ds.InstanceID,
		ds.ServiceName,
		ds.Type,
		classList,
		clientCmdList,
		ds.Streams,
	}
	return returnDef
}

// PeerBroadcast sends a message to service peers
func (ds Service) PeerBroadcast(method string, params interface{}) {
	// Get list of peer service IDs
	var peerServiceIDList = ds.drpNode.TopologyTracker.FindServicePeers(ds.InstanceID)
	for _, peerServiceID := range peerServiceIDList {
		ds.drpNode.ServiceCmd(ds.ServiceName, method, params, nil, &peerServiceID, false, false, nil)
	}
}

// ServiceDefinition is used for advertising service definitions
type ServiceDefinition struct {
	InstanceID string
	Name       string
	Type       string
	Classes    []string
	ClientCmds []string
	Streams    []string
}
