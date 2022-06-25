'use strict';

const DRP_Service = require('drp-mesh').Service;

const MongoClient = require('mongodb').MongoClient;
const MongoDB = require('mongodb').Db;

const assert = require('assert');

require('events').EventEmitter.prototype._maxListeners = 100;

class DRP_CacheManager extends DRP_Service {
    /**
    * @param {string} serviceName Service Name
    * @param {drpNode} drpNode DRP Node
    * @param {number} priority Priority (lower better)
    * @param {number} weight Weight (higher better)
    * @param {string} scope Scope [local|zone|global(defaut)]
    * @param {string} mongoHost Mongo Host
    * @param {string} mongoUser Mongo User
    * @param {string} mongoPw Mongo Password
    */
    constructor(serviceName, drpNode, priority, weight, scope, mongoHost, mongoUser, mongoPw) {
        super(serviceName, drpNode, "CacheManager", null, false, priority, weight, drpNode.Zone, scope, null, null, 1);
        let thisService = this;

        /** @type {string} Mongo URL */
        this.__MongoHost = mongoHost;
        this.__MongoUser = mongoUser;
        this.__MongoPw = mongoPw;

        /** @type {MongoClient} */
        this.__MongoClient = null;

        /** @type {MongoDB} */
        //this.CacheDB = null;

        if (thisService.__MongoHost) {
            thisService.ConnectToMongo();
        }

        //this.TestCache = null;

        this.ClientCmds = {
            "listDatabases": async (params) => {
                let returnObj = null;
                let adminDb = thisService.__MongoClient.db().admin();
                // List all the available databases
                let dbListResult = await adminDb.listDatabases();
                returnObj = dbListResult.databases;
                return returnObj;
            },
            "readClassCache": async (params) => {
                return await thisService.ReadClassCacheFromMongo(params.serviceName, params.className);
            },
            "writeClassCache": async (params) => {
                return await thisService.WriteClassCacheToMongo(params.serviceName, params.className, params.cacheData, params.snapTime);
            }
        };
    }

    async ConnectToMongo() {
        let thisService = this;
        const user = encodeURIComponent(thisService.__MongoUser);
        const password = encodeURIComponent(thisService.__MongoPw);
        const authMechanism = 'DEFAULT';
        let mongoUrl = thisService.__MongoUser ? `mongodb://${user}:${password}@${thisService.__MongoHost}:27017/?authMechanism=${authMechanism}` : `mongodb://${thisService.__MongoHost}:27017`;
        thisService.DRPNode.log(`Trying to connect to Mongo -> [${mongoUrl}]`);
        /** @type {MongoClient} */
        thisService.__MongoClient = await MongoClient.connect(`${mongoUrl}`, { useNewUrlParser: true, useUnifiedTopology: true });
    }

    async ReadClassCacheFromMongo(serviceName, className) {
        let thisService = this;
        return new Promise(function (resolve, reject) {
            // Open the collector DB 
            var serviceDB = thisService.__MongoClient.db(serviceName);
            // Connect to class collection
            var classCollection = serviceDB.collection(className);

            var configCollection = serviceDB.collection('collectorInfo');

            configCollection.findOne({ 'className': className }, function (err, document) {
                assert.equal(err, null);
                //assert.equal(1, docs.length);
                thisService.DRPNode.log('className: ' + className + ", data: " + JSON.stringify(document));

                if (document) {
                    classCollection.find({ '_snapTime': document.lastSnapTime }).toArray(function (err, docs) {
                        resolve({ "err": err, "className": className, "docs": docs, "lastSnapTime": document.lastSnapTime });
                    });
                } else {
                    // No snap exists!
                    resolve({ "err": "No snap exists for className: " + className, "className": className });
                }
            });
        });
    }

    async WriteClassCacheToMongo(serviceName, className, cacheData, snapTime) {
        let thisService = this;
        return new Promise(function (resolve, reject) {
            // Reject if no data
            if (Object.keys(cacheData).length === 0) {
                thisService.DRPNode.log("No collector records to insert for  " + serviceName + "/" + className);
                resolve();
            } else {

                // Open the collector DB 
                let collectorDB = thisService.__MongoClient.db(serviceName);
                // Connect to class collection
                let classCollection = collectorDB.collection(className);
                // Convert class data from hash to array
                let classDataAsArray = [];
                for (let objPK in cacheData) {
                    classDataAsArray.push(cacheData[objPK]);
                }

                let configCollection = collectorDB.collection('collectorInfo');

                configCollection.update({ 'className': className }, { 'className': className, 'lastSnapTime': snapTime }, { upsert: true }, function (err, result) {
                    if (err) {
                        reject(err);
                    }
                    classCollection.insert(classDataAsArray, { checkKeys: false }, function (err, result) {
                        if (err) {
                            thisService.DRPNode.log("Error inserting collector records: " + err);
                            reject(err);
                        } else {
                            thisService.DRPNode.log("Inserted collector records for  " + serviceName + "/" + className);
                            resolve();
                        }
                    });
                });
            }
        });
    }
}

module.exports = DRP_CacheManager;