'use strict';
const DRP_Node = require('drp-mesh').Node;
const NetScalerManager = require('drp-service-netscaler');
const os = require("os");

// Node variables
let hostID = process.env.HOSTID || os.hostname();
let domainName = process.env.DOMAINNAME || null;
let meshKey = process.env.MESHKEY || null;
let zoneName = process.env.ZONENAME || null;
let registryUrl = process.env.REGISTRYURL || null;
let debug = process.env.DEBUG || false;
let testMode = process.env.TESTMODE || false;
let authenticatorService = process.env.AUTHENTICATORSERVICE || null;

// Service specific variables
let fs = require('fs');
let promisify = require('util').promisify;
let serviceName = process.env.SERVICENAME || "NetScaler";
let priority = process.env.PRIORITY || null;
let weight = process.env.WEIGHT || null;
let scope = process.env.SCOPE || null;

// Set Roles
let roleList = ["Provider"];

// Create Node
console.log(`Starting DRP Node`);
let myNode = new DRP_Node(roleList, hostID, domainName, meshKey, zoneName);
myNode.Debug = debug;
myNode.TestMode = testMode;
myNode.RegistryUrl = registryUrl;
myNode.ConnectToMesh(async () => {
	let thisSvc = new NetScalerManager(serviceName, myNode, priority, weight, scope, async () => {
		// This is executed on inital load and refreshes

		// Get Set Names
		let setNames = await myNode.ServiceCmd("DocMgr", "listDocs", { serviceName: "NetScaler" }, null, null, false, true, null);

		// Loop over config set names
		for (let i = 0; i < setNames.length; i++) {
			let thisSetName = setNames[i];
			if (!thisSetName) continue;
			let thisSetDoc = await myNode.ServiceCmd("DocMgr", "loadDoc", { serviceName: "NetScaler", docName: thisSetName }, null, null, false, true, null);
			thisSvc.AddSet(thisSetName, thisSetDoc.Hosts, thisSetDoc.PrivateKey);
		}
	});
    myNode.AddService(thisSvc);
});
