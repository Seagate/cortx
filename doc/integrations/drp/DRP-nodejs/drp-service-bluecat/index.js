'use strict';

var DRP_Service = require('drp-mesh').Service;
let axios = require('axios');

class APIEntity {
    constructor(id, name, type, properties) {
        this.id = id;
        this.name = name;
        this.type = type;
        this.properties = properties;
    }

    PropertiesToObject(properties) {
        let thisAPIEntity = this;
        let returnObj = {};
        let splitString = properties || thisAPIEntity.properties;
        let keyValArray = splitString.split("|");
        for (let i = 0; i < keyValArray.length; i++) {
            let keyValPair = keyValArray[i].split("=");
            if (keyValPair.length === 2) {
                returnObj[keyValPair[0]] = keyValPair[1];
            }
        }
        return returnObj;
    }
}

class View extends APIEntity {
    constructor(...args) {
        super(...args);
    }
}

class HostRecord extends APIEntity {
    constructor(...args) {
        super(...args);
    }
}

class BlueCatMgmtHost {
    constructor(params) {
        this.name = params.name;
        this.mgmtIP = params.mgmtIP;
        this.__apiUser = params.apiUser;
        this.__apiPassword = params.apiPassword;
        /** @type {axios.default} */
        this.__restAgent = axios.create({
            baseURL: `https://${this.mgmtIP}/Services/REST/v1/`,
            timeout: 5000,
            headers: {},
            proxy: false
        });
        this.maxTokenAgeSec = 250;
        this.__authToken = null;
        this.tokenAcquiredTimestamp = null;
        this.config = {
            "ConfigurationID": 0,
            "Views": {}
        };
        /** @type {BlueCatManager} */
        this.bcMgr = params.bcMgr;
    }

    async GetAllSubZones(parentObjectID, targetZoneObj) {
        let thisBcMgmtHost = this;
        let subZoneObjList = await thisBcMgmtHost.GetEntities_All(parentObjectID, "Zone");

        for (let i = 0; i < subZoneObjList.length; i++) {
            let subZoneObj = subZoneObjList[i];
            let propertiesObj = APIEntity.prototype.PropertiesToObject(subZoneObj.properties);
            let absoluteName = propertiesObj['absoluteName'];
            subZoneObj.records = {};

            targetZoneObj[absoluteName] = subZoneObj;

            await thisBcMgmtHost.GetEntities_All(subZoneObj.id, "HostRecord", (hostRecords) => {
                for (let i = 0; i < hostRecords.length; i++) {
                    let thisHostRecord = hostRecords[i];
                    if (thisHostRecord) {
                        subZoneObj.records[thisHostRecord.name] = thisHostRecord;
                    } else {
                        // Error - all HostRecord objects should have a name
                        let badRecord = true;
                    }
                }
            });

            await thisBcMgmtHost.GetEntities_All(subZoneObj.id, "AliasRecord", (aliasRecords) => {
                for (let i = 0; i < aliasRecords.length; i++) {
                    subZoneObj.records[aliasRecords[i].name] = aliasRecords[i];
                }
            });

            if (Object.keys(subZoneObj.records).length === 0) {
                delete targetZoneObj[absoluteName];
            }

            await thisBcMgmtHost.GetAllSubZones(subZoneObj.id, targetZoneObj);
        }
    }

    async ListAllZones(parentObjectID) {
        let thisBcMgmtHost = this;
        let returnList = [];
        let subZoneObjList = await thisBcMgmtHost.GetEntities_All(parentObjectID, "Zone");

        for (let i = 0; i < subZoneObjList.length; i++) {
            let subZoneObj = subZoneObjList[i];
            let propertiesObj = APIEntity.prototype.PropertiesToObject(subZoneObj.properties);
            let absoluteName = propertiesObj['absoluteName'];
            returnList.push(absoluteName);
            returnList = returnList.concat(await thisBcMgmtHost.ListAllZones(subZoneObj.id));
        }

        return returnList;
    }

    async GetSubZones(parentObjectID) {
        let thisBcMgmtHost = this;

        let zoneObj = {};

        let subZoneObjList = await thisBcMgmtHost.GetEntities(parentObjectID, "Zone", 0, 100);

        for (let i = 0; i < subZoneObjList.length; i++) {
            let subZoneObj = subZoneObjList[i];
            let propertiesObj = APIEntity.prototype.PropertiesToObject(subZoneObj.properties);
            let absoluteName = propertiesObj['absoluteName'];
            subZoneObj.Zones = await thisBcMgmtHost.GetSubZones(subZoneObj.id);
            zoneObj[absoluteName] = subZoneObj;
        }

        return zoneObj;
    }

    async PopulateViews() {
        let thisBcMgmtHost = this;
        // Get Views
        let viewObjList = await thisBcMgmtHost.GetViews(thisBcMgmtHost.config.ConfigurationID);
        console.dir(viewObjList);
        for (let i = 0; i < viewObjList.length; i++) {
            let viewObj = viewObjList[i];
            thisBcMgmtHost.config.Views[viewObj.name] = viewObj;

            // Get Zones
            viewObj.Zones = {};
            await thisBcMgmtHost.GetAllSubZones(viewObj.id, viewObj.Zones);
        }

        // Set default view to first entry if it's not already set
        let defaultView = thisBcMgmtHost.bcMgr.defaultView;
        if (!defaultView || defaultView && !thisBcMgmtHost.config.Views[defaultView]) {
            let viewNameList = Object.keys(thisBcMgmtHost.config.Views);
            if (viewNameList.length) {
                thisBcMgmtHost.bcMgr.defaultView = viewNameList[0];
            }
        }
    }

    async GetInfoForMember(onLoadComplete) {
        let thisBcMgmtHost = this;
        let systemInfoString = await thisBcMgmtHost.GetSystemInfo();
        let parsedValues = APIEntity.prototype.PropertiesToObject(systemInfoString);
        thisBcMgmtHost.systemInfo = parsedValues;

        if (thisBcMgmtHost.systemInfo.replicationRole && thisBcMgmtHost.systemInfo.replicationRole === "PRIMARY") {
            thisBcMgmtHost.bcMgr.activeMember = thisBcMgmtHost;
            console.log(`Active host -> '${thisBcMgmtHost.systemInfo.hostName}'`);
            thisBcMgmtHost.bcMgr.config = thisBcMgmtHost.config;
            thisBcMgmtHost.config.ConfigurationID = await thisBcMgmtHost.GetFirstConfigurationID();
            await thisBcMgmtHost.PopulateViews();
            if (onLoadComplete && typeof onLoadComplete === "function") onLoadComplete();
        }
    }

    async GetFirstConfigurationID() {
        let thisBcMgmtHost = this;
        // Get Configuration ObjectID
        let response = await thisBcMgmtHost.ExecuteCommand("getEntities", {
            parentId: 0,
            type: "Configuration",
            start: 0,
            count: 100
        }, "get");
        return response[0].id;
    }

    async GetViews(configID) {
        let thisBcMgmtHost = this;
        // Get Configuration ObjectID
        let response = await thisBcMgmtHost.ExecuteCommand("getEntities", {
            parentId: configID,
            type: "View",
            start: 0,
            count: 10
        }, "get");
        return response; //[0].id;
    }

    async GetZonesByHint(viewID, hint) {
        let thisBcMgmtHost = this;
        // Get Configuration ObjectID
        let response = await thisBcMgmtHost.ExecuteCommand("getZonesByHint", {
            containerId: viewID,
            start: 0,
            count: 10,
            options: 'hint=' + hint
        }, "get");
        //console.dir(thisBcMgmtHost.restAgent);
        return response; //[0].id;
    }

    async GetHostRecordsByHint(hint) {
        let thisBcMgmtHost = this;
        // Get Configuration ObjectID
        let response = await thisBcMgmtHost.ExecuteCommand("getHostRecordsByHint", {
            start: 0,
            count: 10,
            options: 'hint=' + hint
        }, "get");
        //console.dir(thisBcMgmtHost.restAgent);
        return response; //[0].id;
    }

    async GetEntities(parentId, type, start, count) {
        let thisBcMgmtHost = this;
        // Get Configuration ObjectID
        let response = await thisBcMgmtHost.ExecuteCommand("getEntities", {
            parentId: parentId,
            type: type,
            start: start,
            count: count
        }, "get");
        return response;
    }

    async GetAliasesByHint(hint) {
        let thisBcMgmtHost = this;
        // Get Configuration ObjectID
        let response = await thisBcMgmtHost.ExecuteCommand("getAliasesByHint", {
            start: 0,
            count: 10,
            options: 'hint=' + hint
        }, "get");
        return response; //[0].id;
    }

    async GetIPRangedByIP(configID, ipAddress) {
        let thisBcMgmtHost = this;
        // Get Configuration ObjectID
        let response = await thisBcMgmtHost.ExecuteCommand("getIPRangedByIP", {
            containerId: configID,
            type: "IP4Network",
            address: ipAddress
        }, "get");
        if (response.properties) {
            let tmpProperties = APIEntity.prototype.PropertiesToObject(response.properties);
            response.properties = tmpProperties;
        }
        return response;
    }

    async GetIP4Address(configID, ipAddress) {
        let thisBcMgmtHost = this;
        // Get Configuration ObjectID
        let response = await thisBcMgmtHost.ExecuteCommand("getIP4Address", {
            containerId: configID,
            address: ipAddress
        }, "get");
        if (response.properties) {
            let tmpProperties = APIEntity.prototype.PropertiesToObject(response.properties);
            response.properties = tmpProperties;
        }
        return response;
    }

    async GetEntityByID(entityId) {
        let thisBcMgmtHost = this;
        // Get Configuration ObjectID
        let response = await thisBcMgmtHost.ExecuteCommand("getEntityById", {
            id: entityId
        }, "get");
        return response; //[0].id;
    }

    async GetEntityByName(parentId, name, type) {
        let thisBcMgmtHost = this;
        // Get Configuration ObjectID
        let response = await thisBcMgmtHost.ExecuteCommand("getEntityByName", {
            parentId: parentId,
            name: name,
            type: type
        }, "get");
        return response; //[0].id;
    }

    async SearchByObjectTypes(objType, objHint, start, count) {
        let thisBcMgmtHost = this;
        // Get Configuration ObjectID
        let response = await thisBcMgmtHost.ExecuteCommand("searchByObjectTypes", {
            keyword: objHint,
            types: objType,
            start: start,
            count: count
        }, "get");
        return response; //[0].id;
    }

    async GetNextAvailableIP4Address(parentId) {
        let thisBcMgmtHost = this;
        // Get Configuration ObjectID
        let response = await thisBcMgmtHost.ExecuteCommand("getNextAvailableIP4Address", {
            parentId: parentId
        }, "get");
        return response; //[0].id;
    }

    async AssignIP4Address(configId, ipAddress, hostName, createARecord) {
        let thisBcMgmtHost = this;
        let hostInfo = "";
        if (createARecord) {
            let defaultViewId = thisBcMgmtHost.config.Views[thisBcMgmtHost.bcMgr.defaultView].id;
            hostInfo = `${hostName},${defaultViewId},true,false`;
        }
        // Get Configuration ObjectID
        let response = await thisBcMgmtHost.ExecuteCommand("assignIP4Address", {
            configurationId: configId,
            ip4Address: ipAddress,
            macAddress: "",
            hostInfo: hostInfo,
            action: "MAKE_STATIC",
            properties: `name=${hostName}`
        }, "post", true);
        return response; //[0].id;
    }

    async UnassignIP4Address(configId, ipAddress, hostName) {
        let thisBcMgmtHost = this;
        // Get IP ObjectID
        let ipObject = await thisBcMgmtHost.GetIP4Address(configId, ipAddress);

        if (!ipObject || !ipObject.id) return `Could not find objectId for IP ${ipAddress}`;

        // Make sure hostname is bound to IP ObjectID
        let foundMatch = false;
        let deleteHostFQDNObjectId = null;
        let linkedEntities = await thisBcMgmtHost.GetLinkedEntities(ipObject.id, "HostRecord");
        for (let i = 0; i < linkedEntities.length; i++) {
            let thisEntity = linkedEntities[i];
            let propertiesObj = APIEntity.prototype.PropertiesToObject(thisEntity.properties);
            if (propertiesObj.absoluteName === hostName) {
                foundMatch = true;
                deleteHostFQDNObjectId = thisEntity.id;
                break;
            }
        }

        if (!foundMatch) return `Could not find HostRecord ${hostName} pointing to IP ${ipAddress}`;

        let response;

        response = await thisBcMgmtHost.ExecuteCommand("delete", {
            objectId: deleteHostFQDNObjectId
        }, "delete");

        // Get Configuration ObjectID
        response = await thisBcMgmtHost.ExecuteCommand("delete", {
            objectId: ipObject.id
        }, "delete");

        await thisBcMgmtHost.SelectiveDeploy([deleteHostFQDNObjectId, ipObject.id]);

        return response; //[0].id;

    }

    async AddAliasRecord(viewName, aliasFQDN, targetFQDN, ttl) {
        let thisBcMgmtHost = this;

        // Make sure viewName is specified and exists in BAM
        if (!viewName || !thisBcMgmtHost.config.Views[viewName]) return `ERROR - BlueCat view named ${viewName} does not exist`;

        // Get view ID
        let viewId = thisBcMgmtHost.config.Views[viewName].id;

        // Get Configuration ObjectID
        let response = await thisBcMgmtHost.ExecuteCommand("addAliasRecord", {
            viewId: viewId,
            absoluteName: aliasFQDN,
            linkedRecordName: targetFQDN,
            ttl: ttl || "-1",
            properties: ""
        }, "post", true);
        return response; //[0].id;
    }

    async GetLinkedEntities(entityId, type) {
        let thisBcMgmtHost = this;
        // Get Configuration ObjectID
        let response = await thisBcMgmtHost.ExecuteCommand("getLinkedEntities", {
            entityId: entityId,
            type: type,
            start: 0,
            count: 100
        }, "get");
        return response; //[0].id;
    }

    async ResizeRange(objectId, range, convertOrphanedIPAddressesTo) {
        let thisBcMgmtHost = this;
        // Update Object
        let response = await thisBcMgmtHost.ExecuteCommand("resizeRange", {
            objectId: objectId,
            range: range,
            convertOrphanedIPAddressesTo: convertOrphanedIPAddressesTo
        }, "put", true);
        return response;
    }

    async SplitIP4Network(networkId, numberOfParts, options) {
        let thisBcMgmtHost = this;
        // Update Object
        let response = await thisBcMgmtHost.ExecuteCommand("splitIP4Network", {
            networkId: networkId,
            numberOfParts: numberOfParts,
            options: options
        }, "post", true);
        return response;
    }

    async Update(bcObject) {
        let thisBcMgmtHost = this;
        // Update Object
        let response = await thisBcMgmtHost.ExecuteCommand("update", bcObject, "put");
        return response;
    }

    async SelectiveDeploy(objectIdList) {
        let thisBcMgmtHost = this;
        // Get Configuration ObjectID
        let response = await thisBcMgmtHost.ExecuteCommand("selectiveDeploy", objectIdList, "post");
        return response;
    }

    async GetEntities_All(parentID, type, cb) {
        let thisBcMgmtHost = this;
        let batchSize = 300;
        let returnSize = 300;
        let totalReceived = 0;
        let returnObjectArray = [];
        while (batchSize === returnSize) {
            let results = await thisBcMgmtHost.GetEntities(parentID, type, totalReceived, batchSize);
            returnSize = results.length;
            totalReceived += returnSize;
            if (cb) {
                cb(results);
            } else {
                returnObjectArray.push.apply(returnObjectArray, results);
            }
        }
        return returnObjectArray;
    }

    /** @returns {APIEntity} System Info */
    async GetSystemInfo() {
        let thisBcMgmtHost = this;
        let response = await thisBcMgmtHost.ExecuteCommand("getSystemInfo", {}, "get");
        return response;
    }

    async ExecuteCommand(command, parameters, verb, paramsInQuery) {
        let thisBcMgmtHost = this;
        let returnObj = null;

        try {
            // Is the token null or over X seconds old?
            if (!thisBcMgmtHost.__authToken || (Date.now() - thisBcMgmtHost.tokenAcquiredTimestamp) / 1000 > thisBcMgmtHost.maxTokenAgeSec) {
                let requestString = `login?&username=${thisBcMgmtHost.__apiUser}&password=${thisBcMgmtHost.__apiPassword}`;
                let responseString = await thisBcMgmtHost.__restAgent.get(requestString);
                /-> ([^-]+) <-/.test(responseString.data);
                if (RegExp.$1) {
                    thisBcMgmtHost.__authToken = RegExp.$1;
                    thisBcMgmtHost.tokenAcquiredTimestamp = Date.now();
                    thisBcMgmtHost.__restAgent.defaults.headers.common['Authorization'] = thisBcMgmtHost.__authToken;
                } else {
                    return "Could not authenticate";
                }
            }

            let response = null;
            switch (verb) {
                case "get":
                    response = await thisBcMgmtHost.__restAgent[verb](command, { params: parameters });
                    returnObj = response.data;
                    break;
                case "put":
                    if (paramsInQuery) {
                        response = await thisBcMgmtHost.__restAgent.put(command, null, { params: parameters });
                    } else {
                        response = await thisBcMgmtHost.__restAgent.put(command, parameters, { headers: { "Content-Type": "application/json" } });
                    }
                    returnObj = response;
                    break;
                case "post":
                    if (paramsInQuery) {
                        response = await thisBcMgmtHost.__restAgent.post(command, null, { params: parameters });
                    } else {
                        response = await thisBcMgmtHost.__restAgent.post(command, parameters, { headers: { "Content-Type": "application/json" } });
                    }
                    returnObj = response.data;
                    break;
                case "delete":
                    response = await thisBcMgmtHost.__restAgent.delete(command, { params: parameters });
                    returnObj = response.data;
                    break;
                default:
                    let bob = 1;
            }
        } catch (ex) {
            returnObj = ex;
        }

        return returnObj;
    }

    // Get FQDN in View
    async GetFQDNRecord(viewName, fqdn) {
        let thisBcMgmtHost = this;

        let returnObj = null;

        if (!thisBcMgmtHost.config.Views[viewName]) return null;

        let viewObj = thisBcMgmtHost.config.Views[viewName];

        // Loop over FQDN splits
        let domainArray = fqdn.split(".");
        let hostArray = [];

        for (let i = 0; i < domainArray.length; i++) {
            let domainName = domainArray.join(".");
            let hostName = hostArray.join(".");

            if (viewObj.Zones[domainName] && viewObj.Zones[domainName].records[hostName]) {
                returnObj = viewObj.Zones[domainName].records[hostName];
                break;
            }

            hostArray.push(domainArray.shift());
        }

        return returnObj;
    }
}

class BlueCatManager extends DRP_Service {
    /**
    *
    * @param {string} serviceName Service Name
    * @param {drpNode} drpNode DRP Node
    * @param {number} priority Priority (lower better)
    * @param {number} weight Weight (higher better)
    * @param {string} scope Scope [local|zone|global(defaut)]
    * @param {string[]} bcHosts BlueCat Management Hosts
    * @param {string} bcUser BlueCat User
    * @param {string} bcPass BlueCat Password
    * @param {string} defaultView Default view name
    * @param {function} onLoadComplete Execute on load complete
    */
    constructor(serviceName, drpNode, priority, weight, scope, bcHosts, bcUser, bcPass, defaultView, onLoadComplete) {
        super(serviceName, drpNode, "BlueCatManager", null, false, priority, weight, drpNode.Zone, scope, null, null, 1);
        let thisBcMgr = this;

        /** @type {Object.<string, BlueCatMgmtHost>} */
        this.members = {};

        /** @type {BlueCatMgmtHost} */
        this.activeMember = null;

        this.defaultView = defaultView || null;

        this.config = null;

        this.deployObjectIds = [];
        this.deployTimer = null;

        this.lastHAquery = {};
        for (let i = 0; i < bcHosts.length; i++) {
            let bcMgmtHostIP = bcHosts[i];
            let bcMgmtHostObj = new BlueCatMgmtHost({
                name: bcMgmtHostIP,
                mgmtIP: bcMgmtHostIP,
                apiUser: bcUser,
                apiPassword: bcPass,
                bcMgr: thisBcMgr
            });
            this.members[bcMgmtHostIP] = bcMgmtHostObj;
            bcMgmtHostObj.GetInfoForMember(onLoadComplete);
        }

        this.ClientCmds = {
            "refreshConfigs": async function () {
                return thisBcMgr.RefreshConfigs();
            },
            "getEntityByName": async (cmdObj) => {
                let methodParams = ['parentId', 'name', 'type'];
                let params = thisBcMgr.GetParams(cmdObj, methodParams);
                let returnObj = null;

                if (params.parentId && params.name && params.type) returnObj = await thisBcMgr.activeMember.GetEntityByName(params.parentId, params.name, params.type)
                else returnObj = { err: `Required params: ${methodParams}`}
                return returnObj;
            },
            "getEntities": async (cmdObj) => {
                let methodParams = ['parentId', 'type', 'start', 'count'];
                let params = thisBcMgr.GetParams(cmdObj, methodParams);
                let returnObj = null;

                if (params.parentId && params.type && params.count) returnObj = await thisBcMgr.activeMember.GetEntities(params.parentId, params.type, params.start, params.count)
                else returnObj = { err: `Required params: ${methodParams}` }
                return returnObj;
            },
            "resizeRange": async (cmdObj) => {
                let methodParams = ['objectId', 'range', 'convertOrphanedIPAddressesTo'];
                let params = thisBcMgr.GetParams(cmdObj, methodParams);
                let returnObj = {
                    err: null,
                    msg: null
                };
                if (params.objectId && params.range && params.convertOrphanedIPAddressesTo) {
                    let response = await thisBcMgr.activeMember.ResizeRange(params.objectId, params.range, params.convertOrphanedIPAddressesTo);
                    returnObj.msg = response.status;
                }
                else returnObj.err = `Required params: ${methodParams}`;
                return returnObj;
            },
            "splitIP4Network": async (cmdObj) => {
                let methodParams = ['networkId', 'numberOfParts', 'options'];
                let params = thisBcMgr.GetParams(cmdObj, methodParams);
                let returnObj = null;

                if (params.networkId && params.numberOfParts && params.options) returnObj = await thisBcMgr.activeMember.SplitIP4Network(params.networkId, params.numberOfParts, params.options)
                else returnObj = { err: `Required params: ${methodParams}` }
                return returnObj;
            },
            "getIP4Address": async (cmdObj) => {
                let ipAddress = null;
                let returnObj = null;
                if (cmdObj.pathList && cmdObj.pathList.length >= 1) {
                    ipAddress = cmdObj.pathList[0];
                }
                if (cmdObj.ipAddress) {
                    ipAddress = cmdObj.ipAddress;
                }
                if (ipAddress) returnObj = await thisBcMgr.activeMember.GetIP4Address(thisBcMgr.activeMember.config.ConfigurationID, ipAddress);
                return returnObj;
            },
            "getIPRangedByIP": async function (cmdObj) {
                let methodParams = ['ipAddress'];
                let params = thisBcMgr.GetParams(cmdObj, methodParams);
                let returnObj = null;

                if (params.ipAddress) returnObj = thisBcMgr.activeMember.GetIPRangedByIP(thisBcMgr.activeMember.config.ConfigurationID, params.ipAddress)
                else returnObj = { err: `Required params: ${methodParams}` };
                return returnObj;
            },
            "getNextAvailableIP": async function (cmdObj) {
                let ipAddress = null;
                let returnObj = null;
                if (cmdObj.pathList && cmdObj.pathList.length >= 1) {
                    ipAddress = cmdObj.pathList[0];
                }
                if (cmdObj.ipAddress) {
                    ipAddress = cmdObj.ipAddress;
                }
                let networkObj = await thisBcMgr.activeMember.GetIPRangedByIP(thisBcMgr.activeMember.config.ConfigurationID, ipAddress);
                if (networkObj && networkObj.id) {
                    returnObj = await thisBcMgr.activeMember.GetNextAvailableIP4Address(networkObj.id);
                }
                return returnObj;
            },
            "assignIP4Address": async function (cmdObj) {
                let ipAddress = null;
                let hostName = "";
                let createARecord = false;
                let returnObj = null;
                if (cmdObj.pathList && cmdObj.pathList.length >= 1) {
                    ipAddress = cmdObj.pathList[0];
                    hostName = cmdObj.pathList[1];
                    if (cmdObj.pathList[2]) createARecord = true;
                }
                if (cmdObj.ipAddress) {
                    ipAddress = cmdObj.ipAddress;
                    if (cmdObj.hostName) hostName = cmdObj.hostName;
                    if (cmdObj.createARecord) createARecord = true;
                }
                if (ipAddress) returnObj = await thisBcMgr.activeMember.AssignIP4Address(thisBcMgr.activeMember.config.ConfigurationID, ipAddress, hostName, createARecord);
                return returnObj;
            },
            "unassignIP4Address": async function (cmdObj) {
                let ipAddress = null;
                let hostName = "";
                let returnObj = null;
                if (cmdObj.pathList && cmdObj.pathList.length >= 1) {
                    ipAddress = cmdObj.pathList[0];
                    hostName = cmdObj.pathList[1];
                }
                if (cmdObj.ipAddress) {
                    ipAddress = cmdObj.ipAddress;
                    if (cmdObj.hostName) hostName = cmdObj.hostName;
                }
                if (ipAddress) returnObj = await thisBcMgr.activeMember.UnassignIP4Address(thisBcMgr.activeMember.config.ConfigurationID, ipAddress, hostName);
                return returnObj;
            },
            "addAliasRecord": async function (cmdObj) {
                let viewName = null;
                let aliasFQDN = null;
                let targetFQDN = null;
                let ttl = null;

                let returnObj = null;
                if (cmdObj.pathList && cmdObj.pathList.length >= 1) {
                    viewName = cmdObj.pathList[0];
                    aliasFQDN = cmdObj.pathList[1];
                    targetFQDN = cmdObj.pathList[2];
                    ttl = cmdObj.pathList[3];
                }
                if (cmdObj.viewName) viewName = cmdObj.viewName;
                if (cmdObj.aliasFQDN) aliasFQDN = cmdObj.aliasFQDN;
                if (cmdObj.targetFQDN) targetFQDN = cmdObj.targetFQDN;
                if (cmdObj.ttl) ttl = cmdObj.ttl;

                if (viewName && aliasFQDN && targetFQDN) {
                    returnObj = await thisBcMgr.activeMember.AddAliasRecord(viewName, aliasFQDN, targetFQDN, ttl);
                } else {
                    returnObj = "ERROR - expecting viewName/aliasFQDN/targetFQDN/{ttl}";
                }
                return returnObj;
            },
            "searchByObjectTypes": async function () {
                let batchSize = 100;
                let returnSize = 100;
                let totalIPAddresses = 0;
                let returnNetworkArray = [];
                thisBcMgr.DRPNode.log(`Querying network records`);
                while (batchSize === returnSize) {
                    let results = await thisBcMgr.activeMember.SearchByObjectTypes("IP4Network", "*", returnNetworkArray.length, batchSize);
                    returnSize = results.length;
                    returnNetworkArray.push.apply(returnNetworkArray, results);
                }
                thisBcMgr.DRPNode.log(`Found ${returnNetworkArray.length} network records`);
                for (let i = 0; i < returnNetworkArray.length; i++) {
                    let networkObj = returnNetworkArray[i];
                    let networkID = networkObj['id'];
                    returnSize = 100;
                    let returnIPArray = [];
                    while (batchSize === returnSize) {
                        let results = await thisBcMgr.activeMember.GetEntities(networkID, "IP4Address", returnIPArray.length, batchSize);
                        returnSize = results.length;
                        totalIPAddresses = totalIPAddresses + results.length;
                        returnIPArray.push.apply(returnIPArray, results);
                    }
                }
                thisBcMgr.DRPNode.log(`Found ${totalIPAddresses} ip records`);
                return returnNetworkArray;
            },
            "getFQDNRecord": async function (cmdObj) {
                let viewName = null;
                let fqdn = null;
                let returnObj = null;
                if (cmdObj.pathList && cmdObj.pathList.length >= 2) {
                    viewName = cmdObj.pathList[0];
                    fqdn = cmdObj.pathList[1];
                }
                if (cmdObj.viewName && cmdObj.fqdn) {
                    viewName = cmdObj.viewName;
                    fqdn = cmdObj.fqdn;
                }
                if (viewName && fqdn) returnObj = await thisBcMgr.activeMember.GetFQDNRecord(viewName, fqdn);
                else returnObj = "Must provide viewName and fqdn";
                return returnObj;
            },
            "getLinkedEntities": async function (cmdObj) {
                let entityId = null;
                let type = null;
                let returnObj = null;
                if (cmdObj.pathList && cmdObj.pathList.length >= 2) {
                    entityId = cmdObj.pathList[0];
                    type = cmdObj.pathList[1];
                }
                if (cmdObj.entityId && cmdObj.type) {
                    entityId = cmdObj.entityId;
                    type = cmdObj.type;
                }
                if (entityId && type) returnObj = await thisBcMgr.activeMember.GetLinkedEntities(entityId, type);
                return returnObj;
            },
            "updateObject": async function (cmdObj) {
                let returnObj = {
                    err: null,
                    msg: null
                };

                let methodParams = ['updateObj'];
                let params = thisBcMgr.GetParams(cmdObj, methodParams);

                if (!params.updateObj) {
                    returnObj.err = "param updateObj not provided";
                }
                if (!params.updateObj.id) {
                    returnObj.err = "param updateObj.id attribute not provided";
                } else {
                    // Let's try to update the object
                    let response = await thisBcMgr.activeMember.Update(params.updateObj);
                    returnObj.msg = response.status;
                    /*
                    thisBcMgr.DRPNode.TopicManager.SendToTopic("BlueCat", `Updating Object: ${JSON.stringify(params.updateObj)}`);
                    if (response && response.status && response.status === 200) {
                        thisBcMgr.DeployObjectId(params.updateObj.id);
                    } else {
                        thisBcMgr.DRPNode.TopicManager.SendToTopic("BlueCat", `Update of Object: ${JSON.stringify(params.updateObj)} failed, response status <${response.status}>`);
                    }
                    */
                }
                return returnObj;
            },
            "getEntityByID": async (cmdObj) => {
                let entityId = null;
                let returnObj = null;
                if (cmdObj.pathList && cmdObj.pathList.length >= 1) {
                    entityId = cmdObj.pathList[0];
                }
                if (cmdObj.entityId) {
                    entityId = cmdObj.entityId;
                }
                if (entityId) returnObj = await thisBcMgr.activeMember.GetEntityByID(entityId);
                return returnObj;
            },
            "deployEntityId": async (cmdObj) => {
                let entityId = null;
                let returnObj = null;
                if (cmdObj.pathList && cmdObj.pathList.length >= 1) {
                    entityId = cmdObj.pathList[0];
                }
                if (cmdObj.entityId) {
                    entityId = cmdObj.entityId;
                }
                if (entityId) {
                    thisBcMgr.DeployObjectId(entityId); //await thisBcMgr.activeMember.SelectiveDeploy([entityId]);
                    returnObj = "Queued for deployment"
                }
                return returnObj;
            }
        };
    }

    DeployObjectId(objectId) {
        let thisBcMgr = this;

        // If a timer is running, reset it
        if (thisBcMgr.deployTimer) {
            clearTimeout(thisBcMgr.deployTimer);
            thisBcMgr.deployTimer = null;
        }

        // Start timer
        thisBcMgr.deployTimer = setTimeout(
            async function () {
                let deployList = thisBcMgr.deployObjectIds;
                thisBcMgr.deployObjectIds = [];
                thisBcMgr.deployTimer = null;
                await thisBcMgr.activeMember.SelectiveDeploy(deployList);
                thisBcMgr.DRPNode.log(`Deployed ${deployList.join(",")}`);
            }, 5000);

        // Add objectId to the deployment list
        thisBcMgr.deployObjectIds.push(objectId);
    }
}

module.exports = BlueCatManager;