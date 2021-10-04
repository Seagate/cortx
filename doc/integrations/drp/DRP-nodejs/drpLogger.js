'use strict';
const DRP_Node = require('drp-mesh').Node;
const DRP_WebServerConfig = require('drp-mesh').WebServer.DRP_WebServerConfig;
const DRP_Logger = require('drp-service-logger');
const os = require("os");

let protocol = "ws";
if (isTrue(process.env.SSL_ENABLED)) {
    protocol = "wss";
}
let isListening = isFalse(process.env.ISLISTENING);
let port = process.env.PORT || 8081;
let listeningName = process.env.LISTENINGNAME || os.hostname();
let hostID = process.env.HOSTID || os.hostname();
let domainName = process.env.DOMAINNAME || "";
let meshKey = process.env.MESHKEY || "supersecretkey";
let zoneName = process.env.ZONENAME || "MyZone";
let registryUrl = process.env.REGISTRYURL || null;
let debug = process.env.DEBUG || false;
let testMode = process.env.TESTMODE || false;

// Service specific variables
let serviceName = process.env.SERVICENAME || "Logger";
let priority = process.env.PRIORITY || null;
let weight = process.env.WEIGHT || null;
let scope = process.env.SCOPE || null;
let mongoHost = process.env.MONGOHOST || "localhost";
let mongoUser = process.env.MONGOUSER || null;
let mongoPw = process.env.MONGOPW || null;

let drpWSRoute = "";

/** @type DRP_WebServerConfig */
let myWebServerConfig = null;

// If we want the node to accept inbound connections, configure the web service
if (isListening) {
    // Set config
    myWebServerConfig = {
        "ListeningURL": `${protocol}://${listeningName}:${port}${drpWSRoute}`,
        "Port": port,
        "SSLEnabled": isTrue(process.env.SSL_ENABLED),
        "SSLKeyFile": process.env.SSL_KEYFILE || "",
        "SSLCrtFile": process.env.SSL_CRTFILE || "",
        "SSLCrtFilePwd": process.env.SSL_CRTFILEPWD || ""
    };
}

// Set Roles
let roleList = ["Logger"];

// Create Node
console.log(`Starting DRP Node`);
let myNode = new DRP_Node(roleList, hostID, domainName, meshKey, zoneName, myWebServerConfig, drpWSRoute);
myNode.Debug = debug;
myNode.TestMode = testMode;
myNode.RegistryUrl = registryUrl;
myNode.ConnectToMesh(async () => {

    // Add logger
    let logger = new DRP_Logger(serviceName, myNode, priority, weight, scope, mongoHost, mongoUser, mongoPw);
    myNode.AddService(logger);

    if (myNode.ListeningName) {
        myNode.log(`Listening at: ${myNode.ListeningName}`);
    }
});

function isTrue(value) {
    if (typeof (value) === 'string') {
        value = value.trim().toLowerCase();
    }
    switch (value) {
        case true:
        case "true":
        case 1:
        case "1":
        case "on":
        case "yes":
            return true;
        default:
            return false;
    }
}

function isFalse(value) {
    if (typeof (value) === 'string') {
        value = value.trim().toLowerCase();
    }
    switch (value) {
        case true:
        case "false":
        case 0:
        case "0":
        case "off":
        case "no":
            return false;
        default:
            return true;
    }
}