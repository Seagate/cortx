'use strict';

const DRP_Service = require('drp-mesh').Service;

const MongoClient = require('mongodb').MongoClient;
const MongoDB = require('mongodb').Db;
const fs = require('fs').promises;

let openAPIDoc = {
    "openapi": "3.0.1",
    "info": {
        "title": "DocMgr",
        "description": "This API is used to store documents",
        "version": "1.0.1"
    },
    "servers": [
        {
            "url": "/Mesh/Services/DocMgr"
        }
    ],
    "tags": [
        {
            "name": "ClientCmds",
            "description": "Service ClientCmds"
        }
    ],
    "paths": {
        "/ClientCmds/listDocs": {
            "get": {
                "tags": [
                    "ClientCmds"
                ],
                "summary": "List documents",
                "description": "Returns list of documents",
                "operationId": "listDocs",
                "responses": {
                    "200": {
                        "description": "Found list of documents",
                        "content": {
                            "application/json": {
                                "schema": {
                                    "type": "object"
                                }
                            }
                        }
                    },
                    "400": {
                        "description": "Invalid query",
                        "content": {}
                    },
                    "404": {
                        "description": "No documents",
                        "content": {}
                    }
                },
                "x-swagger-router-controller": "DocMgr"
            }
        },
        "/ClientCmds/loadDoc/{docName}": {
            "get": {
                "tags": [
                    "ClientCmds"
                ],
                "summary": "Get document",
                "description": "Returns document",
                "operationId": "loadDoc",
                "parameters": [
                    {
                        "name": "docName",
                        "in": "path",
                        "description": "Doc name to retrieve",
                        "required": true,
                        "schema": {
                            "type": "string"
                        }
                    }
                ],
                "responses": {
                    "200": {
                        "description": "successful operation",
                        "content": {
                            "application/json": {
                                "schema": {
                                    "type": "object"
                                }
                            }
                        }
                    },
                    "400": {
                        "description": "Invalid document name supplied",
                        "content": {}
                    },
                    "404": {
                        "description": "Document not found",
                        "content": {}
                    }
                },
                "x-swagger-router-controller": "DocMgr"
            }
        }
    },
    "components": {
        "securitySchemes": {
            "x-api-key": {
                "type": "apiKey",
                "name": "x-api-key",
                "in": "header"
            },
            "x-api-token": {
                "type": "apiKey",
                "name": "x-api-token",
                "in": "header"
            }
        }
    },
    "security": [
        { "x-api-key": [] },
        { "x-api-token": [] }
    ]
};

class DocManager extends DRP_Service {
    /**
     * 
     * @param {string} serviceName Service Name
     * @param {drpNode} drpNode DRP Node
     * @param {number} priority Priority (lower better)
     * @param {number} weight Weight (higher better)
     * @param {string} scope Scope [local|zone|global(defaut)]
     * @param {string} basePath Base path
     * @param {string} mongoHost Mongo Host
     * @param {string} mongoUser Mongo User
     * @param {string} mongoPw Mongo Password
     */
    constructor(serviceName, drpNode, priority, weight, scope, basePath, mongoHost, mongoUser, mongoPw) {
        super(serviceName, drpNode, "DocManager", null, false, priority, weight, drpNode.Zone, scope, null, null, 1);
        let thisDocMgr = this;
        this.basePath = basePath;

        /** @type {string} Mongo URL */
        this.__MongoHost = mongoHost;
        this.__MongoUser = mongoUser;
        this.__MongoPw = mongoPw;

        /** @type {MongoClient} */
        this.__MongoClient = null;

        /** @type {MongoDB} */
        this.__DocMgrDB = null;

        if (thisDocMgr.__MongoHost) thisDocMgr.ConnectToMongo();

        this.ClientCmds = {
            /*
            getOpenAPIDoc: async function (params) {
                return openAPIDoc;
            },
            */
            listServices: async () => {
                // return list of service
                return await thisDocMgr.ListDocServices();
            },
            listDocs: async function (params) {
                let serviceName = null;
                if (params.serviceName) {
                    serviceName = params.serviceName;
                } else if (params.pathList && params.pathList.length > 0) {
                    serviceName = params.pathList[0];
                } else {
                    return null;
                }
                let docList = await thisDocMgr.ListDocs(serviceName);
                return docList;
            },
            loadDoc: async function (params) {
                let docName = null;
                if (params.serviceName && params.docName) {
                    serviceName = params.serviceName;
                    docName = params.docName;
                } else if (params.pathList && params.pathList.length > 1) {
                    serviceName = params.pathList[0];
                    docName = params.pathList[1];
                } else {
                    return null;
                }
                let docData = await thisDocMgr.LoadDoc(serviceName, docName);
                return docData;
            },
            saveDoc: async function (params) {
                await thisDocMgr.SaveDoc(params.serviceName, params.docName, params.docData);
                return "Saved";
            }
        };

        this.DocServices = async (params) => {
            let serviceName = null;
            let docName = null;
            if (params.pathList) {
                serviceName = params.pathList.shift();
                docName = params.pathList.shift();
            }
            if (params.serviceName) serviceName = params.serviceName;
            if (params.docName) docName = params.docName;

            if (!serviceName) {
                // return list of service
                return await thisDocMgr.ListDocServices();
            } else if (!docName) {
                // return list of docs
                return await thisDocMgr.ListDocs(serviceName);
            } else {
                // return doc
                return await thisDocMgr.LoadDoc(serviceName, docName); 
            }
        };
    }

    async ConnectToMongo() {
        let thisDocMgr = this;
        const user = encodeURIComponent(thisDocMgr.__MongoUser);
        const password = encodeURIComponent(thisDocMgr.__MongoPw);
        const authMechanism = 'DEFAULT';
        let mongoUrl = `mongodb://${user}:${password}@${thisDocMgr.__MongoHost}:27017/?authMechanism=${authMechanism}`;
        thisDocMgr.DRPNode.log(`Trying to connect to Mongo -> [${mongoUrl}]`);
        /** @type {MongoClient} */
        thisDocMgr.__MongoClient = await MongoClient.connect(`${mongoUrl}`, { useNewUrlParser: true, useUnifiedTopology: true });
        let bob = 1;
        // Open the collector DB 
        this.__DocMgrDB = thisDocMgr.__MongoClient.db(thisDocMgr.serviceName);
    }

    async ListDocServices() {
        let thisDocMgr = this;
        let returnData = [];
        // Load doc data
        let dirData = null;
        if (thisDocMgr.__MongoHost) {
            let docCollectionList = await thisDocMgr.__DocMgrDB.listCollections().toArray();
            returnData = docCollectionList.map(collectionProfile => { return collectionProfile["name"]; });
        } else {
            dirData = await fs.readdir(thisDocMgr.basePath);
            for (var i = 0; i < dirData.length; i++) {
                let serviceName = dirData[i];

                // Make sure this is a file
                let pathStat = await fs.stat(thisDocMgr.basePath  + '/' + serviceName);
                if (pathStat.isDirectory()) {
                    returnData.push(serviceName);
                }
            }
        }
        return returnData;
    }

    /**
     * 
     * @param {string} serviceName Name of service
     * @returns {Object.<string,object>} Dictionary of files and attributes
     */
    async ListDocs(serviceName) {
        let thisDocMgr = this;
        let returnData = [];
        // Load doc data
        let dirData = null;
        if (thisDocMgr.__MongoHost) {
            let serviceDocCollection = thisDocMgr.__DocMgrDB.collection(serviceName);
            let docList = await serviceDocCollection.find({}, { projection: { _id: 0, docName: 1 } }).toArray();
            returnData = docList.map(collectionProfile => { return collectionProfile["docName"]; });
        } else {
            dirData = await fs.readdir(thisDocMgr.basePath + '/' + serviceName);
            for (var i = 0; i < dirData.length; i++) {
                let docName = dirData[i];

                // Make sure this is a file
                let pathStat = await fs.stat(thisDocMgr.basePath + '/' + serviceName + '/' + docName);
                if (!pathStat.isDirectory()) {
                    returnData.push(docName);
                }
            }
        }
        return returnData;
    }

    async LoadDoc(serviceName, docName) {
        let thisDocMgr = this;
        // Load file data
        let docData = null;
        if (this.__MongoHost) {
            // Connect to Service doc collection
            let serviceDocCollection = thisDocMgr.__DocMgrDB.collection(serviceName);
            let docObj = await serviceDocCollection.findOne({ docName: docName });
            if (docObj && docObj.docData) docData = docObj.docData;
        } else {
            docData = await fs.readFile(`${thisDocMgr.basePath}/${serviceName}/${docName}`, 'utf8');
        }

        return docData;
    }

    async SaveDoc(serviceName, docName, docData) {
        let thisDocMgr = this;

        // Verify necessary attributes are not empty
        if (!serviceName || serviceName.length === 0) return "Cannot save - the serviceName parameter is null or empty";
        if (!docName || docName.length === 0) return "Cannot save - the docName parameter is null or empty";
        if (!docData || docData.length === 0) return "Cannot save - the docData parameter is null or empty";

        // Save file data
        let saveResults = null;
        if (this.__MongoHost) {
            // Connect to Service doc collection
            let serviceDocCollection = thisDocMgr.__DocMgrDB.collection(serviceName);
            saveResults = await serviceDocCollection.updateOne({ docName: docName }, { $set: { docData: docData } }, { upsert: true });
        } else {
            saveResults = await fs.writeFile(`${thisDocMgr.basePath}/${serviceName}/${docName}`, docData);
        }
        return saveResults;
    }
}

module.exports = DocManager;