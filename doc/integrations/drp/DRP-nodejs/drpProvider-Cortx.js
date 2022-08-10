'use strict';
const DRP_Node = require('drp-mesh').Node;
const DRP_Service = require('drp-mesh').Service;
const AWS = require('aws-sdk');
const os = require("os");

let hostID = process.env.HOSTID || os.hostname();
let domainName = process.env.DOMAINNAME || "";
let meshKey = process.env.MESHKEY || "supersecretkey";
let zoneName = process.env.ZONENAME || "MyZone";
let registryUrl = process.env.REGISTRYURL || null;
let debug = process.env.DEBUG || false;
let testMode = process.env.TESTMODE || false;

let s3Endpoint = process.env.S3ENDPOINT || null;
let s3AccessKeyID = process.env.S3ACCESSKEYID || null;
let s3SecretAccessKey = process.env.S3SECRETACCESSKEY || null;

// Must ignore cert errors due to Cortx certs
process.env.NODE_TLS_REJECT_UNAUTHORIZED = 0;

// Set Roles
let roleList = ["Provider"];

// Create Cortx service class
class CortxService extends DRP_Service {
    constructor(serviceName, drpNode, priority, weight, scope) {
        super(serviceName, drpNode, "CortxService", null, false, priority, weight, drpNode.Zone, scope, null, [], 1);
        let thisService = this;

        this.S3 = new AWS.S3();
        this.S3.config.s3ForcePathStyle = true;
        this.S3.config.credentials = new AWS.Credentials(s3AccessKeyID, s3SecretAccessKey);
        this.S3.endpoint = s3Endpoint;

        // Define global methods
        this.ClientCmds = {
            listBuckets: async (params) => {
                let returnObj = await this.S3.listBuckets().promise();
                return returnObj;
            },
            listObjects: async (params) => {
                let methodParams = ['bucket'];
                let parsedParams = thisService.GetParams(params, methodParams);
                let returnObj = null;
                if (!parsedParams.bucket) return "Must provide bucket name";
                returnObj = await this.S3.listObjects({ Bucket: parsedParams.bucket }).promise();
                return returnObj;
            },
            getObject: async (params) => {
                let methodParams = ['bucket', 'key'];
                let parsedParams = thisService.GetParams(params, methodParams);
                let returnObj = null;
                if (!parsedParams.bucket) return "Must provide bucket name";
                returnObj = await this.S3.getObject({ Bucket: parsedParams.bucket, Key: parsedParams.key }).promise();
                let attachment = returnObj.Body.toString();
                return attachment;
            },
            getObjectFromJSON: async (params) => {
                let methodParams = ['bucket', 'key'];
                let parsedParams = thisService.GetParams(params, methodParams);
                let returnObj = null;
                if (!parsedParams.bucket) return "Must provide bucket name";
                returnObj = await this.S3.getObject({ Bucket: parsedParams.bucket, Key: parsedParams.key }).promise();
                let attachment = JSON.parse(returnObj.Body.toString());
                return attachment;
            }
        };
    }
}

// Create Node
console.log(`Starting DRP Node`);
let myNode = new DRP_Node(roleList, hostID, domainName, meshKey, zoneName, null, null);
myNode.Debug = debug;
myNode.TestMode = testMode;
myNode.RegistryUrl = registryUrl;
myNode.ConnectToMesh(async () => {

    // Add Cortx service
    myNode.AddService(new CortxService("Cortx", myNode, 10, 10, "global"));
});
