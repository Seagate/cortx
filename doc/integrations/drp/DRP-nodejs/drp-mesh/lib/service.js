'use strict';

const UMLClass = require('./uml').Class;
//const DRP_Node = require('./node');

function getRandomInt(max) {
    return Math.floor(Math.random() * Math.floor(max));
}

class DRP_Service {
    /**
     * 
     * @param {string} serviceName Service Name
     * @param {DRP_Node} drpNode DRP Node
     * @param {string} type Service Type
     * @param {string} instanceID Instance ID
     * @param {boolean} sticky Stickiness
     * @param {number} priority Lower better
     * @param {number} weight Higher better
     * @param {string} zone Declared zone
     * @param {string} scope Availability Local|Zone|Global
     * @param {string[]} dependencies Peer service dependencies
     * @param {string[]} streams Streams provided
     * @param {number} status Service status [0|1|2]
     */
    constructor(serviceName, drpNode, type, instanceID, sticky, priority, weight, zone, scope, dependencies, streams, status) {
        this.serviceName = serviceName;
        this.DRPNode = drpNode;
        this.ClientCmds = {};
        /** @type Object.<string,UMLClass> */
        this.Classes = {};
        this.Type = type;
        this.InstanceID = instanceID || `${this.DRPNode.NodeID}-${serviceName}-${getRandomInt(9999)}`;
        this.Sticky = sticky;
        this.Priority = priority || 10;
        this.Weight = weight || 10;
        this.Zone = zone || "DEFAULT";
        this.Scope = scope || "global";
        this.Dependencies = dependencies || [];
        this.Streams = streams || [];
        this.Status = status || 0;
        this.isCacheable = false;
        this.lastSnapTime = null;
        this.snapIsRunning = false;
        this.snapStartTime = null;
        this.snapEndTime = null;
        this.snapEndStatus = null;
    }

    /**
     * 
     * @param {UMLClass} umlClass New Class definition
     */
    AddClass(umlClass) {
        this.Classes[umlClass.Name] = umlClass;
    }

    GetDefinition() {
        let thisService = this;
        let serviceDefinition = {
            InstanceID: thisService.InstanceID,
            Name: thisService.serviceName,
            Type: thisService.Type,
            Scope: thisService.Scope,
            Zone: thisService.Zone,
            Classes: {},
            ClientCmds: Object.keys(thisService.ClientCmds),
            Streams: thisService.Streams,
            Status: thisService.Status,
            Sticky: thisService.Sticky,
            Weight: thisService.Weight,
            Priority: thisService.Priority,
            Dependencies: thisService.Dependencies
        };

        // Loop over classes, get defs (excluding caches)
        let classNameList = Object.keys(thisService.Classes);
        for (let i = 0; i < classNameList.length; i++) {
            let className = classNameList[i];
            serviceDefinition.Classes[className] = thisService.Classes[className].GetDefinition();
        }
        return serviceDefinition;
    }

    InitiateSnap() {
        let thisService = this;
        let returnData = {
            'status': null,
            'data': null
        };
        if (!thisService.isCacheable) {
            // Not marked as cacheable, don't bother executing RunSnap
            returnData = {
                'status': 'SERVICE NOT CACHEABLE',
                'data': null
            };
        } else if (thisService.snapIsRunning) {
            // Already running, kick back an error
            returnData = {
                'status': 'SNAP ALREADY RUNNING',
                'data': { 'snapStartTime': thisService.snapStartTime }
            };
        } else {

            let runSnap = async () => {
                thisService.snapStartTime = new Date().toISOString();
                thisService.snapIsRunning = true;
                thisService.snapEndStatus = null;

                // Run the provider specific snap logic
                try {
                    await thisService.RunSnap();
                    thisService.snapEndMsg = "OK";
                } catch (ex) {
                    thisService.snapEndMsg = ex;
                }

                thisService.snapEndTime = new Date().toISOString();
                thisService.lastSnapTime = thisService.snapStartTime;
                thisService.snapIsRunning = false;
            };

            runSnap();

            // Return output from collector
            returnData = {
                'status': 'SNAP INITIATED',
                'data': { 'snapStartTime': thisService.snapStartTime }
            };
        }
        return returnData;
    }

    async RunSnap() {
        // This is a placeholder; derived classes should override this method
    }

    async ReadClassCacheFromService(className) {
        let thisService = this;
        let replyObj = await thisService.DRPNode.ServiceCmd("CacheManager", "readClassCache", { "serviceName": thisService.serviceName, "className": className }, null, null, false, true, null);
        if (replyObj.err) {
            thisService.DRPNode.log("Could not read cached objects for " + thisService.serviceName + "\\" + className + " -> " + replyObj.err);
            thisService.Classes[className].records = {};
            thisService.Classes[className].loadedCache = false;
        } else {
            thisService.lastSnapTime = replyObj.lastSnapTime;
            for (let objIdx in replyObj.docs) {
                let classObj = replyObj.docs[objIdx];
                let classObjPK = classObj['_objPK'];
                thisService.Classes[className].cache[classObjPK] = classObj;
            }

            thisService.DRPNode.log("Done reading cached objects for " + thisService.serviceName + "\\" + className);
        }
        thisService.Classes[className].loadedCache = true;
        return null;
    }

    async WriteClassCacheToService(className, cacheData) {
        let thisService = this;

        // Reject if no data
        if (Object.keys(cacheData).length === 0) {
            thisService.DRPNode.log("No collector records to insert for  " + thisService.serviceName + "/" + className);
            return null;
        } else {
            let replyObj = await thisService.DRPNode.ServiceCmd("CacheManager", "writeClassCache", {
                "serviceName": thisService.serviceName,
                "className": className,
                "cacheData": cacheData,
                "snapTime": thisService.snapStartTime
            }, null, null, false, true, null);
            return replyObj;
        }
    }

    async LoadClassCaches() {
        let thisService = this;
        let classNames = Object.keys(thisService.Classes);
        for (let i = 0; i < classNames.length; i++) {
            await thisService.ReadClassCacheFromService(classNames[i]);
        }
    }

    /**
     * Send a command to peer services
     * @param {string} method Method name
     * @param {object} params Method parameters
     */
    PeerBroadcast(method, params) {
        let thisService = this;

        // Get list of peer service IDs
        let peerServiceIDList = thisService.DRPNode.TopologyTracker.FindServicePeers(thisService.InstanceID);

        // Loop over peers, broadcast command
        for (let i = 0; i < peerServiceIDList.length; i++) {
            let peerServiceID = peerServiceIDList[i];
            thisService.DRPNode.ServiceCmd(thisService.serviceName, method, params, null, peerServiceID);
        }
    }

    /**
     * Get parameters for Service Method
     * @param {DRP_SvcMethodParams} params Parameters object
     * @param {string[]} paramNames Ordered list of parameters to extract
     * @returns {object}
     */
    GetParams(params, paramNames) {
        /*
         * Parameters can be passed three ways:
         *   - Ordered list of remaining path elements (params.pathList[paramNames[x]])
         *   - POST or PUT body (params.body.myVar)
         *   - Directly in params (params.myVar)
        */
        let returnObj = {};
        if (!paramNames || !Array.isArray(paramNames)) return returnObj;
        for (let i = 0; i < paramNames.length; i++) {
            returnObj[paramNames[i]] = null;
            // First, see if the parameters were part of the remaining path (CLI or REST)
            if (params.pathList && Array.isArray(params.pathList)) {
                if (typeof params.pathList[i] !== 'undefined') {
                    returnObj[paramNames[i]] = params.pathList[i];
                }
            }

            // Second, see if the parameters were passed in the body (REST)
            if (params.body && typeof params.body[paramNames[i]] !== 'undefined') {
                returnObj[paramNames[i]] = params.body[paramNames[i]];
            }

            // Third, see if the parameters were passed directly in the params (DRP Exec)
            if (typeof params[paramNames[i]] !== 'undefined') {
                returnObj[paramNames[i]] = params[paramNames[i]];
            }
        }
        return returnObj;
    }
}

class DRP_SvcMethodParams {
    constructor() {
        this.body = {};
        this.pathList = [];
    }
}

module.exports = DRP_Service;