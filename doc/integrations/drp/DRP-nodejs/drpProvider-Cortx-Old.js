'use strict';
const DRP_Node = require('drp-mesh').Node;
const DRP_Service = require('drp-mesh').Service;
const DRP_WebServerConfig = require('drp-mesh').WebServer.DRP_WebServerConfig;
const vdmServer = require('drp-service-rsage').VDM;
const DocMgr = require('drp-service-docmgr');
const DRP_AuthRequest = require('drp-mesh').Auth.DRP_AuthResponse;
const DRP_AuthResponse = require('drp-mesh').Auth.DRP_AuthResponse;
const DRP_Authenticator = require('drp-mesh').Auth.DRP_Authenticator;
const DRP_UMLAttribute = require('drp-mesh').UML.Attribute;
const DRP_UMLFunction = require('drp-mesh').UML.Function;
const DRP_UMLClass = require('drp-mesh').UML.Class;
const rSageHive = require('drp-service-rsage').Hive;
const DRP_Logger = require('drp-service-logger');
const os = require("os");
const AWS = require('aws-sdk');

// Must ignore cert errors due to Cortx certs
process.env.NODE_TLS_REJECT_UNAUTHORIZED = 0;

let hostID = process.env.HOSTID || os.hostname();
let domainName = process.env.DOMAINNAME || "";
let meshKey = process.env.MESHKEY || "supersecretkey";
let zoneName = process.env.ZONENAME || "MyZone";
let debug = process.env.DEBUG || false;
let testMode = process.env.TESTMODE || false;

let s3Endpoint = process.env.S3ENDPOINT || null;
let s3AccessKeyID = process.env.S3ACCESSKEYID || null;
let s3SecretAccessKey = process.env.S3SECRETACCESSKEY || null;

let drpWSRoute = "";

class DRP_Service_CortxStorage extends DRP_Service {
    constructor(serviceName, drpNode, type, instanceID, sticky, priority, weight, zone, scope, dependencies, streams, status, s3Endpoint, s3AccessKeyID, s3SecretAccessKey) {
        super(serviceName, drpNode, type, instanceID, sticky, priority, weight, zone, scope, dependencies, streams, status);
        let thisService = this;
        this.__S3 = new AWS.S3();
        this.__S3.config.s3ForcePathStyle = true;
        this.__S3.config.credentials = new AWS.Credentials(s3AccessKeyID, s3SecretAccessKey);
        this.__S3.endpoint = s3Endpoint;
        this.s3BucketName = this.serviceName.toLowerCase();

        //this.CreateBucketIfNotExists();

        Object.assign(this.ClientCmds, {
            listBuckets: async (params) => {
                let returnObj = await this.__S3.listBuckets().promise();
                return returnObj;
            },
            listObjects: async (params) => {
                let methodParams = ['bucket'];
                let parsedParams = thisService.GetParams(params, methodParams);
                let returnObj = null;
                if (!parsedParams.bucket) return "Must provide bucket name";
                returnObj = await this.ListObjects(parsedParams.bucket);
                return returnObj;
            },
            getObject: async (params) => {
                let methodParams = ['bucket', 'key'];
                let parsedParams = thisService.GetParams(params, methodParams);
                let returnObj = null;
                if (!parsedParams.bucket) return "Must provide bucket name";
                returnObj = await this.GetObject(parsedParams.bucket, parsedParams.key);
                return returnObj;
            },
            getObjectFromJSON: async (params) => {
                let methodParams = ['bucket', 'key'];
                let parsedParams = thisService.GetParams(params, methodParams);
                let returnObj = null;
                if (!parsedParams.bucket) return "Must provide bucket name";
                returnObj = await this.GetObject(parsedParams.bucket, parsedParams.key, true);
                return returnObj;
            },
            putObject: async (params) => {
                let methodParams = ['bucket', 'key', 'body'];
                let parsedParams = thisService.GetParams(params, methodParams);
                let returnObj = null;
                if (!parsedParams.bucket) return "Must provide bucket name";
                returnObj = await this.PutObject(parsedParams.bucket, parsedParams.key, parsedParams.body);
                return returnObj;
            },
            getAllObjects: async (params) => {
                let methodParams = ['bucket'];
                let parsedParams = thisService.GetParams(params, methodParams);
                let returnObj = null;
                if (!parsedParams.bucket) return "Must provide bucket name";
                returnObj = await this.SearchObjects(parsedParams.bucket, {});
                return returnObj;
            }
        });
    }

    async VerifyS3Connection() {
        try {
            await this.__S3.listBuckets().promise();
            return null;
        } catch (ex) {
            return ex;
        }
    }

    async CreateBucketIfNotExists() {
        let listBucketResponse = await this.ListBuckets();
        let bucketExists = false;
        for (let bucketObj of listBucketResponse.Buckets) {
            if (bucketObj.Name === this.s3BucketName) {
                bucketExists = true;
                break;
            }
        }
        if (!bucketExists) {
            try {
                let createResponse = await this.__S3.createBucket({ Bucket: this.s3BucketName }).promise();
                this.DRPNode.log(`Created S3 Bucket for service ${this.serviceName}`);
            } catch (ex) {
                this.DRPNode.log(`Error creating S3 Bucket for service ${this.serviceName}: ${ex}`);
            }
        }
    }

    async ListBuckets() {
        let returnObj = await this.__S3.listBuckets().promise();
        return returnObj;
    }

    async ListObjects(bucketName) {
        let returnObj = await this.__S3.listObjects({ Bucket: bucketName }).promise();
        return returnObj;
    }

    async GetObject(bucketName, objectKey, convertFromJSON) {
        let returnObj = null;
        let getObjectResponse = await this.__S3.getObject({ Bucket: bucketName, Key: objectKey }).promise();
        let attachment = getObjectResponse.Body.toString();
        if (convertFromJSON) {
            returnObj = JSON.parse(attachment);
        } else {
            returnObj = attachment;
        }
        return returnObj;
    }

    async PutObject(bucketName, objectKey, body) {
        let returnObj = await this.__S3.putObject({ Bucket: bucketName, Key: objectKey, Body: body }).promise();
        return returnObj;
    }

    async SearchObjects(bucketName, filter, returnKeysOnly) {
        let returnObjectList = await this.ListObjects(bucketName);
        let searchResults = [];
        let filterKeys = Object.keys(filter);
        for (let i = 0; i < returnObjectList.Contents.length; i++) {
            let checkKey = returnObjectList.Contents[i].Key;
            let rawObjectData = await this.GetObject(bucketName, checkKey, false);
            let objectData = null;
            try {
                objectData = JSON.parse(rawObjectData);
            }
            catch (ex) {
                objectData = rawObjectData;
            }
            let objectMatches = true;
            for (let j = 0; j < filterKeys.length; j++) {
                let filterField = filterKeys[j];
                let filterValue = filter[filterField];
                if (typeof objectData !== "object" || objectData[filterField] !== filterValue) {
                    objectMatches = false;
                    break;
                }
            }
            if (objectMatches) {
                if (returnKeysOnly) {
                    searchResults.push(checkKey);
                } else {
                    searchResults.push(objectData);
                }
            }
        }
        return searchResults;
    }

    async WriteCacheToCortx() {
        let classNameList = Object.keys(this.Classes);
        for (let i = 0; i < classNameList.length; i++) {
            let thisClassName = classNameList[i];
            // Loop over records
            let classObjectKeyList = Object.keys(this.Classes[thisClassName].cache);
            for (let j = 0; j < classObjectKeyList.length; j++) {
                let thisClassObjectKey = classObjectKeyList[j];
                let thisClassObjectData = this.Classes[thisClassName].cache[thisClassObjectKey];
                let objectKey = `${thisClassName}-${thisClassObjectKey}`;
                try {
                    let results = await this.PutObject(this.s3BucketName, objectKey, thisClassObjectData.ToString());
                } catch (ex) {
                    this.DRPNode.log(`Could not create S3 object ${this.s3BucketName}:${objectKey}: ${ex}`);
                }
            }
        }
    }

    async ReadCacheFromCortx() {
        let objectListResponse = await this.ListObjects(this.s3BucketName);
        for (let objectRecord of objectListResponse.Contents) {
            let objectData = await this.GetObject(this.s3BucketName, objectRecord.Key, true);
            this.Classes[objectData['_objClass']].AddRecord(objectData, objectData['_serviceName'], objectData['_snapTime']);
        }
    }
}

// Civic Data Source classes
class FireDept extends DRP_Service_CortxStorage {
    constructor(serviceName, drpNode, priority, weight, scope, s3Endpoint, s3AccessKeyID, s3SecretAccessKey) {
        super(serviceName, drpNode, "FireDept", null, false, priority, weight, drpNode.Zone, scope, null, ['PublicSafety'], 1, s3Endpoint, s3AccessKeyID, s3SecretAccessKey);
        let thisService = this;
        this.Startup();
    }

    async Startup() {
        // Define data classes for this Provider
        this.AddClass(new DRP_UMLClass("Station", [],
            [
                new DRP_UMLAttribute("stationID", "stationID", null, false, "int", null, "1", "PK,MK"),
                new DRP_UMLAttribute("description", null, null, false, "string(128)", null, "1", null),
                new DRP_UMLAttribute("city", "city", null, false, "string(128)", null, "1", "FK"),
                new DRP_UMLAttribute("address", "address", null, false, "string(128)", null, "1", "FK")
            ],
            []
        ));

        this.AddClass(new DRP_UMLClass("Person", [],
            [
                new DRP_UMLAttribute("personID", "personID", null, false, "int", null, "1", "PK,MK"),
                new DRP_UMLAttribute("stationID", "stationID", null, false, "string(128)", null, "1", "FK"),
                new DRP_UMLAttribute("firstName", null, null, false, "string(128)", null, "1", null),
                new DRP_UMLAttribute("lastName", null, null, false, "string(128)", null, "1", null),
                new DRP_UMLAttribute("title", null, null, false, "string(128)", null, "1", null)
            ],
            []
        ));

        this.AddClass(new DRP_UMLClass("Equipment", [],
            [
                new DRP_UMLAttribute("stationID", "stationID", null, false, "string(128)", null, "1", "FK"),
                new DRP_UMLAttribute("equipmentID", "equipmentID", null, false, "string(128)", null, "1", "PK,MK"),
                new DRP_UMLAttribute("equipmentType", "equipmentType", null, false, "string(128)", null, "1", "FK"),
                new DRP_UMLAttribute("description", null, null, false, "string(128)", null, "1", null),
                new DRP_UMLAttribute("status", null, null, false, "int", null, "1", null)
            ],
            []
        ));

        // If sample data doesn't exist in CORTX, add it
        //let objectListResponse = await this.ListObjects(this.s3BucketName);
        //if (objectListResponse.Contents.length === 0) {
            let snapTime = "2021-04-27T08:00:00.000Z";

            // Add sample data records
            this.Classes['Station'].AddRecord({
                "stationID": 1000,
                "description": "East side of town",
                "city": "Springfield",
                "address": "123 Pine St"
            }, this.serviceName, snapTime);

            // Add sample data records
            this.Classes['Person'].AddRecord({
                "personID": 1001,
                "stationID": 1000,
                "firstName": "John",
                "lastName": "Smith",
                "title": "Firefighter"
            }, this.serviceName, snapTime);

            // Add sample data records
            this.Classes['Person'].AddRecord({
                "personID": 1002,
                "stationID": 1000,
                "firstName": "Bob",
                "lastName": "Smith",
                "title": "Firefighter"
            }, this.serviceName, snapTime);

            this.Classes['Equipment'].AddRecord({
                "stationID": 1000,
                "equipmentID": 1100,
                "description": "Old truck",
                "status": 0
            }, this.serviceName, snapTime);

            // Add sample data records
            this.Classes['Station'].AddRecord({
                "stationID": 2000,
                "description": "East side of town",
                "city": "Springfield",
                "address": "234 Lake St"
            }, this.serviceName, snapTime);

            // Add sample data records
            this.Classes['Person'].AddRecord({
                "personID": 2001,
                "stationID": 2000,
                "firstName": "Ted",
                "lastName": "Smith",
                "title": "Firefighter"
            }, this.serviceName, snapTime);

            // Add sample data records
            this.Classes['Person'].AddRecord({
                "personID": 2002,
                "stationID": 2000,
                "firstName": "Bill",
                "lastName": "Smith",
                "title": "Firefighter"
            }, this.serviceName, snapTime);

            this.Classes['Equipment'].AddRecord({
                "stationID": 2000,
                "equipmentID": 2100,
                "description": "New truck",
                "status": 1
            }, this.serviceName, snapTime);

            // Write demo data to CORTX
            //await this.WriteCacheToCortx();
        //}

        // Read demo data from CORTX
        //await this.ReadCacheFromCortx();

        // Mark cache loading as complete
        this.Classes['Station'].loadedCache = true;
        this.Classes['Person'].loadedCache = true;
        this.Classes['Equipment'].loadedCache = true;

        let sendRandomStreamData = () => {
          let min = 3,
          max = 10;
          var rand = Math.floor(Math.random() * (max - min + 1) + min); //Generate Random number between 5 - 10
          let timeStamp = new Date().getTime();
          myNode.TopicManager.SendToTopic("PublicSafety", `Fire Dept: a random fire event`);
          setTimeout(sendRandomStreamData, rand * 1000);
        }

        sendRandomStreamData()
    }
}

// Civic Data Source classes
class PoliceDept extends DRP_Service_CortxStorage {
    constructor(serviceName, drpNode, priority, weight, scope, s3Endpoint, s3AccessKeyID, s3SecretAccessKey) {
        super(serviceName, drpNode, "PoliceDept", null, false, priority, weight, drpNode.Zone, scope, null, ['PublicSafety'], 1, s3Endpoint, s3AccessKeyID, s3SecretAccessKey);
        let thisService = this;
        this.Startup();        
    }

    async Startup() {
        // Define data classes for this Provider
        this.AddClass(new DRP_UMLClass("Station", [],
            [
                new DRP_UMLAttribute("stationID", "stationID", null, false, "int", null, "1", "PK,MK"),
                new DRP_UMLAttribute("description", null, null, false, "string(128)", null, "1", null),
                new DRP_UMLAttribute("city", "city", null, false, "string(128)", null, "1", "FK"),
                new DRP_UMLAttribute("address", "address", null, false, "string(128)", null, "1", "FK")
            ],
            []
        ));

        this.AddClass(new DRP_UMLClass("Person", [],
            [
                new DRP_UMLAttribute("personID", "personID", null, false, "int", null, "1", "PK,MK"),
                new DRP_UMLAttribute("stationID", "stationID", null, false, "string(128)", null, "1", "FK"),
                new DRP_UMLAttribute("firstName", null, null, false, "string(128)", null, "1", null),
                new DRP_UMLAttribute("lastName", null, null, false, "string(128)", null, "1", null),
                new DRP_UMLAttribute("title", null, null, false, "string(128)", null, "1", null)
            ],
            []
        ));

        // If sample data doesn't exist in CORTX, add it
        //let objectListResponse = await this.ListObjects(this.s3BucketName);
        //if (objectListResponse.Contents.length === 0) {

            let snapTime = "2021-04-27T08:00:00.000Z";

            // Add sample data records
            this.Classes['Station'].AddRecord({
                "stationID": 3000,
                "description": "East side of town",
                "city": "Springfield",
                "address": "123 Pine St"
            }, this.serviceName, snapTime);

            // Add sample data records
            this.Classes['Person'].AddRecord({
                "personID": 3001,
                "stationID": 3000,
                "firstName": "John",
                "lastName": "Smith",
                "title": "Officer"
            }, this.serviceName, snapTime);

            // Add sample data records
            this.Classes['Person'].AddRecord({
                "personID": 3002,
                "stationID": 3000,
                "firstName": "Bob",
                "lastName": "Smith",
                "title": "Officer"
            }, this.serviceName, snapTime);

            // Add sample data records
            this.Classes['Station'].AddRecord({
                "stationID": 4000,
                "description": "East side of town",
                "city": "Springfield",
                "address": "234 Lake St"
            }, this.serviceName, snapTime);

            // Add sample data records
            this.Classes['Person'].AddRecord({
                "personID": 4001,
                "stationID": 4000,
                "firstName": "Ted",
                "lastName": "Smith",
                "title": "Officer"
            }, this.serviceName, snapTime);

            // Add sample data records
            this.Classes['Person'].AddRecord({
                "personID": 4002,
                "stationID": 4000,
                "firstName": "Bill",
                "lastName": "Smith",
                "title": "Officer"
            }, this.serviceName, snapTime);

            // Write demo data to CORTX
            //await this.WriteCacheToCortx();
        //}

        // Read demo data from CORTX
        //await this.ReadCacheFromCortx();

        // Mark cache loading as complete
        this.Classes['Station'].loadedCache = true;
        this.Classes['Person'].loadedCache = true;

        let sendRandomStreamData = () => {
          let min = 3,
          max = 10;
          var rand = Math.floor(Math.random() * (max - min + 1) + min); //Generate Random number between 5 - 10
          let timeStamp = new Date().getTime();
          myNode.TopicManager.SendToTopic("PublicSafety", `Police Dept: a random law enforcement event`);
          setTimeout(sendRandomStreamData, rand * 1000);
        }

        sendRandomStreamData();
    }
}

// Set Roles
let roleList = ["Provider"];

// Create Node
console.log(`Starting DRP Node`);
let myNode = new DRP_Node(roleList, hostID, domainName, meshKey, zoneName);
myNode.Debug = debug;
myNode.TestMode = testMode;
myNode.ConnectToMesh(async () => {

    let fireService = new FireDept("FireDept", myNode, 10, 10, "global", s3Endpoint, s3AccessKeyID, s3SecretAccessKey);
    /*
    let verifyS3ConnErr = await fireService.VerifyS3Connection()
    if (verifyS3ConnErr) {
        myNode.log(`Error connecting to S3: ${verifyS3ConnErr}`);
        process.exit(1);
    }
    */
    myNode.AddService(fireService);

    let policeService = new PoliceDept("PoliceDept", myNode, 10, 10, "global", s3Endpoint, s3AccessKeyID, s3SecretAccessKey);
    myNode.AddService(policeService);

    async function wait(ms) {
        return new Promise(resolve => {
            setTimeout(resolve, ms);
        });
    }
});

