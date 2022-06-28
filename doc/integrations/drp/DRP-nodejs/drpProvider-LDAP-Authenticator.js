'use strict';
const DRP_Node = require('drp-mesh').Node;
const DRP_LDAP = require('drp-service-ldap');

const os = require("os");

let hostID = process.env.HOSTID || os.hostname();
let domainName = process.env.DOMAINNAME || null;
let meshKey = process.env.MESHKEY || null;
let zoneName = process.env.ZONENAME || null;
let registryUrl = process.env.REGISTRYURL || null;
let debug = process.env.DEBUG || false;
let testMode = process.env.TESTMODE || false;

// Service specific variables
let serviceName = process.env.SERVICENAME || "LDAPAuthenticator";
let priority = process.env.PRIORITY || null;
let weight = process.env.WEIGHT || null;
let scope = process.env.SCOPE || null;
let ldapURL = process.env.LDAPURL;
let userBase = process.env.USERBASE;
let userContainerType = process.env.USERCONTAINERTYPE || "cn";

// Set Roles
let roleList = ["Provider"];

// Create Node
console.log(`Starting DRP Node`);
let myNode = new DRP_Node(roleList, hostID, domainName, meshKey, zoneName);
myNode.Debug = debug;
myNode.TestMode = testMode;
myNode.RegistryUrl = registryUrl;
myNode.ConnectToMesh(async () => {
    // Add Authenticator Service
    let myAuthenticator = new DRP_LDAP(serviceName, myNode, priority, weight, scope, ldapURL, userBase, userContainerType, null, null, null);
    myNode.AddService(myAuthenticator);
});
