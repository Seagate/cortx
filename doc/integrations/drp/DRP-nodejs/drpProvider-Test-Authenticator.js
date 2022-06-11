'use strict';
const DRP_Node = require('drp-mesh').Node;
const DRP_AuthRequest = require('drp-mesh').Auth.DRP_AuthResponse;
const DRP_AuthResponse = require('drp-mesh').Auth.DRP_AuthResponse;
const DRP_Authenticator = require('drp-mesh').Auth.DRP_Authenticator;
const os = require("os");

let hostID = process.env.HOSTID || os.hostname();
let domainName = process.env.DOMAINNAME || "";
let meshKey = process.env.MESHKEY || "supersecretkey";
let zoneName = process.env.ZONENAME || "MyZone";
let registryUrl = process.env.REGISTRYURL || null;
let debug = process.env.DEBUG || false;
let testMode = process.env.TESTMODE || false;

// Service specific variables
let serviceName = process.env.SERVICENAME || "TestAuthenticator";
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
    // Test Authentication Service
    let myAuthenticator = new DRP_Authenticator(serviceName, myNode, priority, weight, scope, 1);
    /**
     * Authenticate User
     * @param {DRP_AuthRequest} authRequest Parameters to authentication function
     * @returns {DRP_AuthResponse} Response from authentication function
     */
    myAuthenticator.Authenticate = async function (authRequest) {
        let thisService = this;
        let authResponse = null;
        //console.dir(authRequest);
        if (authRequest.UserName && authRequest.Password) {
            // For demo purposes; accept any user/password or token
            authResponse = new DRP_AuthResponse(thisService.GetToken(), authRequest.UserName, "Some User", ["Users"], null, thisService.serviceName, thisService.DRPNode.getTimestamp());
            if (thisService.DRPNode.Debug) thisService.DRPNode.log(`Authenticate [${authRequest.UserName}] -> SUCCEEDED`);
            thisService.DRPNode.TopicManager.SendToTopic("AuthLogs", authResponse);
            thisService.DRPNode.ServiceCmd("Logger", "writeLog", { serviceName: thisService.serviceName, logData: authResponse });
        }
        return authResponse;
    };

    myNode.AddService(myAuthenticator);
});
