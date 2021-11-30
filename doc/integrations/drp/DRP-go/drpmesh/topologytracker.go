package drpmesh

import (
	"encoding/json"
	"fmt"
	"math/rand"
	"strings"
	"time"

	wr "github.com/mroth/weightedrand"
)

// TopologyTracker keeps track of Nodes and Services in the mesh
type TopologyTracker struct {
	drpNode      *Node
	NodeTable    *NodeTable
	ServiceTable *ServiceTable
}

// Initialize creates the node and service tables
func (tt *TopologyTracker) Initialize(drpNode *Node) {
	tt.drpNode = drpNode
	tt.NodeTable = &NodeTable{}
	tt.ServiceTable = &ServiceTable{}

	currentTimestamp := tt.drpNode.GetTimestamp()

	newNodeEntry := NodeTableEntry{}
	newNodeEntry.NodeID = &tt.drpNode.NodeID
	newNodeEntry.ProxyNodeID = nil
	newNodeEntry.Scope = tt.drpNode.Scope
	newNodeEntry.Zone = &tt.drpNode.Zone
	newNodeEntry.LearnedFrom = &tt.drpNode.NodeID
	newNodeEntry.LastModified = &currentTimestamp
	newNodeEntry.Roles = tt.drpNode.NodeRoles
	newNodeEntry.NodeURL = tt.drpNode.listeningName
	newNodeEntry.HostID = &tt.drpNode.HostID

	addNodePacket := TopologyPacket{*newNodeEntry.NodeID, "add", "node", *newNodeEntry.NodeID, *newNodeEntry.Scope, *newNodeEntry.Zone, newNodeEntry.ToJSON()}
	tt.ProcessPacket(addNodePacket, tt.drpNode.NodeID, tt.drpNode.IsRegistry())
}

// ProcessPacket handles DRP topology packets
func (tt *TopologyTracker) ProcessPacket(topologyPacket TopologyPacket, srcNodeID string, sourceIsRegistry bool) {
	thisTopologyTracker := tt
	thisNode := thisTopologyTracker.drpNode
	var targetTable TopologyTable = nil

	// Flag for relaying
	doRelay := false

	// Base TopologyTableEntry from TopologyPacket (excludes node/service attributes)
	topologyPacketData := &TopologyTableEntry{}
	marshalErr := json.Unmarshal(topologyPacket.Data, topologyPacketData)
	if marshalErr != nil {
		thisNode.Log(fmt.Sprintf("error marshalling json: %s", marshalErr), true)
		return
	}

	var topologyPacketDataFull interface{} = nil

	// Entries in Node and Service tables related to item advertised in TopologyPacket
	var nodeTableEntry *NodeTableEntry = nil
	//var serviceTableEntry *ServiceTableEntry = nil

	// NodeTable entry for the local node (will be null on startup)
	localNodeEntry := thisTopologyTracker.NodeTable.GetEntry(thisNode.NodeID).(*NodeTableEntry)

	// NodeTable entry for the source node (will be null on startup)
	sourceNodeEntry := thisTopologyTracker.NodeTable.GetEntry(srcNodeID).(*NodeTableEntry)

	// Set the targetTableEntry to null for assignment
	var targetTableEntry TopologyTableEntryInterface = nil

	// Get Base object
	switch topologyPacket.Type {
	case "node":
		targetTable = *thisTopologyTracker.NodeTable
		nodeTableEntry = targetTable.GetEntry(topologyPacket.ID).(*NodeTableEntry)
		targetTableEntry = targetTable.GetEntry(topologyPacket.ID).(TopologyTableEntryInterface)
		break
	case "service":
		targetTable = *thisTopologyTracker.ServiceTable
		nodeTableEntry = thisTopologyTracker.NodeTable.GetEntry(topologyPacket.ID).(*NodeTableEntry)
		//serviceTableEntry = targetTable.GetEntry(topologyPacket.ID).(*ServiceTableEntry)
		targetTableEntry = targetTable.GetEntry(topologyPacket.ID).(TopologyTableEntryInterface)
		break
	default:
		return
	}

	// Whatever type it is, store it in topologyPacketDataFull
	topologyPacketDataFull = targetTable.ConvertJSONToEntry(topologyPacket.Data)
	marshalErr = json.Unmarshal(topologyPacket.Data, topologyPacketDataFull)
	if marshalErr != nil {
		thisNode.Log(fmt.Sprintf("error marshalling json: %s", marshalErr), true)
		return
	}

	// Inbound topology packet; service add, update, delete
	switch topologyPacket.Cmd {
	case "add":

		if targetTable.HasEntry(topologyPacket.ID) {
			// We already know about this one
			thisNode.Log(fmt.Sprintf("We've received a topologyPacket for a record we already have: %s[%s]", topologyPacket.Type, topologyPacket.ID), true)

			// If we're a Registry and the learned entry is a Registry, ignore it
			if localNodeEntry.IsRegistry() && nodeTableEntry.IsRegistry() {
				thisNode.Log("Ignoring - we learned about this from another registry node", true)
				return
			}

			// Someone sent us info about the local node; ignore it
			if topologyPacketData.NodeID == localNodeEntry.NodeID {
				thisNode.Log("Ignoring - received packet regarding the local node", true)
				return
			}

			// We knew about the entry before, but the node just connected to us
			if *topologyPacketData.NodeID == srcNodeID && *targetTableEntry.GetLearnedFrom() != srcNodeID {
				// This is a direct connection from the source; update the LearnedFrom
				if thisNode.IsRegistry() {
					// Another Registry has made a warm Node handoff to this one
					thisNode.Log(fmt.Sprintf("Updating LearnedFrom for %s [%s] from [%s] to [%s]", topologyPacket.Type, topologyPacket.ID, *targetTableEntry.GetLearnedFrom(), srcNodeID), true)
					targetTableEntry.SetLearnedFrom(srcNodeID, thisNode.GetTimestamp())

					// We may want to redistribute
					doRelay = true
					break
				} else {
					if sourceIsRegistry || sourceNodeEntry != nil && sourceNodeEntry.IsRegistry() {
						// A Registry Node has connected to this non-Registry node.
						thisNode.Log(fmt.Sprintf("Connected to new Registry, overwriting LearnedFrom for %s [%s] from [%s] to [%s]", topologyPacket.Type, topologyPacket.ID, *targetTableEntry.GetLearnedFrom(), srcNodeID), true)
						targetTableEntry.SetLearnedFrom(srcNodeID, thisNode.GetTimestamp())
					} else {
						// A non-Registry Node has connected to this non-Registry node.  Do not update LearnedFrom.
						thisNode.Log("Ignoring - neither the local node nor the sending node are Registries", true)
					}
					return
				}
			}

			// We are a Registry and learned about a newer route from another Registry; warm handoff?
			if thisNode.IsRegistry() && (sourceIsRegistry || sourceNodeEntry != nil && sourceNodeEntry.IsRegistry()) && *topologyPacketData.LearnedFrom == *topologyPacketData.NodeID && *nodeTableEntry.LearnedFrom != *nodeTableEntry.NodeID {
				//thisNode.log(`Ignoring ${topologyPacket.type} table entry [${topologyPacket.id}] from Node [${srcNodeID}], not not relayed from an authoritative source`);
				thisNode.Log(fmt.Sprintf("Updating LearnedFrom for %s [%s] from [%s] to [%s]", topologyPacket.Type, topologyPacket.ID, *targetTableEntry.GetLearnedFrom(), srcNodeID), true)
				targetTableEntry.SetLearnedFrom(srcNodeID, thisNode.GetTimestamp())

				// We wouldn't want to redistribute to other registries and we wouldn't need to redistribute to other nodes connected to us
				return
			}

			// We are not a Registry and Received this from a Registry after failure
			if !thisNode.IsRegistry() && (sourceIsRegistry || sourceNodeEntry != nil && sourceNodeEntry.IsRegistry()) {
				// We must have learned from a new Registry; treat like an add
				thisNode.Log(fmt.Sprintf("Connected to new Registry, overwriting LearnedFrom for %s [%s] from [%s] to [%s]", topologyPacket.Type, topologyPacket.ID, *targetTableEntry.GetLearnedFrom(), srcNodeID), true)
				targetTableEntry.SetLearnedFrom(srcNodeID, thisNode.GetTimestamp())
			}

		} else {
			// If this is a Registry receiving a second hand advertisement about another Registry, ignore it
			if thisNode.IsRegistry() && topologyPacket.Type == "node" && nodeTableEntry.IsRegistry() && srcNodeID != *topologyPacketData.NodeID {
				thisNode.Log("Ignoring - local node is a Registry secondhand advertisement about another Registry", true)
				return
			}

			// If this is a Registry and the sender didn't get it from an authoritative source, ignore it
			if thisNode.IsRegistry() && *topologyPacketData.NodeID != thisNode.NodeID && topologyPacketData.LearnedFrom != topologyPacketData.NodeID && topologyPacketData.LearnedFrom != topologyPacketData.ProxyNodeID {
				thisNode.Log(fmt.Sprintf("Ignoring %s table entry [%s] from Node [%s], not relayed from an authoritative source", topologyPacket.Type, topologyPacket.ID, srcNodeID), true)
				return
			}

			// If this is a service entry and we don't have a corresponding node table entry, ignore it
			if topologyPacket.Type == "service" && thisTopologyTracker.NodeTable.GetEntry(*topologyPacketData.NodeID) == nil {
				thisNode.Log(fmt.Sprintf("Ignoring service table entry [%s], no matching node table entry", topologyPacket.ID), true)
				return
			}

			thisNode.Log(fmt.Sprintf("Adding new %s table entry [%s]", topologyPacket.Type, topologyPacket.ID), true)
			targetTable.AddEntry(topologyPacket.ID, topologyPacketDataFull, thisNode.GetTimestamp())

			// Set for relay
			doRelay = true
		}
		break
	case "update":
		if targetTable.HasEntry(topologyPacket.ID) {
			targetTable.UpdateEntry(topologyPacket.ID, topologyPacketDataFull, thisNode.GetTimestamp())
			doRelay = true
		} else {
			thisNode.Log(fmt.Sprintf("Could not update non-existent %s entry %s", topologyPacket.Type, topologyPacket.ID), true)
			return
		}
		break
	case "delete":
		// Only delete if we learned the packet from the sourceID or if we are the source (due to disconnect)
		if topologyPacket.ID == thisNode.NodeID && topologyPacket.Type == "node" {
			thisNode.Log("This node tried to delete itself.  Why?", true)
			//console.dir(topologyPacket);
			return
		}
		// Update this rule so that if the table LearnedFrom is another Registry, do not delete or relay!  We are no longer authoritative
		if targetTable.HasEntry(topologyPacket.ID) && (*topologyPacketData.NodeID == srcNodeID || *topologyPacketData.LearnedFrom == srcNodeID) || thisNode.NodeID == srcNodeID {
			doRelay = true
			targetTable.DeleteEntry(topologyPacket.ID)
			if topologyPacket.Type == "node" {
				// Delete services from this node
				for serviceInstanceID, thisServiceEntry := range *thisTopologyTracker.ServiceTable {
					if *thisServiceEntry.NodeID == topologyPacket.ID || *thisServiceEntry.LearnedFrom == topologyPacket.ID {
						thisNode.Log(fmt.Sprintf("Removing entries learned from Node[%s] -> Service[%s]", topologyPacket.ID, serviceInstanceID), true)
						thisTopologyTracker.ServiceTable.DeleteEntry(serviceInstanceID)
					}
				}

				// Remove any dependent Nodes
				for _, checkNodeEntry := range *thisTopologyTracker.NodeTable {
					if *checkNodeEntry.LearnedFrom == topologyPacket.ID {
						// Delete this entry
						thisNode.Log(fmt.Sprintf("Removing entries learned from Node[%s] -> Node[%s]", topologyPacket.ID, *checkNodeEntry.NodeID), true)
						var packetDataBytes = checkNodeEntry.ToJSON()
						nodeDeletePacket := TopologyPacket{*checkNodeEntry.NodeID, "delete", "node", *checkNodeEntry.NodeID, *checkNodeEntry.Scope, *checkNodeEntry.Zone, packetDataBytes}
						thisTopologyTracker.ProcessPacket(nodeDeletePacket, *checkNodeEntry.NodeID, false)
					}
				}
			}
		} else {
			// Ignore delete command
			thisNode.Log(fmt.Sprintf("Ignoring delete from Node[%s]", srcNodeID), true)
			return
		}
		break
	default:
		return
	}

	// Send to TopicManager
	// TO DO - Add back after implementing TopicManager
	//thisNode.TopicManager.SendToTopic("TopologyTracker", topologyPacket)

	thisTopologyTracker.drpNode.Log(fmt.Sprintf("Imported topology packet from [%s] -> %s %s[%s]", topologyPacket.OriginNodeID, topologyPacket.Cmd, topologyPacket.Type, topologyPacket.ID), true)

	if !doRelay {
		// We don't want to relay the packet we received to anyone
		return
	}

	for targetNodeID, thisEndpoint := range thisTopologyTracker.drpNode.NodeEndpoints {
		relayPacket := thisTopologyTracker.AdvertiseOutCheck(topologyPacketData, &targetNodeID)

		if relayPacket {
			thisEndpoint.SendCmd("DRP", "topologyUpdate", topologyPacket, nil, nil, nil)
			thisNode.Log(fmt.Sprintf("Relayed topology packet to node: [%s]", targetNodeID), true)
		} else {
			if targetNodeID != thisNode.NodeID {
				//thisNode.log(`Not relaying packet to node[${targetNodeID}], roles ${thisTopologyTracker.NodeTable[targetNodeID].Roles}`);
			}
		}
	}
}

// ListServices returns a unique list of service names available for use
func (tt *TopologyTracker) ListServices() []string {
	uniqueServiceMap := make(map[string]bool)
	for _, serviceTableEntry := range *tt.ServiceTable {
		if !uniqueServiceMap[*serviceTableEntry.Name] {
			uniqueServiceMap[*serviceTableEntry.Name] = true
		}
	}

	uniqueServiceSlice := make([]string, 0, len(uniqueServiceMap))
	for v := range uniqueServiceMap {
		uniqueServiceSlice = append(uniqueServiceSlice, v)
	}
	return uniqueServiceSlice
}

// GetServicesWithProviders returns a map of services along with the Providers' NodeIDs (used by PathCmd)
func (tt *TopologyTracker) GetServicesWithProviders() map[string]*struct {
	ServiceName string
	Providers   []string
} {

	returnObject := make(map[string]*struct {
		ServiceName string
		Providers   []string
	})
	for _, serviceTableEntry := range *tt.ServiceTable {
		if _, ok := returnObject[*serviceTableEntry.Name]; !ok {
			returnObject[*serviceTableEntry.Name] = &struct {
				ServiceName string
				Providers   []string
			}{*serviceTableEntry.Name, []string{}}
		}
		newSlice := append(returnObject[*serviceTableEntry.Name].Providers, *serviceTableEntry.NodeID)
		returnObject[*serviceTableEntry.Name].Providers = newSlice
	}
	return returnObject
}

// FindInstanceOfService finds the best instance of a service to execute a command
func (tt *TopologyTracker) FindInstanceOfService(serviceName *string, serviceType *string, zone *string, nodeID *string) *ServiceTableEntry {
	thisTopologyTracker := tt
	thisNode := thisTopologyTracker.drpNode

	// Look in this Node's zone unless a zone was specified
	checkZone := thisNode.Zone
	if zone != nil {
		checkZone = *zone
	}

	// If neither a name nor a type is specified, return null
	if serviceName == nil && serviceType == nil {
		return nil
	}

	/*
	* Status MUST be 1 (Ready)
	* Local zone is better than others (if specified, must match)
	* Lower priority is better
	* Higher weight is better
	 */

	var bestServiceEntry *ServiceTableEntry = nil
	candidateList := []*ServiceTableEntry{}

	for _, serviceTableEntry := range *tt.ServiceTable {

		// Skip if the service isn't ready
		if serviceTableEntry.Status != 1 {
			continue
		}

		// Skip if the service name/type doesn't match
		if serviceName != nil && *serviceName != *serviceTableEntry.Name {
			continue
		}
		if serviceType != nil && *serviceType != *serviceTableEntry.Type {
			continue
		}

		// Skip if the node ID doesn't match
		if nodeID != nil && *nodeID != *serviceTableEntry.NodeID {
			continue
		}

		// If we offer the service locally, select it and continue
		if *serviceTableEntry.NodeID == thisNode.NodeID {
			return serviceTableEntry
		}

		// Skip if the zone is specified and doesn't match
		switch *serviceTableEntry.Scope {
		case "local":
			break
		case "global":
			break
		case "zone":
			if checkZone != *serviceTableEntry.Zone {
				continue
			}
			break
		default:
			// Unrecognized scope option
			continue
		}

		// Delete if we don't have a corresponding Node entry
		if !thisTopologyTracker.NodeTable.HasEntry(*serviceTableEntry.NodeID) {
			thisNode.Log(fmt.Sprintf("Deleted service table entry [%s], no matching node table entry", *serviceTableEntry.InstanceID), false)
			thisTopologyTracker.ServiceTable.DeleteEntry(*serviceTableEntry.InstanceID)
			continue
		}

		// If this is the first candidate, set it and go
		if bestServiceEntry == nil {
			bestServiceEntry = serviceTableEntry
			candidateList = []*ServiceTableEntry{bestServiceEntry}
			continue
		}

		// Check this against the current bestServiceEntry

		// Better zone?
		if *bestServiceEntry.Zone != checkZone && *serviceTableEntry.Zone == checkZone {
			// The service being evaluated is in a better zone
			bestServiceEntry = serviceTableEntry
			candidateList = []*ServiceTableEntry{bestServiceEntry}
			continue
		} else if *bestServiceEntry.Zone == checkZone && *serviceTableEntry.Zone != checkZone {
			// The service being evaluated is in a different zone
			continue
		}

		// Local preference?
		if *bestServiceEntry.Scope != "local" && *serviceTableEntry.Scope == "local" {
			bestServiceEntry = serviceTableEntry
			candidateList = []*ServiceTableEntry{bestServiceEntry}
			continue
		}

		// Lower Priority?
		if bestServiceEntry.Priority > serviceTableEntry.Priority {
			bestServiceEntry = serviceTableEntry
			candidateList = []*ServiceTableEntry{bestServiceEntry}
			continue
		}

		// Weighted?
		if bestServiceEntry.Priority == serviceTableEntry.Priority {
			candidateList = append(candidateList, serviceTableEntry)
		}
	}

	// Did we find a match?
	if len(candidateList) == 1 {
		// Single match
	} else if len(candidateList) > 1 {
		// Multiple matches; select based on weight

		rand.Seed(time.Now().UTC().UnixNano()) // always seed random!

		var choices []wr.Choice = []wr.Choice{}
		for _, serviceTableEntry := range candidateList {
			choices = append(choices, wr.Choice{Item: serviceTableEntry, Weight: serviceTableEntry.Weight})
		}

		chooser, _ := wr.NewChooser(choices...)
		bestServiceEntry = chooser.Pick().(*ServiceTableEntry)
		if thisNode.Debug {
			qualifierText := ""
			if serviceName != nil {
				qualifierText = fmt.Sprintf("name[%s]", *serviceName)
			}
			if serviceType != nil {
				if len(qualifierText) != 0 {
					qualifierText = fmt.Sprintf("%s/", qualifierText)
				}
				qualifierText = `${qualifierText}type[${serviceType}]`
			}
			thisNode.Log(fmt.Sprintf("Need service %s, randomly selected [%s]", qualifierText, *bestServiceEntry.InstanceID), true)
		}
	}

	return bestServiceEntry
}

// FindServicePeers returns the service peers for a specified instance
func (tt *TopologyTracker) FindServicePeers(serviceID string) []string {
	thisTopologyTracker := tt
	peerServiceIDList := []string{}
	originServiceTableEntry := thisTopologyTracker.ServiceTable.GetEntry(serviceID).(*ServiceTableEntry)
	if originServiceTableEntry == nil {
		return peerServiceIDList
	}

	for _, serviceTableEntry := range *tt.ServiceTable {

		// Skip the instance specified
		if *serviceTableEntry.InstanceID == serviceID {
			continue
		}

		// Skip if the service isn't ready
		if serviceTableEntry.Status != 1 {
			continue
		}

		// Skip if the service name/type doesn't match
		if originServiceTableEntry.Name != serviceTableEntry.Name {
			continue
		}
		if originServiceTableEntry.Type != serviceTableEntry.Type {
			continue
		}

		// Skip if the zone doesn't match
		switch *originServiceTableEntry.Scope {
		case "local":
			if *originServiceTableEntry.NodeID != *serviceTableEntry.NodeID {
				continue
			}
			break
		case "global":
			break
		case "zone":
			if *originServiceTableEntry.Zone != *serviceTableEntry.Zone {
				continue
			}
			break
		default:
			// Unrecognized scope option
			continue
		}

		peerServiceIDList = append(peerServiceIDList, *serviceTableEntry.InstanceID)
	}
	return peerServiceIDList
}

// AdvertiseOutCheck determines whether or not a TopologyTableEntry should be forwarded to given NodeID
func (tt *TopologyTracker) AdvertiseOutCheck(topologyEntry *TopologyTableEntry, targetNodeID *string) bool {

	thisTopologyTracker := tt
	thisNode := thisTopologyTracker.drpNode
	localNodeID := thisNode.NodeID
	doSend := false

	advertisedNodeID := topologyEntry.NodeID
	//learnedFromNodeID := topologyEntry.LearnedFrom
	//proxyNodeID := topologyEntry.ProxyNodeID
	advertisedScope := topologyEntry.Scope
	advertisedZone := topologyEntry.Zone

	localNodeEntry := (*thisTopologyTracker.NodeTable)[localNodeID]
	targetNodeEntry := (*thisTopologyTracker.NodeTable)[*targetNodeID]

	// We don't recognize the target node; give them everything by default
	if targetNodeEntry == nil {
		return true
	}

	// TODO - Add logic to support Proxied Nodes

	switch *advertisedScope {
	case "local":
		// Never advertise local
		return false
	case "zone":
		// Do not proceed if the target isn't in the same zone
		if *advertisedZone != *targetNodeEntry.Zone {
			//thisNode.log(`Not relaying because Node[${targetNodeID}] is not in the same zone!`);
			return false
		}
		break
	case "global":
		// Global services can be advertised anywhere
		break
	default:
		// Unknown scope type
		return false
	}

	// Never send back to the origin
	if *advertisedNodeID == *targetNodeID {
		return false
	}

	// Always relay locally sourced entries
	if *advertisedNodeID == localNodeID {
		return true
	}

	// Only send items for which we are authoritative
	if localNodeEntry.IsRegistry() {
		// The local node is a Registry

		/// Relay to connected non-Registry Nodes
		if targetNodeEntry != nil && !targetNodeEntry.IsRegistry() {
			return true
		}

		// Relay if the advertised node was locally connected
		if *topologyEntry.LearnedFrom == *topologyEntry.NodeID {
			return true
		}

		// Relay if we know the target isn't a Registry
		if targetNodeEntry != nil && !targetNodeEntry.IsRegistry() {
			return true
		}

		// Do not relay
		//console.log(`Not relaying to Node[${targetNodeID}]`);
		//console.dir(topologyEntry);
		return false
	}

	return doSend
}

// AdvertiseOutCheckNode determines whether or not a Node topology entry should be advertised to another Node
func (tt *TopologyTracker) AdvertiseOutCheckNode(nodeTableEntry *NodeTableEntry, targetNodeID *string) bool {
	return true
}

// AdvertiseOutCheckService determines whether or not a Service topology entry should be advertised to another Node
func (tt *TopologyTracker) AdvertiseOutCheckService(serviceTableEntry *ServiceTableEntry, targetNodeID *string) bool {
	return true
}

// GetRegistry returns a copy of the local Registry (Node and Service tables)
func (tt *TopologyTracker) GetRegistry(requestingNodeID *string) struct {
	NodeTable    map[string]*NodeTableEntry
	ServiceTable map[string]*ServiceTableEntry
} {
	thisTopologyTracker := tt

	returnNodeTable := make(map[string]*NodeTableEntry)
	returnServiceTable := make(map[string]*ServiceTableEntry)

	if requestingNodeID == nil {
		requestingNodeID = new(string)
		*requestingNodeID = ""
	}

	for advertisedNodeID, advertisedNodeEntry := range *thisTopologyTracker.NodeTable {
		relayPacket := thisTopologyTracker.AdvertiseOutCheckNode(advertisedNodeEntry, requestingNodeID)
		if relayPacket {
			returnNodeTable[advertisedNodeID] = advertisedNodeEntry
		}
	}

	for advertisedServiceID, advertisedServiceEntry := range *thisTopologyTracker.ServiceTable {
		relayPacket := thisTopologyTracker.AdvertiseOutCheckService(advertisedServiceEntry, requestingNodeID)
		if relayPacket {
			returnServiceTable[advertisedServiceID] = advertisedServiceEntry
		}
	}

	returnObj := struct {
		NodeTable    map[string]*NodeTableEntry
		ServiceTable map[string]*ServiceTableEntry
	}{
		returnNodeTable,
		returnServiceTable,
	}

	return returnObj
}

// ProcessNodeConnect executes after connecting to a Node endpoint
func (tt *TopologyTracker) ProcessNodeConnect(remoteEndpoint EndpointInterface, remoteNodeDeclaration *NodeDeclaration, localNodeIsProxy bool) {
	thisTopologyTracker := tt
	thisNode := thisTopologyTracker.drpNode
	thisNode.Log(fmt.Sprintf("Connection established with Node [%s] (%s)", remoteNodeDeclaration.NodeID, strings.Join(remoteNodeDeclaration.NodeRoles, ",")), false)
	returnData := remoteEndpoint.SendCmdAwait("DRP", "getRegistry", map[string]string{"reqNodeID": thisNode.NodeID}, nil, nil)

	sourceIsRegistry := false

	remoteRegistry := struct {
		NodeTable    map[string]NodeTableEntry
		ServiceTable map[string]ServiceTableEntry
	}{}

	err := json.Unmarshal(*returnData.Payload, &remoteRegistry)
	if err != nil {
		thisNode.Log(fmt.Sprintf("ProcessNodeConnect Payload unmarshal error: %s", err), false)
		return
	}

	for _, a := range remoteNodeDeclaration.NodeRoles {
		if a == "Registry" {
			sourceIsRegistry = true
		}
	}

	runCleanup := false

	for _, thisNodeEntry := range remoteRegistry.NodeTable {
		if localNodeIsProxy {
			thisNodeEntry.ProxyNodeID = &thisNode.NodeID
		}
		nodeAddPacket := TopologyPacket{*thisNodeEntry.NodeID, "add", "node", *thisNodeEntry.NodeID, *thisNodeEntry.Scope, *thisNodeEntry.Zone, thisNodeEntry.ToJSON()}
		thisTopologyTracker.ProcessPacket(nodeAddPacket, *remoteEndpoint.GetID(), false)
	}

	for _, thisServiceEntry := range remoteRegistry.ServiceTable {
		if localNodeIsProxy {
			thisServiceEntry.ProxyNodeID = thisServiceEntry.NodeID
		}
		serviceAddPacket := TopologyPacket{*thisServiceEntry.NodeID, "add", "service", *thisServiceEntry.InstanceID, *thisServiceEntry.Scope, *thisServiceEntry.Zone, thisServiceEntry.ToJSON()}
		thisTopologyTracker.ProcessPacket(serviceAddPacket, *remoteEndpoint.GetID(), false)
	}

	// Execute onControlPlaneConnect callback
	if !thisNode.IsRegistry() && sourceIsRegistry && !thisNode.ConnectedToControlPlane {
		// We are connected to a Registry
		thisNode.ConnectedToControlPlane = true
		runCleanup = true
		if thisNode.onControlPlaneConnect != nil && !thisNode.HasConnectedToMesh {
			(*thisNode.onControlPlaneConnect)()
		}
		thisNode.HasConnectedToMesh = true
	}

	// Remove any stale entries if we're reconnecting to a new Registry
	if runCleanup {
		thisTopologyTracker.StaleEntryCleanup()
	}
}

// ProcessNodeDisconnect processes topology commands for Node disconnect events
func (tt *TopologyTracker) ProcessNodeDisconnect(disconnectedNodeID string) {
	thisTopologyTracker := tt
	thisNode := thisTopologyTracker.drpNode

	// Remove node; this should trigger an autoremoval of entries learned from it

	// If we are not a Registry and we just disconnected from a Registry, hold off on this process!
	thisNodeEntry := (*thisTopologyTracker.NodeTable)[thisNode.NodeID]
	disconnectedNodeEntry := (*thisTopologyTracker.NodeTable)[disconnectedNodeID]

	if disconnectedNodeEntry == nil {
		thisNode.Log(fmt.Sprintf("Ran ProcessNodeDisconnect on non-existent Node [%s]", disconnectedNodeID), false)
		return
	}

	thisNode.Log(fmt.Sprintf("Connection terminated with Node [%s] (%s)", *disconnectedNodeEntry.NodeID, strings.Join(disconnectedNodeEntry.Roles, ",")), false)

	// If both the local and remote are non-Registry nodes, skip further processing.  May just be a direct connection timing out.
	if !thisNodeEntry.IsRegistry() && !disconnectedNodeEntry.IsRegistry() {
		return
	}

	// See if we're connected to other Registry Nodes
	hasAnotherRegistryConnection := len(thisTopologyTracker.ListConnectedRegistryNodes()) > 0

	// Do we need to hold off on purging the Registry?
	if !thisNodeEntry.IsRegistry() && disconnectedNodeEntry != nil && disconnectedNodeEntry.IsRegistry() && !hasAnotherRegistryConnection {
		// Do not go through with the cleanup process; delete only the disconnected Registry node
		// for now and we'll run the StaleEntryCleanup when we connect to the next Registry.
		thisNode.Log(fmt.Sprintf("We disconnected from Registry Node[%s] and have no other Registry connections", disconnectedNodeID), false)
		thisTopologyTracker.NodeTable.DeleteEntry(disconnectedNodeID)
		thisNode.ConnectedToControlPlane = false
		return
	}

	// Issue Node Delete topology commands for the disconnected Node or any entries learned from the disconnected Node
	for _, checkNodeEntry := range *thisTopologyTracker.NodeTable {
		if *checkNodeEntry.NodeID == disconnectedNodeID || *checkNodeEntry.LearnedFrom == disconnectedNodeID {
			nodeDeletePacket := TopologyPacket{*thisNodeEntry.NodeID, "delete", "node", *checkNodeEntry.NodeID, *checkNodeEntry.Scope, *checkNodeEntry.Zone, checkNodeEntry.ToJSON()}
			thisTopologyTracker.ProcessPacket(nodeDeletePacket, *checkNodeEntry.NodeID, checkNodeEntry.IsRegistry())
		}
	}

	if thisNode.ConnectedToControlPlane {
		thisNode.TopologyTracker.StaleEntryCleanup()
	}
}

// GetNextHop return the next hop to communicate with a given Node ID
func (tt *TopologyTracker) GetNextHop(checkNodeID string) *string {
	var checkNodeTableEntry = tt.NodeTable.GetEntry(checkNodeID).(*NodeTableEntry)
	if checkNodeTableEntry != nil {
		return checkNodeTableEntry.LearnedFrom
	}
	return nil
}

// ValidateNodeID tells whether or not a NodeID is present in the NodeTable
func (tt *TopologyTracker) ValidateNodeID(checkNodeID string) bool {
	return tt.NodeTable.HasEntry(checkNodeID)
}

// GetNodeWithURL returns NodeID with a given NodeURL
func (tt *TopologyTracker) GetNodeWithURL(checkNodeURL string) *string {
	thisTopologyTracker := tt
	for thisNodeID, thisNodeEntry := range *thisTopologyTracker.NodeTable {
		if thisNodeEntry.NodeURL != nil && *thisNodeEntry.NodeURL == checkNodeURL {
			return &thisNodeID
		}
	}
	return nil
}

// StaleEntryCleanup removed stale Node and Service entries
func (tt *TopologyTracker) StaleEntryCleanup() {
	thisTopologyTracker := tt
	for _, thisNodeEntry := range *thisTopologyTracker.NodeTable {
		if thisNodeEntry.ProxyNodeID != nil && thisTopologyTracker.ValidateNodeID(*thisNodeEntry.ProxyNodeID) {
			thisTopologyTracker.NodeTable.DeleteEntry(*thisNodeEntry.NodeID)
		}
	}

	for _, thisServiceEntry := range *thisTopologyTracker.ServiceTable {
		if !thisTopologyTracker.ValidateNodeID(*thisServiceEntry.NodeID) {
			thisTopologyTracker.ServiceTable.DeleteEntry(*thisServiceEntry.NodeID)
		}
	}
}

// ListConnectedRegistryNodes returns a list of connected Registry NodeIDs
func (tt *TopologyTracker) ListConnectedRegistryNodes() []string {
	thisTopologyTracker := tt
	connectedRegistryList := []string{}

	for checkNodeID, checkNodeEntry := range *thisTopologyTracker.NodeTable {
		if *checkNodeEntry.NodeID != thisTopologyTracker.drpNode.NodeID && checkNodeEntry.IsRegistry() && thisTopologyTracker.drpNode.IsConnectedTo(checkNodeID) {
			// Remote Node is a Registry and we are connected to it
			connectedRegistryList = append(connectedRegistryList, checkNodeID)
		}
	}
	return connectedRegistryList
}

// FindRegistriesInZone returns a list of Registry Nodes in a given zone
func (tt *TopologyTracker) FindRegistriesInZone(zoneName string) []string {
	thisTopologyTracker := tt
	zoneRegistryList := []string{}

	for checkNodeID, checkNodeEntry := range *thisTopologyTracker.NodeTable {
		if checkNodeEntry.IsRegistry() && *checkNodeEntry.Zone == zoneName {
			// Remote Node is a Registry and we are connected to it
			zoneRegistryList = append(zoneRegistryList, checkNodeID)
		}
	}
	return zoneRegistryList
}

// TopologyTable is used for NodeTable and ServiceTable modules
type TopologyTable interface {
	HasEntry(string) bool
	AddEntry(string, interface{}, string)
	UpdateEntry(string, interface{}, string)
	GetEntry(string) interface{}
	DeleteEntry(string)
	ConvertJSONToEntry([]byte) interface{}
	ConvertEntryToJSON(interface{}) []byte
}

// TopologyTableEntryInterface is used for NodeTableEntry and ServiceTableEntry
type TopologyTableEntryInterface interface {
	GetLearnedFrom() *string
	SetLearnedFrom(string, string)
	ToJSON() []byte
}

// TopologyTableEntry is used for NodeTableEntry and ServiceTableEntry
type TopologyTableEntry struct {
	TopologyTableEntryInterface
	NodeID       *string
	ProxyNodeID  *string
	Scope        *string
	Zone         *string
	LearnedFrom  *string
	LastModified *string
}

// GetLearnedFrom returns the LearnedFrom attribute
func (tte TopologyTableEntry) GetLearnedFrom() *string {
	return tte.LearnedFrom
}

// SetLearnedFrom sets the LearnedFrom attribute
func (tte TopologyTableEntry) SetLearnedFrom(learnedFrom string, lastModified string) {
	*tte.LearnedFrom = learnedFrom
	*tte.LastModified = lastModified
}

// NodeTableEntry provides the details of a Node
type NodeTableEntry struct {
	TopologyTableEntry
	Roles   []string
	NodeURL *string
	HostID  *string
}

// ToJSON converts the NodeTableEntry to JSON
func (nte NodeTableEntry) ToJSON() []byte {
	entryBytes, _ := json.Marshal(nte)
	return entryBytes
}

// IsRegistry - tells whether a Node topology entry hold the Registry role
func (nte NodeTableEntry) IsRegistry() bool {
	for _, a := range nte.Roles {
		if a == "Registry" {
			return true
		}
	}
	return false
}

// IsBroker tells whether a Node topology entry hold the Broker role
func (nte NodeTableEntry) IsBroker() bool {
	for _, a := range nte.Roles {
		if a == "Broker" {
			return true
		}
	}
	return false
}

// UsesProxy - tells whether a Node topology entry uses a Proxy
func (nte NodeTableEntry) UsesProxy() bool {
	if nte.ProxyNodeID != nil {
		return true
	}
	return false
}

// NodeTable is a map of NodeTableEntry objects
type NodeTable map[string]*NodeTableEntry

// HasEntry tells whether or not a given entry is present in the NodeTable
func (nt NodeTable) HasEntry(entryID string) bool {
	_, ok := nt[entryID]
	return ok
}

// AddEntry adds a new entry to the NodeTable
func (nt NodeTable) AddEntry(entryID string, entryData interface{}, lastModified string) {
	nt[entryID] = entryData.(*NodeTableEntry)
	nt[entryID].LastModified = &lastModified
}

// UpdateEntry updates an existing entry in the NodeTable
func (nt NodeTable) UpdateEntry(entryID string, entryData interface{}, lastModified string) {
	nt[entryID] = entryData.(*NodeTableEntry)
	nt[entryID].LastModified = &lastModified
}

// GetEntry returns an entry from the NodeTable
func (nt NodeTable) GetEntry(entryID string) interface{} {
	var returnVal *NodeTableEntry = nil
	targetNodeTableEntry, ok := nt[entryID]
	if ok {
		returnVal = targetNodeTableEntry
	}
	return returnVal
}

// DeleteEntry deletes an entry from the NodeTable
func (nt NodeTable) DeleteEntry(entryID string) {
	delete(nt, entryID)
}

// ConvertJSONToEntry unmarshals JSON to table entry
func (nt NodeTable) ConvertJSONToEntry(entryJSONBytes []byte) interface{} {
	thisEntry := &NodeTableEntry{}
	json.Unmarshal(entryJSONBytes, thisEntry)
	return thisEntry
}

// ConvertEntryToJSON marshals table entry to JSON
func (nt NodeTable) ConvertEntryToJSON(tableEntry interface{}) []byte {
	entryToConvert := tableEntry.(NodeTableEntry)
	entryBytes, _ := json.Marshal(entryToConvert)
	return entryBytes
}

// ServiceTableEntry provides the details of a Service
type ServiceTableEntry struct {
	TopologyTableEntry
	Name         *string
	Type         *string
	InstanceID   *string
	Sticky       bool
	Priority     uint
	Weight       uint
	Dependencies []string
	Streams      []string
	Status       int
}

// ToJSON marshals table entry to JSON
func (ste ServiceTableEntry) ToJSON() []byte {
	entryBytes, _ := json.Marshal(ste)
	return entryBytes
}

// ServiceTable is a map of NodeTableEntry objects
type ServiceTable map[string]*ServiceTableEntry

// HasEntry tells whether or not a given entry is present in the ServiceTable
func (st ServiceTable) HasEntry(entryID string) bool {
	_, ok := st[entryID]
	return ok
}

// AddEntry adds a new entry to the ServiceTable
func (st ServiceTable) AddEntry(entryID string, entryData interface{}, lastModified string) {
	st[entryID] = entryData.(*ServiceTableEntry)
	st[entryID].LastModified = &lastModified
}

// UpdateEntry updates an existing entry in the ServiceTable
func (st ServiceTable) UpdateEntry(entryID string, entryData interface{}, lastModified string) {
	st[entryID] = entryData.(*ServiceTableEntry)
	st[entryID].LastModified = &lastModified
}

// GetEntry returns an entry from the ServiceTable
func (st ServiceTable) GetEntry(entryID string) interface{} {
	var returnVal *ServiceTableEntry = nil
	targetServiceTableEntry, ok := st[entryID]
	if ok {
		returnVal = targetServiceTableEntry
	}
	return returnVal
}

// DeleteEntry deletes an entry from the ServiceTable
func (st ServiceTable) DeleteEntry(entryID string) {
	delete(st, entryID)
}

// ConvertJSONToEntry unmarshals JSON to table entry
func (st ServiceTable) ConvertJSONToEntry(entryJSONBytes []byte) interface{} {
	thisEntry := &ServiceTableEntry{}
	json.Unmarshal(entryJSONBytes, thisEntry)
	return thisEntry
}

// ConvertEntryToJSON marshals table entry to JSON
func (st ServiceTable) ConvertEntryToJSON(tableEntry interface{}) []byte {
	entryToConvert := tableEntry.(ServiceTableEntry)
	entryBytes, _ := json.Marshal(entryToConvert)
	return entryBytes
}

// TopologyPacket defines the structure of a packet transmitted between Nodes
type TopologyPacket struct {
	OriginNodeID string          `json:"originNodeID"`
	Cmd          string          `json:"cmd"`
	Type         string          `json:"type"`
	ID           string          `json:"id"`
	Scope        string          `json:"scope"`
	Zone         string          `json:"zone"`
	Data         json.RawMessage `json:"data"`
}
