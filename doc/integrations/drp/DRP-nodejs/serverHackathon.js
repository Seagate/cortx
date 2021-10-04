'use strict';
const DRP_Node = require('drp-mesh').Node;
const DRP_WebServer = require('drp-mesh').WebServer;
const vdmServer = require('drp-service-rsage').VDM;
const DocMgr = require('drp-service-docmgr');
const DRP_AuthRequest = require('drp-mesh').Auth.DRP_AuthResponse;
const DRP_AuthResponse = require('drp-mesh').Auth.DRP_AuthResponse;
const DRP_Authenticator = require('drp-mesh').Auth.DRP_Authenticator;
const FedExAPIMgr = require('drp-service-fedex');
const os = require("os");

var protocol = "ws";
if (process.env.SSL_ENABLED) {
    protocol = "wss";
}
var port = process.env.PORT || 8080;
let listeningName = process.env.LISTENINGNAME || os.hostname();
let hostID = process.env.HOSTID || os.hostname();
let domainName = process.env.DOMAINNAME || null;
let meshKey = process.env.MESHKEY || null;
let zoneName = process.env.ZONENAME || "MyZone";
let debug = process.env.DEBUG || false;
let testMode = process.env.TESTMODE || false;

let apiKey = process.env.APIKEY || null;
let secretKey = process.env.SECRETKEY || null;
let serviceBaseURL = process.env.SERVICEBASEURL || null;
let shippingAccount = process.env.SHIPPINGACCOUNT || null;

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

let webRoot = process.env.WEBROOT || "webroot";

// Set Roles
let roleList = ["Broker", "Registry"];

// Create Node
console.log(`Starting DRP Node`);
let myNode = new DRP_Node(roleList, hostID, domainName, meshKey, zoneName, myServerConfig, drpWSRoute);
myNode.Debug = debug;
myNode.TestMode = testMode;
myNode.ConnectToMesh(async () => {
    // Test Authentication Service
    let myAuthenticator = new DRP_Authenticator("TestAuthenticator", myNode, 10, 10, "global", 1);
    /**
     * Authenticate User
     * @param {DRP_AuthRequest} authRequest Parameters to authentication function
     * @returns {DRP_AuthResponse} Response from authentication function
     */
    myAuthenticator.Authenticate = async function (authRequest) {
        let thisService = this;
        let authResponse = null;
        console.dir(authRequest);
        if (authRequest.UserName && authRequest.Password) {
            // For demo purposes; accept any user/password or token
            authResponse = new DRP_AuthResponse(thisService.GetToken(), authRequest.UserName, "Some User", ["Users"], null, thisService.serviceName, thisService.DRPNode.getTimestamp());
        }
        myNode.TopicManager.SendToTopic("AuthLogs", authResponse);
        return authResponse;
    };

    myNode.AddService(myAuthenticator);

    // Create VDM Server on node
    let myVDMServer = new vdmServer("VDM", myNode, webRoot, "vdmapplets");

    myNode.AddService(myVDMServer);
    myNode.EnableREST("/Mesh", "Mesh");

    // Add another service for demo
    let myService = new DocMgr("DocMgr", myNode, 10, 10, "global", "jsondocs", null, null, null);
    myNode.AddService(myService);

    // Add a FedEx service
    myNode.AddService(new FedExAPIMgr("FedEx", myNode, 10, 10, "global", apiKey, secretKey, shippingAccount, serviceBaseURL));

    if (myNode.ListeningName) {
        myNode.log(`Listening at: ${myNode.ListeningName}`);
    }
});
