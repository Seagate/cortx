'use strict';
const DRP_Node = require('drp-mesh').Node;
const BlueCatManager = require('drp-service-bluecat');
const os = require("os");

let hostID = process.env.HOSTID || os.hostname();
let domainName = process.env.DOMAINNAME || null;
let meshKey = process.env.MESHKEY || null;
let zoneName = process.env.ZONENAME || null;
let registryUrl = process.env.REGISTRYURL || null;
let debug = process.env.DEBUG || false;
let testMode = process.env.TESTMODE || false;

// Service specific variables
let serviceName = process.env.SERVICENAME || "BlueCat";
let priority = process.env.PRIORITY || null;
let weight = process.env.WEIGHT || null;
let scope = process.env.SCOPE || null;
let bcUser = process.env.BCUSER || "";
let bcPass = process.env.BCPASS || "";
let bcHosts = process.env.BCHOSTS.split(/[ ,]+/) || [];
let defaultView = process.env.DEFAULTVIEW || null;

// Set Roles
let roleList = ["Provider"];

// Create Node
console.log(`Starting DRP Node`);
let myNode = new DRP_Node(roleList, hostID, domainName, meshKey, zoneName);
myNode.Debug = debug;
myNode.TestMode = testMode;
myNode.RegistryUrl = registryUrl;
myNode.ConnectToMesh(async () => {
    let myService = new BlueCatManager(serviceName, myNode, priority, weight, scope, bcHosts, bcUser, bcPass, defaultView, () => {
        myNode.AddService(myService);
    });
});
