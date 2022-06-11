'use strict';
const DRP_Node = require('drp-mesh').Node;
const DocMgr = require('drp-service-docmgr');
const os = require("os");

let hostID = process.env.HOSTID || os.hostname();
let domainName = process.env.DOMAINNAME || "";
let meshKey = process.env.MESHKEY || "supersecretkey";
let zoneName = process.env.ZONENAME || "MyZone";
let registryUrl = process.env.REGISTRYURL || null;
let debug = process.env.DEBUG || false;
let testMode = process.env.TESTMODE || false;

// Service specific variables
let serviceName = process.env.SERVICENAME || "DocMgr";
let priority = process.env.PRIORITY || null;
let weight = process.env.WEIGHT || null;
let scope = process.env.SCOPE || null;
let docsPath = process.env.DOCSPATH || 'jsondocs';
let mongoHost = process.env.MONGOHOST || null;
let mongoUser = process.env.MONGOUSER || null;
let mongoPw = process.env.MONGOPW || null;

// Set Roles
let roleList = ["Provider"];

// Create Node
console.log(`Starting DRP Node`);
let myNode = new DRP_Node(roleList, hostID, domainName, meshKey, zoneName);
myNode.Debug = debug;
myNode.TestMode = testMode;
myNode.RegistryUrl = registryUrl;
myNode.ConnectToMesh(async () => {
    let myService = new DocMgr(serviceName, myNode, priority, weight, scope, docsPath, mongoHost, mongoUser, mongoPw);
    myNode.AddService(myService);
});
