'use strict';
const DRP_Node = require('drp-mesh').Node;
const DRP_WebServer = require('drp-mesh').WebServer;
const CacheManager = require('drp-service-cache');
const os = require("os");

var protocol = "ws";
if (process.env.SSL_ENABLED) {
    protocol = "wss";
}
let port = process.env.PORT || 8080;
let listeningName = process.env.LISTENINGNAME || os.hostname();
let hostID = process.env.HOSTID || os.hostname();
let domainName = process.env.DOMAINNAME || "";
let meshKey = process.env.MESHKEY || "supersecretkey";
let zoneName = process.env.ZONENAME || "MyZone";
let registryUrl = process.env.REGISTRYURL || null;
let debug = process.env.DEBUG || false;
let testMode = process.env.TESTMODE || false;

// Service specific variables
let serviceName = process.env.SERVICENAME || "CacheManager";
let priority = process.env.PRIORITY || null;
let weight = process.env.WEIGHT || null;
let scope = process.env.SCOPE || null;
let mongoHost = process.env.MONGOHOST || "localhost";
let mongoUser = process.env.MONGOUSER || null;
let mongoPw = process.env.MONGOPW || null;

let drpWSRoute = "";

// Set config
let myServerConfig = {
    "ListeningURL": `${protocol}://${listeningName}:${port}${drpWSRoute}`,
    "Port": port,
    "SSLEnabled": process.env.SSL_ENABLED || false,
    "SSLKeyFile": process.env.SSL_KEYFILE || "",
    "SSLCrtFile": process.env.SSL_CRTFILE || "",
    "SSLCrtFilePwd": process.env.SSL_CRTFILEPWD || ""
};

// Set Roles
let roleList = ["Provider"];

// Create Node
console.log(`Starting DRP Node`);
let myNode = new DRP_Node(roleList, hostID, domainName, meshKey, zoneName, myServerConfig, drpWSRoute);
myNode.Debug = debug;
myNode.TestMode = testMode;
myNode.RegistryUrl = registryUrl;
myNode.ConnectToMesh(async () => {

    // Add Cache Service
    let myService = new CacheManager(serviceName, myNode, priority, weight, scope, mongoHost, mongoUser, mongoPw);
    myNode.AddService(myService);

    if (myNode.ListeningName) {
        myNode.log(`Listening at: ${myNode.ListeningName}`);
    }
});
