package main

import (
	"fmt"
	"os"

	"./drpmesh"
)

func main() {
	fmt.Println("Test DRP_Node Instantiation:")
	//listeningName := "ws://somehost.domain.com:8080"
	nodeHostname, _ := os.Hostname()
	thisNode := drpmesh.CreateNode([]string{"Provider"}, nodeHostname, "mydomain.xyz", "supersecretkey", "zone1", "global", nil, nil, nil, true)
	thisNode.Log("Node created", false)
	//fmt.Printf("%+v\n", thisNode)
	//thisNode.ConnectToBroker("ws://localhost:8080", nil)

	/*
		thisNodeID := &thisNode.NodeID
		var results = thisNode.TopologyTracker.GetRegistry(thisNodeID)

		var resultsBytes, _ = json.Marshal(results)
		fmt.Printf("%s\n", string(resultsBytes))
	*/
	thisNode.ConnectToRegistry("ws://localhost:8080", nil, nil)
	//thisNode.ConnectToMesh()

	//servicesWithProviders := thisNode.TopologyTracker.GetServicesWithProviders()
	//var resultsBytes, _ = json.Marshal(servicesWithProviders)
	//fmt.Printf("%s\n", string(resultsBytes))

	//drpServiceDef := thisNode.Services["DRP"].GetDefinition()
	//var resultsBytes, _ = json.Marshal(drpServiceDef)
	//fmt.Printf("%s\n", string(resultsBytes))

	//serviceCmdResponse := thisNode.ServiceCmd("DocMgr", "listServices", nil, nil, nil, true, true, nil)
	//var resultsBytes, _ = json.Marshal(serviceCmdResponse)
	//fmt.Printf("%s\n", string(resultsBytes))

	//serviceName := "DocMgr"
	//bestServiceTableEntry := thisNode.TopologyTracker.FindInstanceOfService(&serviceName, nil, nil, nil)
	//var resultsBytes, _ = json.Marshal(bestServiceTableEntry)
	//fmt.Printf("%s\n", string(resultsBytes))

	doneChan := make(chan bool)
	_ = <-doneChan
	//fmt.Printf("%+v\n", results)
}
