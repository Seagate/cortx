'use strict';
const DRP_Node = require('drp-mesh').Node;
const TestService = require('drp-service-test');
const DRP_WebServerConfig = require('drp-mesh').WebServer.DRP_WebServerConfig;
const vdmServer = require('drp-service-rsage').VDM;
const Hive = require('drp-service-rsage').Hive;
const DocMgr = require('drp-service-docmgr');
const DRP_AuthRequest = require('drp-mesh').Auth.DRP_AuthResponse;
const DRP_AuthResponse = require('drp-mesh').Auth.DRP_AuthResponse;
const DRP_Authenticator = require('drp-mesh').Auth.DRP_Authenticator;
const DRP_Logger = require('drp-service-logger');
const os = require("os");
const { DRP_PermissionSet, DRP_Permission } = require('drp-mesh/lib/securable');

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
let debug = process.env.DEBUG || false;
let testMode = process.env.TESTMODE || false;

let drpWSRoute = "";

// Set config
/** @type {DRP_WebServerConfig} */
let myWebServerConfig = {
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

let myNodePermissions = new DRP_PermissionSet();
myNodePermissions.Groups['Admins'] = new DRP_Permission(true, true, true);
myNodePermissions.Keys['ASDFJKL'] = new DRP_Permission(true, true, true);

// Create Node
console.log(`Starting DRP Node`);
let myNode = new DRP_Node(roleList, hostID, domainName, meshKey, zoneName, myWebServerConfig, drpWSRoute, myNodePermissions);
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
        //console.dir(authRequest);
        if (authRequest.UserName) {
            // For demo purposes; accept any user/password or token
            switch (authRequest.UserName) {
                case 'admin':
                case 'Admin':
                    authResponse = new DRP_AuthResponse(thisService.GetToken(), authRequest.UserName, "Admin User", ["Admins"], null, thisService.serviceName, thisService.DRPNode.getTimestamp());
                    break;
                default:
                    authResponse = new DRP_AuthResponse(thisService.GetToken(), authRequest.UserName, "Random User", ["Users"], null, thisService.serviceName, thisService.DRPNode.getTimestamp());
            }

            if (thisService.DRPNode.Debug) thisService.DRPNode.log(`Authenticate [${authRequest.UserName}] -> SUCCEEDED`);
            thisService.DRPNode.TopicManager.SendToTopic("AuthLogs", authResponse);
            thisService.DRPNode.ServiceCmd("Logger", "writeLog", { serviceName: thisService.serviceName, logData: authResponse });
        }
        return authResponse;
    };

    myNode.AddService(myAuthenticator);

    // Add logger
    //let logger = new DRP_Logger("Logger", myNode, 10, 10, "global", "localhost", null, null);
    //myNode.AddService(logger);

    // Create VDM Server on node
    let myVDMServer = new vdmServer("VDM", myNode, webRoot, "vdmapplets", "xrapplets", null);
    myNode.AddService(myVDMServer);
    myNode.EnableREST("/Mesh", "Mesh");

    // Add DocMgr service
    let myDocMgr = new DocMgr("DocMgr", myNode, 10, 10, "global", "jsondocs", null, null, null);
    myNode.AddService(myDocMgr);

    // Add TestService
    let myTestService = new TestService("TestService", myNode, 10, 10, "global");
    myNode.AddService(myTestService);

    // Add Hive
    let myHiveService = new Hive("Hive", myNode, 10, 10, "global");
    myHiveService.Start();
    myNode.AddService(myHiveService);

    if (myNode.ListeningName) {
        myNode.log(`Listening at: ${myNode.ListeningName}`);
    }
});
