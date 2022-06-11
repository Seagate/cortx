'use strict';
const DRP_Node = require('drp-mesh').Node;
const FedExAPIMgr = require('drp-service-fedex');
const os = require("os");

let hostID = process.env.HOSTID || os.hostname();
let domainName = process.env.DOMAINNAME || null;
let meshKey = process.env.MESHKEY || null;
let zoneName = process.env.ZONENAME || null;
let registryUrl = process.env.REGISTRYURL || null;
let debug = process.env.DEBUG || false;
let testMode = process.env.TESTMODE || false;

let serviceName = process.env.SERVICENAME || "FedEx";
let priority = process.env.PRIORITY || null;
let weight = process.env.WEIGHT || null;
let scope = process.env.SCOPE || null;

let apiKey = process.env.APIKEY || null;
let secretKey = process.env.SECRETKEY || null;
let serviceBaseURL = process.env.SERVICEBASEURL || null;
let shippingAccount = process.env.SHIPPINGACCOUNT || null;

// Set Roles
let roleList = ["Provider"];

// Create Node
console.log(`Starting DRP Node`);
let myNode = new DRP_Node(roleList, hostID, domainName, meshKey, zoneName, serviceBaseURL);
myNode.Debug = debug;
myNode.TestMode = testMode;
myNode.RegistryUrl = registryUrl;
myNode.ConnectToMesh(async () => {

    // Add a FedEx service
    myNode.AddService(new FedExAPIMgr(serviceName, myNode, priority, weight, scope, apiKey, secretKey, shippingAccount));
});

