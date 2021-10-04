'use strict';

const os = require('os');
const util = require('util');
const tcpp = require('tcp-ping');
const tcpPing = util.promisify(tcpp.ping);
const dns = require('dns').promises;
const express = require('express');
const Express_Router = express.Router;
const DRP_Endpoint = require("./endpoint");
const DRP_Client = require("./client");
const DRP_Service = require("./service");
const DRP_TopicManager = require("./topicmanager");
const DRP_RouteHandler = require("./routehandler");
const { DRP_WebServer, DRP_WebServerConfig } = require("./webserver");
const { DRP_SubscribableSource, DRP_Subscriber } = require('./subscription');
const { DRP_AuthRequest, DRP_AuthResponse, DRP_AuthFunction } = require('./auth');
const { DRP_Packet, DRP_Cmd, DRP_Reply, DRP_Stream, DRP_RouteOptions } = require('./packet');
const { DRP_VirtualDirectory, DRP_Permission, DRP_PermissionSet, DRP_Securable } = require('./securable');
const Express_Request = express.request;
const Express_Response = express.response;
const swaggerUI = require("swagger-ui-express");

class DRP_PathCmd {
    /**
     * @param {string} method Method to execute
     * @param {string} pathList List of path elements
     * @param {object} params Arguments or payload
     */
    constructor(method, pathList, params) {
        this.method = method;
        this.pathList = pathlist;
        this.params = params;
    }
}

class DRP_NodeDeclaration {
    /**
     * 
     * @param {string} nodeID Node ID
     * @param {string[]} nodeRoles Functional Roles ['Registry','Broker','Provider','Logger']
     * @param {string} hostID Host Identifier
     * @param {string} nodeURL Listening URL (optional)
     * @param {string} domainName Domain Name
     * @param {string} meshKey Domain Key
     * @param {string} zoneName Zone Name
     * @param {string} scope Scope
     */
    constructor(nodeID, nodeRoles, hostID, nodeURL, domainName, meshKey, zoneName, scope) {
        this.NodeID = nodeID;
        this.NodeRoles = nodeRoles;
        this.HostID = hostID;
        this.NodeURL = nodeURL;
        this.DomainName = domainName;
        this.MeshKey = meshKey;
        this.Zone = zoneName;
        this.Scope = scope;
    }
}

class DRP_Node extends DRP_Securable {
    /**
     * 
     * @param {string[]} nodeRoles List of Roles: Broker, Provider, Registry 
     * @param {string} hostID Host Identifier
     * @param {string} domainName DRP Domain Name
     * @param {string} meshKey DRP Mesh Key
     * @param {string} zone DRP Zone Name (optional)
     * @param {DRP_WebServerConfig} webServerConfig Web server config (optional)
     * @param {string} drpRoute DRP WS Route (optional)
     * @param {DRP_PermissionSet} permissionSet DRP Permission Set
     */
    constructor(nodeRoles, hostID, domainName, meshKey, zone, webServerConfig, drpRoute, permissionSet) {
        super(permissionSet);
        let thisNode = this;
        this.NodeID = `${os.hostname()}-${process.pid}`;
        this.HostID = hostID;
        /** @type {DRP_WebServer} */
        this.WebServer = null;
        this.WebServerConfig = webServerConfig || null;
        /** @type {Object<string,Express_Router}} */
        this.SwaggerRouters = {};
        this.drpRoute = drpRoute || "/";
        this.DomainName = domainName;
        this.__MeshKey = meshKey;
        if (!this.__MeshKey) this.Die("No MeshKey provided");
        this.Zone = zone || "default";
        /** @type{string} */
        this.RegistryUrl = null;
        this.Debug = false;
        this.TestMode = false;
        /** @type{string} */
        this.AuthenticationServiceName = null;
        this.HasConnectedToMesh = false;
        /** @type{function} */
        this.onControlPlaneConnect = null;
        this.ListeningName = null;

        // If we have a web server config, start listening
        if (this.WebServerConfig && this.WebServerConfig.ListeningURL) {
            this.WebServer = new DRP_WebServer(webServerConfig);
            this.WebServer.start();

            this.ListeningName = this.WebServer.config.ListeningURL;
        }

        /** @type{string[]} */
        this.NodeRoles = nodeRoles || [];
        /** @type{string} */
        this.WebProxyURL = null;

        // By default, Registry nodes are "connected" to the Control Plane and non-Registry nodes aren't
        this.ConnectedToControlPlane = thisNode.IsRegistry();

        // Wait time for Registry reconnect attempts
        this.ReconnectWaitTimeSeconds = 0;

        /** @type {Object.<string,DRP_NodeClient>} */
        this.NodeEndpoints = {};

        /** @type {Object.<string,DRP_NodeClient>} */
        this.ConsumerEndpoints = {};

        // Create topic manager
        this.TopicManager = new DRP_TopicManager(thisNode);
        this.TopicManager.CreateTopic("TopologyTracker", 100);

        // Create subscription manager
        this.SubscriptionManager = new DRP_SubscriptionManager(thisNode);

        // Create topology tracker
        this.TopologyTracker = new DRP_TopologyTracker(thisNode);

        let newNodeEntry = new DRP_NodeTableEntry(thisNode.NodeID, null, nodeRoles, this.ListeningName, "global", this.Zone, this.NodeID, this.HostID);
        let addNodePacket = new DRP_TopologyPacket(newNodeEntry.NodeID, "add", "node", newNodeEntry.NodeID, newNodeEntry.Scope, newNodeEntry.Zone, newNodeEntry);
        thisNode.TopologyTracker.ProcessPacket(addNodePacket, this.NodeID);

        /** @type Object.<string,DRP_Service> */
        this.Services = {};

        /** @type Object.<string,DRP_AuthResponse> */
        this.ConsumerTokens = {};

        // Add a route handler even if we don't have an Express server (needed for stream relays)
        this.RouteHandler = new DRP_RouteHandler(this, this.drpRoute);

        this.PacketRelayCount = 0;

        this.TCPPing = this.TCPPing;
        this.GetServiceDefinitions = this.GetServiceDefinitions;
        this.ListClassInstances = this.ListClassInstances;

        this.Mesh = async (params, callingEndpoint) => {
            let pathData = await thisNode.EvalPath(thisNode.GetBaseObj().Mesh, params, callingEndpoint);
            return pathData;
        }

        let localDRPEndpoint = new DRP_Endpoint(null, this, "Local");
        this.ApplyNodeEndpointMethods(localDRPEndpoint);

        let DRPService = new DRP_Service("DRP", this, "DRP", null, false, 10, 10, this.Zone, "local", null, ["TopologyTracker"], 1);
        DRPService.ClientCmds = localDRPEndpoint.EndpointCmds;

        this.AddService(DRPService);
    }

    get Debug() { return this.__debug }
    set Debug(val) { this.__debug = this.IsTrue(val) }

    get TestMode() { return this.__testMode }
    set TestMode(val) { this.__testMode = this.IsTrue(val) }

    /**
     * Print message to stdout
     * @param {string} message Message to output
     * @param {boolean} isDebugMsg Is it a debug message?
     */
    log(message, isDebugMsg) {
        // If it's a debug message and we don't have debugging turned on, return
        if (!this.Debug && isDebugMsg) return;

        let paddedNodeID = this.NodeID.padEnd(14, ' ');
        console.log(`${this.getTimestamp()} [${paddedNodeID}] -> ${message}`);
    }
    getTimestamp() {
        let date = new Date();
        let hour = date.getHours();
        hour = (hour < 10 ? "0" : "") + hour;
        let min = date.getMinutes();
        min = (min < 10 ? "0" : "") + min;
        let sec = date.getSeconds();
        sec = (sec < 10 ? "0" : "") + sec;
        let year = date.getFullYear();
        let month = date.getMonth() + 1;
        month = (month < 10 ? "0" : "") + month;
        let day = date.getDate();
        day = (day < 10 ? "0" : "") + day;
        return year + "" + month + "" + day + "" + hour + "" + min + "" + sec;
    }

    __GetNodeDeclaration() {
        let thisNode = this;
        return new DRP_NodeDeclaration(thisNode.NodeID, thisNode.NodeRoles, thisNode.HostID, thisNode.ListeningName, thisNode.DomainName, thisNode.__MeshKey, thisNode.Zone);
    }

    async GetConsumerToken(username, password) {
        let thisNode = this;
        let returnToken = null;
        /** @type {DRP_AuthResponse} */
        let authResults = await thisNode.Authenticate(username, password, null);
        if (authResults) {
            thisNode.ConsumerTokens[authResults.Token] = authResults;

            // If this Node is a Broker, relay to other Brokers in Zone
            if (thisNode.IsBroker()) {
                let nodeIDList = Object.keys(thisNode.TopologyTracker.NodeTable);
                for (let i = 0; i < nodeIDList.length; i++) {
                    let checkNode = thisNode.TopologyTracker.NodeTable[nodeIDList[i]];
                    if (checkNode.NodeID !== thisNode.NodeID && checkNode.IsBroker() && checkNode.Zone === thisNode.Zone) {
                        // Send command to remote Broker
                        thisNode.ServiceCmd("DRP", "addConsumerToken", { tokenPacket: authResults }, checkNode.NodeID, null, false, false, null);
                    }
                }
            }
            returnToken = authResults.Token;
        }
        return returnToken;
    }

    GetLastTokenForUser(userName) {
        let thisNode = this;
        let tokenList = Object.keys(thisNode.ConsumerTokens).reverse();
        for (let i = 0; i < tokenList.length; i++) {
            let checkTokenID = tokenList[i];
            let thisToken = thisNode.ConsumerTokens[tokenList[i]];
            if (userName === thisToken.UserName) return checkTokenID;
        }
        return null;
    }

    /**
     * 
     * @param {string} restRoute Route to listen for Node REST requests
     * @param {string} basePath Base path list
     * @param {boolean} writeToLogger If true, output REST Logs to Logger
     * @returns {number} Failure code
     */
    EnableREST(restRoute, basePath, writeToLogger) {
        let thisNode = this;

        if (!thisNode.WebServer || !thisNode.WebServer.expressApp) return 1;

        // Set Consumer Token Cleanup Interval - 60 seconds
        let checkFrequencyMs = 60000;
        setInterval(() => {
            let iCurrentTimestamp = parseInt(thisNode.getTimestamp());
            let consumerTokenIDList = Object.keys(thisNode.ConsumerTokens);

            // Collect tokens from ConsumerEndpoints
            let connectedTokenList = [];
            let consumerEndpointIDList = Object.keys(thisNode.ConsumerEndpoints);
            for (let i = 0; i < consumerEndpointIDList.length; i++) {
                let consumerEndpointID = consumerEndpointIDList[i];
                connectedTokenList.push(thisNode.ConsumerEndpoints[consumerEndpointID].AuthInfo.value);
            }

            // TO DO - if clients automatically time out, we need to add a keepalive so that
            // tokens of consumers connected to this Broker are not removed from other Brokers
            for (let i = 0; i < consumerTokenIDList.length; i++) {
                let checkToken = consumerTokenIDList[i];
                let checkTokenObj = thisNode.ConsumerTokens[checkToken];
                let iCheckTimestamp = parseInt(checkTokenObj.AuthTimestamp);

                // Skip if currently connected
                if (connectedTokenList.includes(checkToken)) continue;

                // Expire after 30 minutes
                let maxAgeSeconds = 60 * 30;
                if (iCurrentTimestamp > iCheckTimestamp + maxAgeSeconds) {

                    // The token has expired
                    delete thisNode.ConsumerTokens[checkToken];
                }
            }
        }, checkFrequencyMs);

        let tmpBasePath = basePath || "";
        let basePathArray = tmpBasePath.replace(/^\/|\/$/g, '').split('/');

        /**
         * 
         * @param {Express_Request} req Request
         * @param {Express_Response} res Response
         * @param {function} next Next step
         */
        let nodeRestHandler = async function (req, res, next) {
            // Get Auth Key
            let authInfo = {
                type: null,
                value: null
            };

            // Check for authInfo:
            //   x-api-key - apps (static)
            //   x-api-token - users (dynamic)
            if (req.headers['x-api-key'] || (req.headers.cookie && /^x-api-key=.*$/.test(req.headers.cookie))) {
                authInfo.type = 'key';
                authInfo.value = req.headers['x-api-key'] || req.headers.cookie.match(/^x-api-key=(.*)$/)[1];
            } else if (req.headers['x-api-token'] || (req.headers.cookie && /^x-api-token=.*$/.test(req.headers.cookie))) {
                authInfo.type = 'token';
                authInfo.value = req.headers['x-api-token'] || req.headers.cookie.match(/^x-api-token=(.*)$/)[1];

                // Make sure the token is current
                if (!thisNode.ConsumerTokens[authInfo.value]) {
                    // We don't recognize this token
                    res.status(401).send("Invalid token");
                    return;
                }
                authInfo.userInfo = thisNode.ConsumerTokens[authInfo.value];
            } else {
                // Unauthorized
                res.status(401).send("No x-api-token or x-api-key provided");
                return;
            }

            // Turn path into list, remove first element
            let decodedPath = decodeURIComponent(req.path);
            let remainingPath = decodedPath.replace(/^\/|\/$/g, '').split('/');
            remainingPath.shift();

            let listOnly = false;
            let format = null;
            let resCode = 404;

            if (req.query.listOnly) listOnly = true;
            if (req.query.format) format = 1;

            // Treat as "getPath"
            let resultString = "";

            try {
                let resultObj = await thisNode.GetObjFromPath({ "method": "cliGetPath", "pathList": basePathArray.concat(remainingPath), "listOnly": listOnly, "AuthInfo": authInfo, "body": req.body }, thisNode.GetBaseObj());
                if (!resultObj || !resultObj.pathItem && !resultObj.pathItemList || resultObj.pathItemList && !resultObj.pathItemList.length) {
                    // No results
                } else {
                    resCode = 200;
                    if (resultObj.pathItem) {
                        resultString = JSON.stringify(resultObj.pathItem, null, format);
                    } else if (resultObj.pathItemList) {
                        resultString = JSON.stringify(resultObj.pathItemList, null, format);
                    } else {
                        resultString = JSON.stringify(resultObj, null, format);
                    }
                }
            } catch (e) {
                resCode = 500;
                resultString = `Failed to stringify response: ${e}`;
            }
            res.status(resCode).send(resultString);
            let logMessage = {
                req: {
                    hostname: req.hostname,
                    ip: req.ip,
                    method: req.method,
                    protocol: req.protocol,
                    path: req.path,
                    headers: Object.assign({}, req.headers),
                    query: req.query,
                    baseUrl: req.baseUrl,
                    body: req.body
                },
                res: {
                    code: resCode,
                    length: resultString.length
                }
            };
            delete logMessage.req.headers["authorization"];
            thisNode.TopicManager.SendToTopic("RESTLogs", logMessage);
            if (writeToLogger) {
                thisNode.ServiceCmd("Logger", "writeLog", { serviceName: "REST", logData: logMessage });
            }
        };

        thisNode.WebServer.expressApp.all(`${restRoute}`, nodeRestHandler);
        thisNode.WebServer.expressApp.all(`${restRoute}/*`, nodeRestHandler);

        thisNode.WebServer.expressApp.use('/api-doc/:serviceName?', (req, res) => {
            // What service are we trying to reach?
            let targetServiceName = req.params['serviceName'];

            if (!targetServiceName) {
                let apiNames = Object.keys(thisNode.SwaggerRouters);
                let linkList = [];
                for (let i = 0; i < apiNames.length; i++) {
                    linkList.push(`<a href="${req.baseUrl}/${apiNames[i]}">${apiNames[i]}</a>`);
                }
                let linkListHtml = linkList.join("<br><br>");
                res.send(`<h3>Service List</h3>${linkListHtml}`);
                return;
            }

            // If we have it, execute
            if (thisNode.SwaggerRouters[targetServiceName]) {
                thisNode.SwaggerRouters[targetServiceName].handle(req, res, null);
            } else {
                res.status(404).send(`API doc not found for service ${targetServiceName}`);
            }
        });

        /**
         * 
         * @param {DRP_TopologyPacket} topologyPacket Topology Packet
         */
        let WatchTopologyForServices = async (topologyPacket) => {
            let thisNode = this;

            // Is this a service add?
            if (topologyPacket.cmd === "add" && topologyPacket.type === "service") {

                // Get topologyData
                /** @type {DRP_ServiceTableEntry} */
                let serviceEntry = topologyPacket.data;

                // If we already have it, ignore
                if (thisNode.SwaggerRouters[serviceEntry.Name]) return;

                // Reach out to the remote node, see if it has a Swagger doc for this service
                await thisNode.AddSwaggerRouter(serviceEntry.Name, serviceEntry.NodeID);
            }

            // Is this a service delete?
            if (topologyPacket.cmd === "delete" && topologyPacket.type === "node") {

                // TODO - Figure out a cleaner way to do this.  The TopologyManager only passes Node deletes to topic, not service deletes
                // Because of this we need to check each service after a Node removal
                let serviceNameList = Object.keys(thisNode.SwaggerRouters);
                for (let i = 0; i < serviceNameList.length; i++) {
                    let serviceName = serviceNameList[i];
                    let serviceInstance = thisNode.TopologyTracker.FindInstanceOfService(serviceName);
                    if (!serviceInstance) delete thisNode.SwaggerRouters[serviceName];
                }
            }

            if (topologyPacket.cmd === "delete" && topologyPacket.type === "service") {
                // Individual service removal; uncommon

                // Get topologyData
                /** @type {DRP_ServiceTableEntry} */
                let serviceEntry = topologyPacket.data;

                let serviceInstance = thisNode.TopologyTracker.FindInstanceOfService(serviceEntry.Name);
                if (!serviceInstance) delete thisNode.SwaggerRouters[serviceEntry.Name];
            }
        };

        // Watch Topology for new Service
        thisNode.TopicManager.SubscribeToTopic(new DRP_Subscriber("TopologyTracker", null, null, null, (topologyPacket) => {
            WatchTopologyForServices(topologyPacket.Message);
        }, null));

        // Get Token
        thisNode.WebServer.expressApp.post('/token', async (req, res) => {
            // Get an auth token
            let username = req.body.username;
            let password = req.body.password;

            let userToken = await GetConsumerToken(username, password);

            if (userToken) {
                res.send(`x-api-token: ${userToken}`);
            } else {
                res.status(401).send("Bad credentials");
            }
            return;
        });

        return 0;
    }

    async AddSwaggerRouter(serviceName, targetNodeID) {
        let thisNode = this;
        // Reach out to the remote node
        let swaggerObj = await thisNode.ServiceCmd(serviceName, "getOpenAPIDoc", {}, targetNodeID, null, true, true, null);

        // Does it exist?
        if (swaggerObj && swaggerObj.openapi) {

            // Override server settings
            swaggerObj.servers = [
                {
                    "url": `/Mesh/Services/${serviceName}`
                }
            ],

                // Override security settings
                swaggerObj.components = {
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
                };
            swaggerObj.security = [
                { "x-api-key": [] },
                { "x-api-token": [] }
            ];

            let thisRouter = express.Router();
            thisRouter.use("/", swaggerUI.serve);
            thisRouter.get("/", swaggerUI.setup(swaggerObj));
            thisNode.SwaggerRouters[serviceName] = thisRouter;
        }
    }

    /**
     * Return a dictionary of class names, services and providers
     * @param {{className: string}} params Parameters
     * @param {object} callingEndpoint Calling Endpoint
     * @param {token} token Command token
     * @returns {Object.<string,Object.<string,{providers:string[]}>>} Class instances
     */
    async ListClassInstances(params, callingEndpoint, token) {
        let thisNode = this;

        let results = {};
        let findClassName = null;
        if (params && params.className) findClassName = params.className;
        else if (params && params.pathList && params.pathList.length > 0) findClassName = params.pathList.shift();

        // Get best instance for each service
        let serviceDefinitions = await thisNode.GetServiceDefinitions(params, callingEndpoint);

        // Get best instance of each service
        let serviceList = Object.keys(serviceDefinitions);
        for (let i = 0; i < serviceList.length; i++) {
            let serviceName = serviceList[i];
            let serviceInstanceObj = serviceDefinitions[serviceName];
            let classList = Object.keys(serviceInstanceObj.Classes);
            for (let k = 0; k < classList.length; k++) {
                let className = classList[k];
                if (!findClassName || findClassName === className) {
                    if (!results[className]) {
                        results[className] = {};
                    }
                    if (!results[className][serviceName]) {
                        results[className][serviceName] = { providers: [] };
                    }
                    results[className][serviceName].providers.push(serviceInstanceObj.NodeID);
                }
            }
        }

        return results;
    }

    GetServiceDefinition(params) {
        /*
         * We need to return:
         * {
         *    "TestService": {ClientCmds: {}, Classes:{}, Streams:{}}
         * }
         */
        let thisNode = this;
        let targetService = thisNode.Services[params.serviceName];
        let serviceDefinition = targetService.GetDefinition();
        return serviceDefinition;
    }

    async GetServiceDefinitions(params, callingEndpoint) {
        /*
         * We need to return:
         * {
         *    "TestService": {ClientCmds: {}, Classes:{}, Streams:{}}
         * }
         */
        let thisNode = this;
        let serviceDefinitions = {};
        let serviceNameList = thisNode.TopologyTracker.ListServices();
        for (let i = 0; i < serviceNameList.length; i++) {
            let serviceName = serviceNameList[i];
            // Unlike before, we don't distribute the actual service definitions in
            // the declarations.  We need to go out to each service and get the info
            let bestInstance = thisNode.TopologyTracker.FindInstanceOfService(serviceName);
            let response = await thisNode.ServiceCmd("DRP", "getServiceDefinition", { serviceName: serviceName }, bestInstance.NodeID, bestInstance.InstanceID, true, true, callingEndpoint);
            response.NodeID = bestInstance.NodeID;
            serviceDefinitions[serviceName] = response;
        }
        return serviceDefinitions;
    }

    GetLocalServiceDefinitions(params, callingEndpoint) {
        /*
         * We need to return:
         * {
         *    "TestService": {ClientCmds: {}, Classes:{}, Streams:{}}
         * }
         */
        let thisNode = this;
        let serviceDefinitions = {};
        let checkServiceName = null;
        if (params) {
            if (params.serviceName) checkServiceName = params.serviceName;
            else if (params.pathList && params.pathList.length > 0) checkServiceName = params.pathList.shift();
        }
        let serviceNameList = Object.keys(thisNode.Services);
        for (let i = 0; i < serviceNameList.length; i++) {
            let serviceName = serviceNameList[i];
            if (checkServiceName && checkServiceName !== serviceName) continue;
            let serviceDefinition = thisNode.Services[serviceName].GetDefinition();
            serviceDefinitions[serviceName] = serviceDefinition;
        }
        return serviceDefinitions;
    }

    /**
     * Return class records
     * @param {{className: string}} params Parameters
     * @returns {Object} Class records
     */
    async GetClassRecords(params) {
        let thisNode = this;

        let results = {};

        // If user didn't supply the className, return null
        if (!params || !params.className) return null;
        let thisClassName = params.className;

        // We need to get a list of all distinct INSTANCES for this class along with the best source for each
        let classInstances = await thisNode.ListClassInstances(params);

        // If we don't have data for this class, return null
        if (!classInstances[thisClassName]) return null;

        let thisClassObj = classInstances[thisClassName];

        // Loop over Class for this service
        let serviceNames = Object.keys(thisClassObj);
        for (let j = 0; j < serviceNames.length; j++) {
            let serviceName = serviceNames[j];

            if (params.serviceName && params.serviceName !== serviceName) continue;

            let recordPath = ["Mesh", "Services", serviceName, "Classes", thisClassName, "cache"];

            // Get data
            let pathParams = { method: "cliGetPath", pathList: recordPath, listOnly: false };
            let pathData = await thisNode.EvalPath(thisNode.GetBaseObj(), pathParams);

            results[serviceName] = pathData.pathItem;

        }
        return results;
    }

    async SendPathCmdToNode(targetNodeID, params) {
        let thisNode = this;
        let oReturnObject = null;

        if (targetNodeID === thisNode.NodeID) {
            // The target NodeID is local
            oReturnObject = thisNode.GetObjFromPath(params, thisNode.GetBaseObj());
        } else {
            try {
                oReturnObject = await thisNode.ServiceCmd("DRP", "pathCmd", params, targetNodeID, null, true, true, null);
            } catch (ex) {
                thisNode.log(`Could not find instance of DRP service for NodeID [${targetNodeID}]`);
            }
        }
        return oReturnObject;
    }

    GetBaseObj() {
        let thisNode = this;
        return {
            NodeID: thisNode.NodeID,
            NodeURL: thisNode.ListeningName,
            DRPNode: thisNode,
            Services: thisNode.Services,
            Streams: thisNode.TopicManager.Topics,
            Endpoints: {
                Nodes: async function (params) {
                    let remainingChildPath = params.pathList;
                    let oReturnObject = null;
                    if (remainingChildPath && remainingChildPath.length > 0) {

                        let targetNodeID = remainingChildPath.shift();

                        // Need to send command to remote Node with remaining tree data
                        params.pathList = remainingChildPath;

                        oReturnObject = await thisNode.SendPathCmdToNode(targetNodeID, params);

                    } else {
                        // Return list of NodeEndpoints
                        oReturnObject = {};
                        let aNodeKeys = Object.keys(thisNode.NodeEndpoints);
                        for (let i = 0; i < aNodeKeys.length; i++) {
                            oReturnObject[aNodeKeys[i]] = {
                                "ConsumerType": "SomeType1",
                                "Status": "Unknown"
                            };
                        }
                    }
                    return oReturnObject;
                },
                Consumers: async function (params) {
                    let remainingChildPath = params.pathList;
                    let oReturnObject = null;
                    if (remainingChildPath && remainingChildPath.length > 0) {

                        let agentID = remainingChildPath.shift();

                        // Need to send command to consumer with remaining tree data
                        params.pathList = remainingChildPath;
                        let targetEndpoint = await thisNode.VerifyConsumerConnection(agentID);

                        if (targetEndpoint) {
                            // Await for command from consumer
                            let results = await targetEndpoint.SendCmd("DRP", "pathCmd", params, true, null);
                            if (results && results.payload && results.payload) {
                                oReturnObject = results.payload;
                            }
                        } else {
                            thisNode.log(`Could not verify consumer connection for [${agentID}]`, true);
                        }

                    } else {
                        // Return list of consumers
                        oReturnObject = {};
                        let aConsumerKeys = Object.keys(thisNode.ConsumerEndpoints);
                        for (let i = 0; i < aConsumerKeys.length; i++) {
                            oReturnObject[aConsumerKeys[i]] = {
                                "ConsumerType": "SomeType1",
                                "Status": "Unknown"
                            };
                        }
                    }
                    return oReturnObject;
                }
            },
            Mesh: {
                Topology: thisNode.TopologyTracker,
                Nodes: async function (params) {
                    // List nodes or redirect to target Node
                    let remainingChildPath = params.pathList;
                    let oReturnObject = {};
                    if (remainingChildPath && remainingChildPath.length > 0) {
                        // Send PathCmd to target
                        let targetNodeID = remainingChildPath.shift();
                        params.pathList = remainingChildPath;
                        oReturnObject = await thisNode.SendPathCmdToNode(targetNodeID, params);
                    } else {
                        // List nodes
                        let aNodeKeys = Object.keys(thisNode.TopologyTracker.NodeTable);
                        for (let i = 0; i < aNodeKeys.length; i++) {
                            oReturnObject[aNodeKeys[i]] = {
                                "ConsumerType": "SomeType1",
                                "Status": "Unknown"
                            };
                        }
                    }
                    return oReturnObject;
                },
                Streams: async function (params) {
                    //console.log("Checking Streams...");
                    let oReturnObject = {};

                    // We need to evaluate the service table, get distinct list of stream names
                    let streamHash = {};
                    let thisSubMgr = thisNode.SubscriptionManager;
                    let serviceTable = thisSubMgr.DRPNode.TopologyTracker.ServiceTable;
                    let serviceEntryIDList = Object.keys(serviceTable);
                    for (let i = 0; i < serviceEntryIDList.length; i++) {
                        let serviceEntry = serviceTable[serviceEntryIDList[i]];
                        for (let j = 0; j < serviceEntry.Streams.length; j++) {
                            let thisStreamName = serviceEntry.Streams[j];
                            if (!streamHash[thisStreamName]) streamHash[thisStreamName] = {};
                            streamHash[thisStreamName][serviceEntry.NodeID] = async (params) => {
                                params.pathList = ['DRPNode', 'TopicManager', 'Topics', thisStreamName].concat(params.pathList);
                                let returnObj = await thisNode.SendPathCmdToNode(serviceEntry.NodeID, params);
                                return returnObj;
                            }
                        }
                    }

                    oReturnObject = thisNode.GetObjFromPath(params, streamHash);
                    return oReturnObject;
                },
                Services: async function (params) {
                    //console.log("Checking Services...");
                    let remainingChildPath = params.pathList;
                    let oReturnObject = {};
                    if (remainingChildPath && remainingChildPath.length > 0) {

                        let serviceName = remainingChildPath.shift();

                        params.pathList = ['Services', serviceName].concat(remainingChildPath);

                        let targetServiceEntry = thisNode.TopologyTracker.FindInstanceOfService(serviceName);
                        if (!targetServiceEntry) return;

                        let targetNodeID = targetServiceEntry.NodeID;
                        //thisNode.log(`Calling node ${targetNodeID}`);

                        oReturnObject = await thisNode.SendPathCmdToNode(targetNodeID, params);

                    } else {
                        // Return list of Service Objects
                        oReturnObject = thisNode.TopologyTracker.GetServicesWithProviders();
                    }
                    return oReturnObject;
                }
            }
        };
    }

    async EvalPath(oCurrentObject, params, callingEndpoint) {
        let oReturnObject = null;

        let aChildPathArray = params.pathList;

        // Do we have a path array?
        if (aChildPathArray.length === 0) {
            // No - act on parent object
            oReturnObject = oCurrentObject;
        } else {
            // Yes - get child
            PathLoop:
            for (let i = 0; i < aChildPathArray.length; i++) {

                let pathItemName = aChildPathArray[i];

                // User is trying to access a hidden attribute; return null
                if (pathItemName.substring(0, 2) === '__') return null;

                // Does the child exist?
                if (pathItemName in oCurrentObject) {

                    // See what we're dealing with
                    let objectType = typeof oCurrentObject[pathItemName];
                    switch (objectType) {
                        case 'object':
                            // Check security
                            if (oCurrentObject[pathItemName].__permissionSet) {
                                // This is a securable item - verify the caller has permissions to see it
                                let isAllowed = oCurrentObject[pathItemName].__IsAllowed(params.AuthInfo, "read");
                                if (!isAllowed) return { err: "Unauthorized" };
                            }

                            // Special handling needed for Set objects
                            let attrType = Object.prototype.toString.call(oCurrentObject[pathItemName]).match(/^\[object (.*)\]$/)[1];

                            switch (attrType) {
                                case "Set":
                                    // Set current object
                                    oCurrentObject = oCurrentObject[pathItemName];
                                    if (i + 1 === aChildPathArray.length) {
                                        // Last one - make this the return object
                                        oReturnObject = [...oCurrentObject];
                                    } else {
                                        // More to the path; skip to the next one
                                        i++;
                                        let setIndexString = pathItemName;
                                        // If the provided index isn't a number, return
                                        if (isNaN(setIndexString)) return oReturnObject;
                                        let setIndexInt = parseInt(setIndexString);
                                        // If the provided index is out of range, return
                                        if (setIndexInt + 1 > oCurrentObject.size) return oReturnObject;
                                        oCurrentObject = [...oCurrentObject][setIndexInt];

                                        if (i + 1 === aChildPathArray.length) {
                                            // Last one - make this the return object
                                            oReturnObject = oCurrentObject;
                                        }
                                    }
                                    break;
                                default:
                                    // Set current object
                                    oCurrentObject = oCurrentObject[pathItemName];
                                    if (i + 1 === aChildPathArray.length) {
                                        // Last one - make this the return object
                                        oReturnObject = oCurrentObject;
                                    }
                            }
                            break;
                        case 'function':
                            // Send the rest of the path to a function
                            let remainingPath = aChildPathArray.splice(i + 1);
                            params.pathList = remainingPath;
                            oReturnObject = await oCurrentObject[pathItemName](params, callingEndpoint);
                            break PathLoop;
                        case 'string':
                            oReturnObject = oCurrentObject[pathItemName];
                            break PathLoop;
                        case 'number':
                            oReturnObject = oCurrentObject[pathItemName];
                            break PathLoop;
                        case 'boolean':
                            oReturnObject = oCurrentObject[pathItemName];
                            break PathLoop;
                        default:
                            break PathLoop;
                    }

                } else {
                    // Child doesn't exist
                    break PathLoop;
                }
            }
        }

        return oReturnObject;
    }

    /**
   * @param {object} params Remaining path
   * @param {Boolean} baseObj Flag to return list of children
   * @param {object} callingEndpoint Endpoint making request
   * @returns {object} oReturnObject Return object
   */
    async GetObjFromPath(params, baseObj, callingEndpoint) {

        // Return object
        let oReturnObject = await this.EvalPath(baseObj, params, callingEndpoint);

        // If we have a return object and want only a list of children, do that now
        if (params.listOnly) {
            if (oReturnObject instanceof Object) {
                if (oReturnObject.err) return oReturnObject;
                if (!oReturnObject.pathItemList) {
                    // Return only child keys and data types
                    oReturnObject = { pathItemList: this.ListObjChildren(oReturnObject) };
                }
            }
        } else if (oReturnObject) {
            if (oReturnObject instanceof Object) {
                if (oReturnObject.err) return oReturnObject;
            }

            if (!(oReturnObject instanceof Object) || !oReturnObject["pathItem"]) {
                // Return object as item
                oReturnObject = { pathItem: oReturnObject };
            }
        }

        return oReturnObject;
    }

    /**
     * 
     * @param {string} remoteNodeID NodeID to connect to
     * @returns {DRP_Endpoint} DRP Node Endpoint
     */
    async VerifyNodeConnection(remoteNodeID) {

        let thisNode = this;

        /** @type DRP_NodeDeclaration */
        let thisNodeEntry = thisNode.TopologyTracker.NodeTable[remoteNodeID];
        if (!thisNodeEntry) return null;

        /** @type {DRP_Endpoint} */
        let thisNodeEndpoint = thisNode.NodeEndpoints[remoteNodeID];

        // Is the remote node listening?  If so, try to connect
        if (!thisNodeEndpoint && thisNodeEntry.NodeURL) {
            let targetNodeURL = thisNodeEntry.NodeURL;

            // We have a target URL, wait a few seconds for connection to initiate
            thisNode.log(`Connecting to Node [${remoteNodeID}] @ '${targetNodeURL}'`, true);
            thisNodeEndpoint = new DRP_NodeClient(targetNodeURL, thisNode.WebProxyURL, thisNode, remoteNodeID);
            thisNode.NodeEndpoints[remoteNodeID] = thisNodeEndpoint;

            for (let i = 0; i < 50; i++) {

                // Are we still trying?
                if (thisNodeEndpoint.IsConnecting()) {
                    // Yes - wait
                    await thisNode.Sleep(100);
                } else {
                    // No - break the for loop
                    break;
                }
            }
        }

        // If this node is listening, try sending a back connection request to the remote node via the registry
        if ((!thisNodeEndpoint || !thisNodeEndpoint.IsReady()) && thisNode.ListeningName) {

            thisNode.log("Sending back request...", true);
            // Let's try having the Provider call us; send command through Registry
            try {
                // Get next hop
                let nextHopNodeID = thisNode.TopologyTracker.GetNextHop(remoteNodeID);

                if (nextHopNodeID) {
                    // Found the next hop
                    thisNode.log(`Sending back request to ${remoteNodeID}, relaying to [${nextHopNodeID}]`, true);
                    let routeOptions = {
                        srcNodeID: thisNode.NodeID,
                        tgtNodeID: remoteNodeID,
                        routeHistory: []
                    };
                    thisNode.NodeEndpoints[nextHopNodeID].SendCmd("DRP", "connectToNode", { "targetNodeID": thisNode.NodeID, "targetURL": thisNode.ListeningName }, false, null, routeOptions);
                } else {
                    // Could not find the next hop
                    throw `Could not find next hop to [${remoteNodeID}]`;
                }

            } catch (err) {
                thisNode.log(`ERR!!!! [${err}]`);
            }

            this.log("Starting wait...", true);
            // Wait a few seconds
            for (let i = 0; i < 50; i++) {

                // Are we still trying?
                if (!thisNode.NodeEndpoints[remoteNodeID] || !thisNode.NodeEndpoints[remoteNodeID].IsReady()) {
                    // Yes - wait
                    await thisNode.Sleep(100);
                } else {
                    // No - break the for loop
                    thisNode.log(`Received back connection from remote node [${remoteNodeID}]`, true);
                    i = 50;
                }
            }

            // If still not successful, delete DRP_NodeClient
            if (!thisNode.NodeEndpoints[remoteNodeID] || !thisNode.NodeEndpoints[remoteNodeID].IsReady()) {
                thisNode.log(`Could not open connection to Node [${remoteNodeID}]`, true);
                if (thisNode.NodeEndpoints[remoteNodeID]) {
                    delete thisNode.NodeEndpoints[remoteNodeID];
                }
                //throw new Error(`Could not get connection to Provider ${remoteNodeID}`);
            } else {
                thisNodeEndpoint = thisNode.NodeEndpoints[remoteNodeID];
            }
        }

        return thisNodeEndpoint;
    }

    /**
     *
     * @param {string} consumerID Consumer ID to connect to
     * @returns {DRP_Endpoint} DRP Consumer Endpoint
     */
    async VerifyConsumerConnection(consumerID) {

        let thisNode = this;

        let thisConsumerEndpoint = null;

        // Make sure the consumer session is active
        if (thisNode.ConsumerEndpoints[consumerID] && thisNode.ConsumerEndpoints[consumerID].IsReady()) thisConsumerEndpoint = thisNode.ConsumerEndpoints[consumerID];

        return thisConsumerEndpoint;
    }

    /**
     * 
     * @param {DRP_Service} serviceObj DRP Service
     */
    async AddService(serviceObj) {
        let thisNode = this;

        if (serviceObj && serviceObj.serviceName && serviceObj.ClientCmds) {
            thisNode.Services[serviceObj.serviceName] = serviceObj;

            // Create topics for service
            for (let i = 0; i < serviceObj.Streams.length; i++) {
                let streamName = serviceObj.Streams[i];
                if (!thisNode.TopicManager.Topics[streamName]) thisNode.TopicManager.CreateTopic(streamName, 1000);
            }

            let newServiceEntry = new DRP_ServiceTableEntry(thisNode.NodeID, null, serviceObj.serviceName, serviceObj.Type, serviceObj.InstanceID, serviceObj.Zone, serviceObj.Sticky, serviceObj.Priority, serviceObj.Weight, serviceObj.Scope, serviceObj.Dependencies, serviceObj.Streams, serviceObj.Status);
            let addServicePacket = new DRP_TopologyPacket(thisNode.NodeID, "add", "service", newServiceEntry.InstanceID, newServiceEntry.Scope, newServiceEntry.Zone, newServiceEntry);
            thisNode.TopologyTracker.ProcessPacket(addServicePacket, thisNode.NodeID);
        }
    }

    async RemoveService(serviceName) {
        let thisNode = this;
        /*
         * TO DO - Add code to:
         * 1. Remove from thisNode.Services[]
         * 2. Create a delete Topology packet
         * 3. Execute ProcessPacket
         * 
         * This function is not currently used anywhere since services are normally
         * removed with the termination of the entire process.  Multiple instances
         * of a single service type should not be run from a single process.
         */
    }

    // Execute a service command with the option of routing via the control plane
    async ServiceCmd(serviceName, method, params, targetNodeID, targetServiceInstanceID, useControlPlane, awaitResponse, callingEndpoint) {
        let thisNode = this;
        let baseErrMsg = "ERROR - ";

        // If if no service or command is provided, return null
        if (!serviceName || !method) return "ServiceCmd: must provide serviceName and method";

        // If no targetNodeID was provided, we need to find a record in the ServiceTable
        if (!targetNodeID) {

            // If no targetServiceInstanceID was provided, we should attempt to locate a service instance

            /** @type {DRP_ServiceTableEntry} */
            let targetServiceRecord = null;

            if (!targetServiceInstanceID) {
                // Update to use the DRP_TopologyTracker object
                targetServiceRecord = thisNode.TopologyTracker.FindInstanceOfService(serviceName);

                // If no match is found then return null
                if (!targetServiceRecord) return null;

                // Assign target Node & Instance IDs
                targetServiceInstanceID = targetServiceRecord.InstanceID;
                targetNodeID = targetServiceRecord.NodeID;

                //if (thisNode.Debug) thisNode.log(`Best instance of service [${serviceName}] is [${targetServiceRecord.InstanceID}] on node [${targetServiceRecord.NodeID}]`, true);
            } else {
                targetServiceRecord = thisNode.TopologyTracker.ServiceTable[targetServiceInstanceID];

                // If no match is found then return null
                if (!targetServiceRecord) return null;

                // Assign target Node
                targetNodeID = targetServiceRecord.NodeID;
            }
        }

        // We don't have a target NodeID
        if (!targetNodeID || !thisNode.TopologyTracker.NodeTable[targetNodeID]) return null;

        // Where is the service?
        if (targetNodeID === thisNode.NodeID) {
            // Execute locally
            let localServiceProvider = null;
            if (serviceName === "DRP" && callingEndpoint) {
                // If the service is DRP and the caller is a remote endpoint, execute from that caller's EndpointCmds
                localServiceProvider = callingEndpoint.EndpointCmds;
            } else {
                if (thisNode.Services[serviceName]) localServiceProvider = thisNode.Services[serviceName].ClientCmds;
            }

            if (!localServiceProvider) {
                thisNode.log(`${baseErrMsg} service ${serviceName} does not exist`, true);
                return null;
            }

            if (!localServiceProvider[method]) {
                thisNode.log(`${baseErrMsg} service ${serviceName} does not have method ${method}`, true);
                return null;
            }

            if (awaitResponse) {
                let results = await localServiceProvider[method](params, callingEndpoint);
                return results;
            } else {
                localServiceProvider[method](params, callingEndpoint);
                return;
            }
        } else {
            // Execute on another Node
            let routeNodeID = targetNodeID;
            let routeOptions = {};

            if (thisNode.ConnectedToControlPlane && !useControlPlane) {
                let localNodeEntry = thisNode.TopologyTracker.NodeTable[thisNode.NodeID];
                let remoteNodeEntry = thisNode.TopologyTracker.NodeTable[targetNodeID];

                // If the remote Node isn't found, return error message
                if (!remoteNodeEntry) return `Node ${targetNodeID} not found in NodeTable`;

                // Make sure either the local Node or remote Node are listening; if not, route via control plane
                if (!localNodeEntry.NodeURL && !remoteNodeEntry.NodeURL) {
                    // Neither the local node nor the remote node are listening, use control plane
                    useControlPlane = true;
                }

                // If the local Node is not a Registry and the remote Node is a non-connected Registry, route via control plane
                if (!useControlPlane && !localNodeEntry.IsRegistry() && remoteNodeEntry.IsRegistry() && !thisNode.NodeEndpoints[targetNodeID]) {
                    useControlPlane = true;
                }

                // If the local Node is a Registry and the remote Node is a non-connected non-Registry, route via control plane
                if (!useControlPlane && localNodeEntry.IsRegistry() && !remoteNodeEntry.IsRegistry() && !thisNode.NodeEndpoints[targetNodeID]) {
                    useControlPlane = true;
                }
            }

            if (useControlPlane) {
                // We want to use to use the control plane instead of connecting directly to the target
                if (thisNode.ConnectedToControlPlane) {
                    routeNodeID = thisNode.TopologyTracker.GetNextHop(targetNodeID);
                    routeOptions = new DRP_RouteOptions(thisNode.NodeID, targetNodeID);
                } else {
                    // We're not connected to a Registry; fallback to VerifyNodeConnection
                    routeNodeID = targetNodeID;
                }
            }

            let routeNodeConnection = await thisNode.VerifyNodeConnection(routeNodeID);

            if (!routeNodeConnection) {
                let errMsg = `Could not establish connection from Node[${thisNode.NodeID}] to Node[${routeNodeID}]`;
                thisNode.log(`ERROR - ${errMsg}`);
                return errMsg;
            }

            if (awaitResponse) {
                let cmdResponse = await routeNodeConnection.SendCmd(serviceName, method, params, true, null, routeOptions, targetServiceInstanceID);
                return cmdResponse.payload;
            } else {
                routeNodeConnection.SendCmd(serviceName, method, params, false, null, routeOptions, targetServiceInstanceID);
                return;
            }
        }
    }

    /**
     * Validate node declaration against local node
     * @param {DRP_NodeDeclaration} declaration Node declaration to check
     * @returns {boolean} Successful [true/false]
     */
    ValidateNodeDeclaration(declaration) {
        let thisNode = this;
        let returnVal = false;

        if (!declaration.NodeID) return false;

        // Do the domains match?
        if (!thisNode.DomainName && !declaration.DomainName || thisNode.DomainName === declaration.DomainName) {
            // Do the domain keys match?
            if (!thisNode.__MeshKey && !declaration.MeshKey || thisNode.__MeshKey === declaration.MeshKey) {
                // Yes - allow the remote node to connect
                returnVal = true;
            }
        }

        return returnVal;
    }

    /**
    * Process Hello packet from remote node (inbound connection)
    * @param {DRP_NodeDeclaration} declaration DRP Node Declaration
    * @param {DRP_Endpoint} sourceEndpoint Source DRP endpoint
    * @param {any} token Reply token
    * @returns {object} Unsure
    */
    async Hello(declaration, sourceEndpoint, token) {
        let thisNode = this;

        let results = null;

        let isDeclarationValid = typeof declaration !== "undefined" && typeof declaration.NodeID !== "undefined" && declaration.NodeID !== null && declaration.NodeID !== "";

        if (isDeclarationValid) {
            // This is a node declaration
            sourceEndpoint.EndpointType = "Node";
            if (thisNode.Debug) thisNode.log(`Remote node client sent Hello [${declaration.NodeID}]`, true);

            // Validate the remote node's domain and key (if applicable)
            if (!thisNode.ValidateNodeDeclaration(declaration)) {
                // The remote node did not offer a MeshKey or the key does not match
                thisNode.log(`Node [${declaration.NodeID}] declaration could not be validated`);
                sourceEndpoint.Close();
                return null;
            }

            let sourceIsRegistry = declaration.NodeRoles.indexOf("Registry") >= 0;

            // Should we redirect this Node to a Registry in another Zone?
            if (thisNode.IsRegistry() && !sourceIsRegistry && declaration.Zone !== thisNode.Zone) {
                // We are a Registry and a Node from another zone has connected to us
                let zoneRegistryList = thisNode.TopologyTracker.FindRegistriesInZone(declaration.Zone);
                if (zoneRegistryList.length > 0) {
                    // Let's tell the remote Node to redirect
                    thisNode.log(`Redirecting Node[${declaration.NodeID}] to one of these registry URLs: ${zoneRegistryList}`);
                    await sourceEndpoint.SendCmd("DRP", "connectToRegistryInList", zoneRegistryList, true, null, null);
                    return;
                } else {
                    thisNode.log(`Could not find a Registry in Zone[${declaration.Zone}] for Node[${declaration.NodeID}]`);
                }
            }

            results = { status: "OK" };

            // Add to NodeEndpoints
            sourceEndpoint.EndpointID = declaration.NodeID;
            thisNode.NodeEndpoints[declaration.NodeID] = sourceEndpoint;

            // Apply all Node Endpoint commands
            thisNode.ApplyNodeEndpointMethods(sourceEndpoint);

            let localNodeIsProxy = false;

            // If the local node is not a Registry and we do not know about the remote node, this is a proxy for that node
            if (!thisNode.IsRegistry() && !thisNode.TopologyTracker.NodeTable[declaration.NodeID]) {
                localNodeIsProxy = true;
            }

            await thisNode.TopologyTracker.ProcessNodeConnect(sourceEndpoint, declaration, localNodeIsProxy);

        } else if (declaration.userAgent) {
            // We need to authenticate the Consumer.  Could be using a static token or a name/password.  Need to implement
            // an authentication function.  Authorization to be handled by target services.

            // Authenticate the consumer
            /** @type {DRP_AuthResponse} */
            let authResponse = await thisNode.Authenticate(declaration.user, declaration.pass, declaration.token);

            // Authentication function did not return successfully
            if (!authResponse) {
                if (thisNode.Debug) {
                    thisNode.log(`Failed to authenticate Consumer`);
                    console.dir(declaration);
                }
                sourceEndpoint.Close();
                return;
            }

            // Authentication 
            if (thisNode.Debug) {
                thisNode.log(`Authenticated Consumer`);
                console.dir(authResponse);
            }

            results = { status: "OK" };

            // This is a consumer declaration
            sourceEndpoint.EndpointType = "Consumer";
            sourceEndpoint.UserAgent = declaration.userAgent;
            // Assign Authentication Response
            sourceEndpoint.AuthInfo = {
                type: "token",
                value: authResponse.Token,
                userInfo: authResponse
            };
            // Moved from wsOpen handler
            if (!thisNode.ConsumerConnectionID) thisNode.ConsumerConnectionID = 1;
            // Assign ID using simple counter for now
            let remoteEndpointID = thisNode.ConsumerConnectionID;
            thisNode.ConsumerConnectionID++;

            sourceEndpoint.EndpointID = remoteEndpointID;
            thisNode.ConsumerEndpoints[remoteEndpointID] = sourceEndpoint;

            // Apply all Node Endpoint commands
            thisNode.ApplyConsumerEndpointMethods(sourceEndpoint);

            if (thisNode.Debug) thisNode.log(`Added ConsumerEndpoint[${sourceEndpoint.EndpointID}], type '${sourceEndpoint.EndpointType}'`);
        }
        else results = "INVALID DECLARATION";

        return results;
    }

    /**
     * 
     * @param {DRP_TopologyPacket} topologyPacket DRP Topology Packet
     * @param {DRP_Endpoint} srcEndpoint Source Endpoint
     */
    async TopologyUpdate(topologyPacket, srcEndpoint) {
        let thisNode = this;
        thisNode.TopologyTracker.ProcessPacket(topologyPacket, srcEndpoint.EndpointID);
    }

    async PingDomainRegistries(domainName) {
        let thisNode = this;

        let srvHash = null;

        if (thisNode.TestMode) {
            thisNode.log("TESTMODE is set - overriding domain, using localhost:8082-8085");
            srvHash = {
                "localhost-8082": { "name": os.hostname(), "port": 8082 },
                "localhost-8083": { "name": os.hostname(), "port": 8083 },
                "localhost-8084": { "name": os.hostname(), "port": 8084 },
                "localhost-8085": { "name": os.hostname(), "port": 8085 }
            };
        } else {
            // Get SRV Records for domain
            let recordList = await dns.resolveSrv(`_drp._tcp.${domainName}`);

            // Prep records
            srvHash = recordList.reduce((map, srvRecord) => {
                let key = `${srvRecord.name}-${srvRecord.port}`;
                srvRecord.pingInfo = null;
                map[key] = srvRecord;
                return map;
            }, {});
        }

        let srvKeys = Object.keys(srvHash);

        // Run tcp pings in parallel
        await Promise.all(
            srvKeys.map(async (srvKey) => {
                let srvRecord = srvHash[srvKey];
                try {
                    srvRecord.pingInfo = await tcpPing({
                        address: srvRecord.name,
                        port: srvRecord.port,
                        timeout: 1000,
                        attempts: 3
                    });
                }
                catch (ex) {
                    // Cannot do tcpPing against host:port
                    thisNode.log(`TCP Pings errored: ${ex}`);
                }
            })
        );
        return srvHash;
    }

    /**
     * Handle connection to Registry Node (post Hello)
     * @param {DRP_NodeClient} nodeClient Node client to Registry
     */
    async RegistryClientHandler(nodeClient) {
        let thisNode = this;
        // Get peer info
        let getDeclarationResponse = await nodeClient.SendCmd("DRP", "getNodeDeclaration", null, true, null);
        if (getDeclarationResponse && getDeclarationResponse.payload && getDeclarationResponse.payload.NodeID) {
            let registryNodeID = getDeclarationResponse.payload.NodeID;
            nodeClient.EndpointID = registryNodeID;
            thisNode.NodeEndpoints[registryNodeID] = nodeClient;
        } else return;

        // Get Registry
        thisNode.TopologyTracker.ProcessNodeConnect(nodeClient, getDeclarationResponse.payload);
    }

    /**
    * Connect to a Registry node via URL
    * @param {string} registryURL DRP Domain FQDN
    * @param {function} openCallback Callback on connection open (Optional)
    * @param {function} closeCallback Callback on connection close (Optional)
    */
    async ConnectToRegistry(registryURL, openCallback, closeCallback) {
        let thisNode = this;
        let retryOnClose = true;
        if (!openCallback || typeof openCallback !== 'function') {
            openCallback = () => { };
        }
        if (closeCallback && typeof closeCallback === 'function') {
            retryOnClose = false;
        } else closeCallback = () => { };
        // Initiate Registry Connection
        let nodeClient = new DRP_NodeClient(registryURL, thisNode.WebProxyURL, thisNode, null, retryOnClose, async () => {

            // This is the callback which occurs after our Hello packet has been accepted
            await thisNode.RegistryClientHandler(nodeClient);
            openCallback();
        }, closeCallback);
    }

    /**
    * Connect to a Registry node via URL - used for retargeting
    * @param {string} registryURL DRP Domain FQDN
    * @param {function} openCallback Callback on connection open
    * @param {function} closeCallback Callback on connection close
    */
    async ConnectToAnotherRegistry(registryURL, openCallback, closeCallback) {
        let thisNode = this;

        // Initiate Registry Connection
        let nodeClient = new DRP_NodeClient(registryURL, thisNode.WebProxyURL, thisNode, null, false, async () => {
            if (thisNode.ConnectedToControlPlane) {
                thisNode.log(`We are already connected to the Control Plane!  No longer need connection to ${registryURL}`);
                //nodeClient.Close();
                return;
            }
            // This is the callback which occurs after our Hello packet has been accepted
            await thisNode.RegistryClientHandler(nodeClient);
            openCallback();
        }, closeCallback);
    }

    // This is for non-Registry nodes
    async ConnectToRegistryByDomain() {
        let thisNode = this;
        // Look up SRV records for DNS
        thisNode.log(`Looking up a Registry Node for domain [${thisNode.DomainName}]...`);
        try {
            let srvHash = await thisNode.PingDomainRegistries(thisNode.DomainName);
            let srvKeys = Object.keys(srvHash);
            // Find registry with lowest average ping time
            let closestRegistry = null;
            for (let i = 0; i < srvKeys.length; i++) {
                let checkRegistry = srvHash[srvKeys[i]];
                if (checkRegistry.pingInfo && checkRegistry.pingInfo.avg) {
                    if (!closestRegistry || checkRegistry.pingInfo.avg < closestRegistry.pingInfo.avg) {
                        closestRegistry = checkRegistry;
                    }
                }
            }
            if (closestRegistry) {
                let protocol = "ws";

                // Dirty check to see if the port is SSL; are the last three digits 44x?
                let portString = closestRegistry.port.toString();
                let checkString = portString.slice(-3, 3);
                if (checkString === "44") {
                    protocol = "wss";
                }

                // Connect to target
                let registryURL = `${protocol}://${closestRegistry.name}:${closestRegistry.port}`;

                let nodeClient = new DRP_NodeClient(registryURL, thisNode.WebProxyURL, thisNode, null, false, async () => {

                    // This is the callback which occurs after our Hello packet has been accepted
                    thisNode.RegistryClientHandler(nodeClient);

                }, async () => {
                    // Disconnect Callback; try again if we're not connected to another Registry
                    if (!thisNode.ConnectedToControlPlane) {
                        await thisNode.Sleep(5000);
                        thisNode.ConnectToRegistryByDomain();
                    }
                });

            } else {
                thisNode.log(`Could not find active registry`);
                await thisNode.Sleep(5000);
                thisNode.ConnectToRegistryByDomain();
            }

        } catch (ex) {
            thisNode.log(`Error resolving DNS: ${ex}`);
            await thisNode.Sleep(5000);
            thisNode.ConnectToRegistryByDomain();
        }
    }

    /**
     * This function is used by Registry Nodes to connect to other Registry Nodes using SRV records
     * */
    async ConnectToOtherRegistries() {
        let thisNode = this;
        try {

            let srvHash = await thisNode.PingDomainRegistries(thisNode.DomainName);
            let srvKeys = Object.keys(srvHash);

            // Connect to all remote registries
            for (let i = 0; i < srvKeys.length; i++) {
                let checkRegistry = srvHash[srvKeys[i]];

                // Skip the local registry
                let checkNamePort = `^wss?://${checkRegistry.name}:${checkRegistry.port}$`;
                let regExp = new RegExp(checkNamePort);
                if (thisNode.ListeningName.match(regExp)) {
                    continue;
                }

                // Is the registry host reachable?
                if (checkRegistry.pingInfo && checkRegistry.pingInfo.avg) {
                    // Dirty check to see if the port is SSL; are the last three digits 44x?
                    let protocol = "ws";
                    let portString = checkRegistry.port.toString();
                    let checkString = portString.slice(-3, 3);
                    if (checkString === "44") {
                        protocol = "wss";
                    }
                    // Connect to target
                    let registryURL = `${protocol}://${checkRegistry.name}:${checkRegistry.port}`;

                    thisNode.ReconnectWaitTimeSeconds = 10;

                    let registryDisconnectCallback = async () => {
                        // On failure, wait 10 seconds, see if the remote registry is connected back then try again
                        // For each attempt, increase the wait time by 10 seconds up to 5 minutes
                        await thisNode.Sleep(thisNode.ReconnectWaitTimeSeconds * 1000);
                        if (!thisNode.TopologyTracker.GetNodeWithURL(registryURL)) {
                            thisNode.ConnectToRegistry(registryURL, null, registryDisconnectCallback);
                            if (thisNode.ReconnectWaitTimeSeconds < 300) thisNode.ReconnectWaitTimeSeconds += 10;
                        }
                    };

                    thisNode.ConnectToRegistry(registryURL, null, registryDisconnectCallback);
                }
            }

        } catch (ex) {
            thisNode.log(`Error resolving DNS: ${ex}`);
        }
    }

    /**
    * Connect to a specific Registry URL
    * @param {string[]} registryList DRP Domain FQDN
    * @param {DRP_NodeClient} endpoint Callback on connection close (Optional)
    * @returns {Promise} Returns Promise
    */
    ConnectToRegistryInList(registryList, endpoint) {
        let thisNode = this;

        let targetRegistryURL = registryList[Math.floor(Math.random() * registryList.length)];

        // We've been asked to contact another Registry
        thisNode.log(`We've been asked to contact another Registry in list [${registryList}], selected ${targetRegistryURL}`);

        returnVal = new Promise(function (resolve, reject) {
            thisNode.ConnectToAnotherRegistry(targetRegistryURL, () => {
                // We've connected to the new Registry; close the connection to the previous one
                endpoint.Close();
                resolve();
            }, () => {
                // The connection closed; fallback to connection by domain (SRV lookup)
                if (!thisNode.ConnectedToControlPlane) {
                    thisNode.ConnectToRegistryByDomain();
                }
            });
        });
        return returnVal;
    }

    /**
     * 
     * @param {function} onControlPlaneConnect Execute after connection to Control Plane
     */
    async ConnectToMesh(onControlPlaneConnect) {
        let thisNode = this;
        if (onControlPlaneConnect) thisNode.onControlPlaneConnect = onControlPlaneConnect;
        // If this is a Registry, seed the Registry with it's own declaration
        if (thisNode.IsRegistry()) {
            if (this.DomainName) {
                // A domain name was provided; attempt to cluster with other registry hosts
                this.log(`This node is a Registry for ${this.DomainName}, attempting to contact other Registry nodes`);
                this.ConnectToOtherRegistries();
            }
            if (thisNode.onControlPlaneConnect && typeof thisNode.onControlPlaneConnect === "function") thisNode.onControlPlaneConnect();
        } else {
            if (this.RegistryUrl) {
                // A specific Registry URL was provided
                this.ConnectToRegistry(this.RegistryUrl);
            } else if (this.DomainName) {
                // A domain name was provided; attempt to connect to a registry host
                this.ConnectToRegistryByDomain();
            } else {
                // No Registry URL or domain provided
                this.Die("No DomainName or RegistryURL provided!");
            }
        }
    }

    async ConnectToNode(params) {
        let thisNode = this;
        let targetNodeID = params.targetNodeID;
        let targetURL = params.targetURL;

        // Initiate Node Connection
        if (thisNode.NodeEndpoints[targetNodeID] && thisNode.NodeEndpoints[targetNodeID].wsConn.readyState < 2) {
            // We already have this NodeEndpoint registered and the wsConn is opening or open
            thisNode.log(`Received back request, already have NodeEndpoints[${targetNodeID}]`, true);
        } else {
            thisNode.log(`Received back request, connecting to [${targetNodeID}] @ ${params.wsTarget}`, true);
            thisNode.NodeEndpoints[targetNodeID] = new DRP_NodeClient(targetURL, thisNode.WebProxyURL, thisNode, targetNodeID, false, null, null);
        }
    }

    ListObjChildren(oTargetObject) {
        // Return only child keys and data types
        let pathObjList = [];
        if (oTargetObject && typeof oTargetObject === 'object') {
            for (let attrName in oTargetObject) {
                if (attrName.substring(0, 2) === '__') continue;
                let returnVal;
                let childAttrObj = oTargetObject[attrName];
                let attrType = Object.prototype.toString.call(childAttrObj).match(/^\[object (.*)\]$/)[1];

                switch (attrType) {
                    case "Object":
                        returnVal = Object.keys(childAttrObj).length;
                        break;
                    case "Array":
                        returnVal = childAttrObj.length;
                        break;
                    case "Set":
                        returnVal = childAttrObj.size;
                        break;
                    case "Function":
                        returnVal = null;
                        break;
                    default:
                        returnVal = childAttrObj;
                }

                let pathObj = {
                    "Name": attrName,
                    "Type": attrType,
                    "Value": returnVal
                };

                if (childAttrObj) {
                    pathObj.Type = childAttrObj.constructor.name;
                    switch (pathObj.Type) {
                        case "DRP_TopicMessage":
                            pathObj.Value = childAttrObj.TimeStamp;
                            break;
                        default:
                    }
                }

                pathObjList.push(pathObj);
            }
        }
        return pathObjList;
    }

    IsRegistry() {
        let thisNode = this;
        let isRegistry = thisNode.NodeRoles.indexOf("Registry") >= 0;
        return isRegistry;
    }

    IsBroker() {
        let thisNode = this;
        let isBroker = thisNode.NodeRoles.indexOf("Broker") >= 0;
        return isBroker;
    }

    /**
     * Tell whether this Node is a proxy for another Node
     * @param {string} checkNodeID Node to check
     * @returns {boolean} Is this node a proxy?
     */
    IsProxyFor(checkNodeID) {
        let thisNode = this;
        let isProxy = false;
        let checkNodeEntry = thisNode.TopologyTracker.NodeTable[checkNodeID];
        if (checkNodeEntry && checkNodeEntry.ProxyNodeID === thisNode.NodeID) {
            isProxy = true;
        }
        return isProxy;
    }

    /**
     * Tell whether this Node is connected to another Node
     * @param {string} checkNodeID Node to check
     * @returns {boolean} Is this node connected?
     */
    IsConnectedTo(checkNodeID) {
        let thisNode = this;
        let isConnected = false;
        let checkNodeEntry = thisNode.NodeEndpoints[checkNodeID];
        if (checkNodeEntry) {
            isConnected = true;
        }
        return isConnected;
    }

    async GetTopology(params, callingEndpoint, token) {
        let thisNode = this;
        let topologyObj = {};
        // We need to get a list of all nodes from the registry
        let nodeIDList = Object.keys(thisNode.TopologyTracker.NodeTable);
        for (let i = 0; i < nodeIDList.length; i++) {
            let targetNodeID = nodeIDList[i];
            let topologyNode = {};

            let nodeTableEntry = thisNode.TopologyTracker.NodeTable[targetNodeID];
            let nodeClientConnections = await thisNode.ServiceCmd("DRP", "listClientConnections", null, targetNodeID, null, true, true, callingEndpoint);
            let nodeServices = await thisNode.ServiceCmd("DRP", "getLocalServiceDefinitions", null, targetNodeID, null, true, true, callingEndpoint);

            // Assign Node Table Entry attributes
            Object.assign(topologyNode, nodeTableEntry);

            // Assign Client Connections
            topologyNode.NodeClients = nodeClientConnections.nodeClients;
            topologyNode.ConsumerClients = nodeClientConnections.consumerClients;

            // Assign Services
            topologyNode.Services = nodeServices;

            // Add to hash
            topologyObj[targetNodeID] = topologyNode;

        }

        return topologyObj;
    }

    /**
     * 
     * @param {string} reason Reason for Node termination
     */
    Die(reason) {
        let thisNode = this;
        thisNode.log(`TERMINATING - ${reason}`, false);
        process.exit(1);
    }

    ListClientConnections() {
        let thisNode = this;
        let nodeClientConnections = {
            nodeClients: {},
            consumerClients: {}
        };

        // Loop over NodeEndpoints
        let nodeIDList = Object.keys(thisNode.NodeEndpoints);
        for (let i = 0; i < nodeIDList.length; i++) {
            let nodeID = nodeIDList[i];
            /** @type DRP_Endpoint */
            let thisEndpoint = thisNode.NodeEndpoints[nodeID];
            if (thisEndpoint.IsServer()) {
                nodeClientConnections.nodeClients[nodeID] = thisEndpoint.ConnectionStats();
            }
        }

        // Loop over ConsumerEndpoints
        let consumerIDList = Object.keys(thisNode.ConsumerEndpoints);
        for (let i = 0; i < consumerIDList.length; i++) {
            let consumerID = consumerIDList[i];
            /** @type DRP_Endpoint */
            let thisEndpoint = thisNode.ConsumerEndpoints[consumerID];
            nodeClientConnections.consumerClients[consumerID] = thisEndpoint.ConnectionStats();
        }

        return nodeClientConnections;
    }

    RemoveEndpoint(staleEndpoint, callback) {
        let thisNode = this;
        let staleEndpointID = staleEndpoint.EndpointID;
        if (staleEndpointID) {
            switch (staleEndpoint.EndpointType) {
                case "Node":
                    if (thisNode.NodeEndpoints[staleEndpointID]) {
                        thisNode.log(`Removing disconnected node [${staleEndpointID}]`, true);
                        thisNode.NodeEndpoints[staleEndpointID].RemoveSubscriptions();
                        delete thisNode.NodeEndpoints[staleEndpointID];
                        thisNode.TopologyTracker.ProcessNodeDisconnect(staleEndpointID);
                    }
                    break;
                case "Consumer":
                    if (thisNode.ConsumerEndpoints[staleEndpointID]) {
                        thisNode.ConsumerEndpoints[staleEndpointID].RemoveSubscriptions();
                        delete thisNode.ConsumerEndpoints[staleEndpointID];
                    }
                    break;
                default:
            }
        }
        if (callback && typeof callback === 'function') {
            callback();
        }
    }

    /**
     * Add Generic Methods to Endpoint
     * @param {DRP_Endpoint} targetEndpoint Endpoint to add methods to
     */
    ApplyGenericEndpointMethods(targetEndpoint) {
        let thisNode = this;

        targetEndpoint.RegisterMethod("getEndpointID", async function (...args) {
            return targetEndpoint.EndpointID;
        });

        targetEndpoint.RegisterMethod("getNodeDeclaration", async function (...args) {
            return thisNode.__GetNodeDeclaration();
        });

        targetEndpoint.RegisterMethod("pathCmd", async (params, srcEndpoint, token) => {
            return await thisNode.GetObjFromPath(params, thisNode.GetBaseObj(), srcEndpoint);
        });

        targetEndpoint.RegisterMethod("getRegistry", (params, srcEndpoint, token) => {
            return thisNode.TopologyTracker.GetRegistry(params.reqNodeID);
        });

        targetEndpoint.RegisterMethod("getServiceDefinition", (...args) => {
            return thisNode.GetServiceDefinition(...args);
        });

        targetEndpoint.RegisterMethod("getServiceDefinitions", async function (...args) {
            return await thisNode.GetServiceDefinitions(...args);
        });

        targetEndpoint.RegisterMethod("getLocalServiceDefinitions", function (...args) {
            return thisNode.GetLocalServiceDefinitions(...args);
        });

        targetEndpoint.RegisterMethod("getClassRecords", async (...args) => {
            return await thisNode.GetClassRecords(...args);
        });

        targetEndpoint.RegisterMethod("listClassInstances", async (...args) => {
            return await thisNode.ListClassInstances(...args);
        });

        targetEndpoint.RegisterMethod("sendToTopic", function (params, srcEndpoint, token) {
            thisNode.TopicManager.SendToTopic(params.topicName, params.topicData);
        });

        targetEndpoint.RegisterMethod("getTopology", async function (...args) {
            return await thisNode.GetTopology(...args);
        });

        targetEndpoint.RegisterMethod("listClientConnections", function (...args) {
            return thisNode.ListClientConnections(...args);
        });

        targetEndpoint.RegisterMethod("tcpPing", async (...args) => {
            return thisNode.TCPPing(...args);
        });

        targetEndpoint.RegisterMethod("findInstanceOfService", async (params) => {
            return thisNode.TopologyTracker.FindInstanceOfService(params.serviceName, params.serviceType, params.zone);
        });

        targetEndpoint.RegisterMethod("listServices", async (params) => {
            return thisNode.TopologyTracker.ListServices(params.serviceName, params.serviceType, params.zone);
        });

        targetEndpoint.RegisterMethod("subscribe", async function (params, srcEndpoint, token) {
            // Only allow if the scope is local or this Node is a Broker
            if (params.scope !== "local" && !thisNode.IsBroker()) return null;

            let sendFunction = async (message) => {
                // Returns send status; error if not null
                return await srcEndpoint.SendReply(params.streamToken, 2, message);
            };
            let sendFailCallback = async (sendFailMsg) => {
                // Failed to send; may have already disconnected, take no further action
            };

            // If a specific NodeID is set, override the scope to local
            if (params.targetNodeID) {
                params.scope = "local";
            }

            let thisSubscription = new DRP_Subscriber(params.topicName, params.scope, params.filter || null, params.targetNodeID || null, sendFunction, sendFailCallback);
            srcEndpoint.Subscriptions[params.streamToken] = thisSubscription;
            let results = await thisNode.SubscriptionManager.RegisterSubscription(thisSubscription);
            return results;
            //return await thisNode.Subscribe(thisSubscription);
        });

        targetEndpoint.RegisterMethod("unsubscribe", async function (params, srcEndpoint, token) {
            let response = false;
            let thisSubscription = srcEndpoint.Subscriptions[params.streamToken];
            if (thisSubscription) {
                thisSubscription.Terminate();
                thisNode.SubscriptionManager.Subscribers.delete(thisSubscription);
                response = true;
            }
            return response;
        });

        targetEndpoint.RegisterMethod("refreshSwaggerRouter", async function (params, srcEndpoint, token) {
            let serviceName = null;
            if (params && params.serviceName) {
                // params was passed from cliGetPath
                serviceName = params.serviceName;

            } else if (params && params.pathList && params.pathList.length > 0) {
                // params was passed from cliGetPath
                serviceName = params.pathList.shift();
            } else {
                if (params && params.pathList) return `Format \\refreshSwaggerRouter\\{serviceName}`;
                else return `FAIL - serviceName not defined`;
            }

            if (thisNode.SwaggerRouters[serviceName]) {
                delete thisNode.SwaggerRouters[serviceName];
                let serviceInstance = thisNode.TopologyTracker.FindInstanceOfService(serviceName);
                if (!serviceInstance) return `FAIL - Service [${serviceName}] does not exist`;
                await thisNode.AddSwaggerRouter(serviceName, serviceInstance.NodeID);
                return `OK - Refreshed SwaggerRouters[${serviceName}]`;
            } else {
                return `FAIL - SwaggerRouters[${serviceName}] does not exist`;
            }
        });
    }

    /**
     * Add Methods to Node Endpoint
     * @param {DRP_Endpoint} targetEndpoint Endpoint to add methods to
     */
    ApplyNodeEndpointMethods(targetEndpoint) {
        let thisNode = this;

        thisNode.ApplyGenericEndpointMethods(targetEndpoint);

        targetEndpoint.RegisterMethod("topologyUpdate", async function (...args) {
            return thisNode.TopologyUpdate(...args);
        });

        targetEndpoint.RegisterMethod("connectToNode", async function (...args) {
            return await thisNode.ConnectToNode(...args);
        });

        targetEndpoint.RegisterMethod("addConsumerToken", async function (params, srcEndpoint, token) {
            if (params.tokenPacket) {
                thisNode.ConsumerTokens[params.tokenPacket.Token] = params.tokenPacket;
            }
            return;
        });

        if (targetEndpoint.IsServer && !targetEndpoint.IsServer()) {
            // Add this command for DRP_Client endpoints
            targetEndpoint.RegisterMethod("connectToRegistryInList", async function (...args) {
                return await thisNode.ConnectToRegistryInList(...args);
            });
        }
    }

    /**
     * Add Methods to Consumer Endpoint
     * @param {DRP_Endpoint} targetEndpoint Endpoint to add methods to
     */
    ApplyConsumerEndpointMethods(targetEndpoint) {
        let thisNode = this;

        thisNode.ApplyGenericEndpointMethods(targetEndpoint);

        targetEndpoint.RegisterMethod("getUserInfo", async function (params, srcEndpoint, token) {
            return targetEndpoint.AuthInfo.userInfo;
        });
    }

    /**
     * Subscribe to local or remote topics
     * @param {DRP_Subscriber} thisSubscription Subscription Object
     * @returns {string} results Results
     */
    /*
    async Subscribe(thisSubscription) {
        let thisNode = this;
        let results = false;

        // What is the scope?
        if (thisSubscription.scope === "local" && (!thisSubscription.targetNodeID || thisSubscription.targetNodeID === thisNode.NodeID)) {
            // Subscribe directly to the local TopicManager; the result is the Subscriber ID
            thisNode.TopicManager.SubscribeToTopic(thisSubscription);
            results = true;
        } else if (thisSubscription.scope === "zone" || thisSubscription.scope === "global" || thisSubscription.targetNodeID) {
            results = await thisNode.SubscriptionManager.RegisterSubscription(thisSubscription);
        }
        return results;
    }
    */

    /**
     * 
     * @param {string} targetNodeID Target Node ID
     * @param {string} topicName Topic Name
     * @param {function} streamProcessor Function for processing stream data
     * @returns {string} Subscription token
     */
    async SubscribeRemote(targetNodeID, topicName, streamProcessor) {
        let thisNode = this;
        let returnVal = null;
        // Subscribe to a remote topic
        let thisNodeEndpoint = await thisNode.VerifyNodeConnection(targetNodeID);
        let sourceStreamToken = thisNodeEndpoint.AddReplyHandler(streamProcessor);

        // Await for command from source node
        let successful = await thisNodeEndpoint.SendCmd("DRP", "subscribe", { "topicName": topicName, "streamToken": sourceStreamToken, "scope": "local" }, true, null);
        if (successful) returnVal = sourceStreamToken;
        return returnVal;
    }

    /**
     * 
     * @param {string} targetNodeID Target Node ID
     * @param {string} streamToken Stream Token
     */
    async UnsubscribeRemote(targetNodeID, streamToken) {
        let thisNode = this;
        let returnVal = null;
        // Unsubscribe from a remote topic
        let thisNodeEndpoint = await thisNode.VerifyNodeConnection(targetNodeID);

        // If thisNodeEndpoint is null, it means the connection has already been terminated
        if (!thisNodeEndpoint) return;

        // Delete the reply handler
        thisNodeEndpoint.DeleteReplyHandler(streamToken);

        // Tell remote node this streamToken is no longer valid
        await thisNodeEndpoint.SendCmd("DRP", "unsubscribe", { "streamToken": streamToken }, true, null);
    }

    async Authenticate(userName, password, token) {
        let thisNode = this;
        let authenticationServiceName = null;
        let authResponse = null;

        // If a token is provided, skip the rest of the process
        if (token) {
            return thisNode.ConsumerTokens[token];
        }

        if (this.AuthenticationServiceName) {
            // This Node has been configured to use a specific Authentication service
            authenticationServiceName = this.AuthenticationServiceName;
        } else {
            // Use the best available
            let authenticationServiceRecord = thisNode.TopologyTracker.FindInstanceOfService(null, "Authenticator");
            if (authenticationServiceRecord) authenticationServiceName = authenticationServiceRecord.Name;
        }
        if (authenticationServiceName) {
            authResponse = await thisNode.ServiceCmd(authenticationServiceName, "authenticate", new DRP_AuthRequest(userName, password, token), null, null, true, true, null);
        } else {
            // No authentication service found
            if (thisNode.Debug) thisNode.log(`Attempted to authenticate Consumer but no Authenticator was specified or found`);
        }
        return authResponse;
    }

    async TCPPing(params, srcEndpoint, token) {
        let thisNode = this;
        let pingInfo = null;
        let pingAddress = null;
        let pingPort = null;
        let pingTimeout = 3000;
        let pingAttempts = 1;

        if (params && typeof params === "string") {
            // params is a string formatted "address:port"
            let pingRegExp = /^(.*):(\d+)$/;
            let pingMatch = pingRegExp.exec(params);
            if (pingMatch.length > 0) {
                // Get parts
                pingAddress = match[1];
                pingPort = match[2];
            }
        } else if (params && params.address && params.port) {
            // params contains address and port members
            pingAddress = params.address;
            pingPort = params.port;
            if (params.timeout) pingTimeout = params.timeout;
            if (params.attempts) pingAttempts = params.attempts;
        } else if (params && params.pathList) {
            // params was passed from cliGetPath
            pingAddress = params.pathList.shift();
            pingPort = params.pathList.shift();
            if (!pingAddress || !pingPort) return `Format: \\TCPPing\\{address}\\{port}`;
        }

        if (!pingAddress || !pingPort) return { "address": "127.0.0.1", "port": "80", "timeout": 3000, "attempts": 3 };
        //console.dir(params);
        try {
            pingInfo = await tcpPing({
                address: pingAddress,
                port: pingPort,
                timeout: pingTimeout,
                attempts: pingAttempts
            });
        }
        catch (ex) {
            // Cannot do tcpPing against host:port
            //thisNode.log(`TCP Pings errored: ${ex}`);
        }
        return pingInfo;
    }

    async Sleep(ms) {
        await new Promise(resolve => {
            setTimeout(resolve, ms);
        });
    }

    IsRestricted(securityObj, params) {
        let authInfo = params.AuthInfo;
        try {
            if (authInfo && authInfo.type) {
                // Is it a token or a key?
                switch (authInfo.type) {
                    case 'key':
                        if (securityObj.Keys.indexOf(authInfo.value) >= 0) return false;
                        break;
                    case 'token':
                        // We need to look over the user's groups and see if any are in the group list
                        for (let i = 0; i < authInfo.userInfo.Groups.length; i++) {
                            let userGroupName = authInfo.userInfo.Groups[i];
                            if (securityObj.Groups.indexOf(userGroupName) >= 0) return false;
                        }
                        break;
                    default:
                }
            }
            return true;
        } catch (ex) {
            return false;
        }
    }

    IsTrue(value) {
        if (typeof (value) === 'string') {
            value = value.trim().toLowerCase();
        }
        switch (value) {
            case true:
            case "true":
            case 1:
            case "1":
            case "on":
            case "y":
            case "yes":
                return true;
            default:
                return false;
        }
    }
}

class DRP_NodeClient extends DRP_Client {
    /**
    * @param {string} wsTarget Remote Node WS target
    * @param {string} proxy Web proxy
    * @param {DRP_Node} drpNode Local Node
    * @param {string} endpointID Remote Endpoint ID
    * @param {boolean} retryOnClose Do we retry on close
    * @param {function} openCallback Execute after connection is established
    * @param {function} closeCallback Execute after connection is terminated
    */
    constructor(wsTarget, proxy, drpNode, endpointID, retryOnClose, openCallback, closeCallback) {
        super(wsTarget, proxy, drpNode, endpointID, "Node");
        this.retryOnClose = retryOnClose;
        this.proxy = proxy;
        this.openCallback = openCallback;
        this.closeCallback = closeCallback;
        // Register Endpoint commands
        // (methods should return output and optionally accept [params, token] for streaming)

        this.DRPNode.ApplyNodeEndpointMethods(this);
    }

    // Define Handlers
    async OpenHandler() {
        super.OpenHandler();
        if (this.DRPNode.Debug) this.DRPNode.log("Node client [" + this.RemoteAddress() + ":" + this.RemotePort() + "] opened");
        let response = await this.SendCmd("DRP", "hello", this.DRPNode.__GetNodeDeclaration(), true, null);
        if (this.openCallback && typeof this.openCallback === 'function') {
            this.openCallback(response);
        }
    }

    async CloseHandler(closeCode) {
        let thisEndpoint = this;
        if (this.DRPNode.Debug) this.DRPNode.log("Node client [" + thisEndpoint.RemoteAddress() + ":" + thisEndpoint.RemotePort() + "] closed with code [" + closeCode + "]");

        thisEndpoint.DRPNode.RemoveEndpoint(thisEndpoint, thisEndpoint.closeCallback);

        if (this.retryOnClose) {
            await thisEndpoint.DRPNode.Sleep(5000);
            this.RetryConnection();
        }
    }

    async ErrorHandler(error) {
        if (this.DRPNode.Debug) this.DRPNode.log("Node client encountered error [" + error + "]");
    }

}

// Object which tracks advertised nodes and services
class DRP_TopologyTracker {
    /**
     * 
     * @param {DRP_Node} drpNode Associated DRP Node
     */
    constructor(drpNode) {
        let thisTopologyTracker = this;
        this.DRPNode = drpNode;
        /** @type {Object.<string,DRP_NodeTableEntry>} */
        this.NodeTable = new DRP_NodeTable();
        /** @type {Object.<string,DRP_ServiceTableEntry>} */
        this.ServiceTable = new DRP_ServiceTable();

        this.GetNextHop = this.GetNextHop;
        this.GetServicesWithProviders = this.GetServicesWithProviders;
        this.ListServices = this.ListServices;
        this.ListConnectedRegistryNodes = this.ListConnectedRegistryNodes;
        this.FindRegistriesInZone = this.FindRegistriesInZone;
        this.FindInstanceOfService = this.FindInstanceOfService;
    }

    /**
     * 
     * @param {DRP_TopologyPacket} topologyPacket DRP Topology Packet
     * @param {string} srcNodeID Node we received this from
     * @param {string} sourceIsRegistry Is the source node a Registry?
     */
    ProcessPacket(topologyPacket, srcNodeID, sourceIsRegistry) {
        let thisTopologyTracker = this;
        let thisNode = thisTopologyTracker.DRPNode;
        let targetTable = null;
        /** @type {DRP_TrackingTableEntry} */
        let topologyEntry = null;

        /** @type {DRP_TrackingTableEntry} */
        let advertisedEntry = topologyPacket.data;
        let thisNodeEntry = thisTopologyTracker.NodeTable[thisNode.NodeID];
        let sourceNodeEntry = thisTopologyTracker.NodeTable[srcNodeID];
        let advertisedNodeEntry = thisTopologyTracker.NodeTable[topologyPacket.data.NodeID];
        let learnedFromEntry = thisTopologyTracker.NodeTable[topologyPacket.data.LearnedFrom];

        //thisNode.log(`ProcessPacket -> ${JSON.stringify(topologyPacket)}`);

        // Get Base object
        switch (topologyPacket.type) {
            case "node":
                targetTable = thisTopologyTracker.NodeTable;
                break;
            case "service":
                targetTable = thisTopologyTracker.ServiceTable;
                break;
            default:
                return;
        }

        // Inbound topology packet; service add, update, delete
        switch (topologyPacket.cmd) {
            case "add":
                if (targetTable[topologyPacket.id]) {
                    // We already know about this one
                    if (thisNode.Debug) thisNode.log(`We've received a topologyPacket for a record we already have: ${topologyPacket.type}[${topologyPacket.id}]`);

                    // If we're a Registry and the learned entry is a Registry, ignore it
                    if (thisNodeEntry.IsRegistry() && advertisedNodeEntry.IsRegistry()) return;

                    // Someone sent us info about the local node; ignore it
                    if (advertisedEntry.NodeID === thisNodeEntry.NodeID) return;

                    // We knew about the entry before, but the node just connected to us
                    if (advertisedEntry.NodeID === srcNodeID && targetTable[topologyPacket.id].LearnedFrom !== srcNodeID) {
                        // This is a direct connection from the source; update the LearnedFrom
                        if (thisNode.IsRegistry()) {
                            // Another Registry has made a warm Node handoff to this one
                            if (thisNode.Debug) thisNode.log(`Updating LearnedFrom for ${topologyPacket.type} [${topologyPacket.id}] from [${targetTable[topologyPacket.id].LearnedFrom}] to [${srcNodeID}]`);
                            targetTable[topologyPacket.id].LearnedFrom = srcNodeID;

                            // We may want to redistribute
                            topologyEntry = targetTable[topologyPacket.id];
                            break;
                        } else {
                            if (sourceIsRegistry || sourceNodeEntry && sourceNodeEntry.IsRegistry()) {
                                // A Registry Node has connected to this non-Registry node.
                                if (thisNode.Debug) thisNode.log(`Connected to new Registry, overwriting LearnedFrom for ${topologyPacket.type} [${topologyPacket.id}] from [${targetTable[topologyPacket.id].LearnedFrom}] to [${srcNodeID}]`);
                                targetTable.AddEntry(topologyPacket.id, topologyPacket.data, thisNode.getTimestamp());
                            } else {
                                // A non-Registry Node has connected to this non-Registry node.  Do not update LearnedFrom.
                            }
                            return;
                        }
                    }

                    // We are a Registry and learned about a newer route from another Registry; warm handoff?
                    if (thisNode.IsRegistry() && (sourceIsRegistry || sourceNodeEntry && sourceNodeEntry.IsRegistry()) && advertisedEntry.LearnedFrom === advertisedEntry.NodeID && advertisedNodeEntry.LearnedFrom !== advertisedEntry.NodeID) {
                        //thisNode.log(`Ignoring ${topologyPacket.type} table entry [${topologyPacket.id}] from Node [${srcNodeID}], not not relayed from an authoritative source`);
                        if (thisNode.Debug) thisNode.log(`Updating LearnedFrom for ${topologyPacket.type} [${topologyPacket.id}] from [${targetTable[topologyPacket.id].LearnedFrom}] to [${srcNodeID}]`);
                        targetTable[topologyPacket.id].LearnedFrom = srcNodeID;

                        // We wouldn't want to redistribute to other registries and we wouldn't need to redistribute to other nodes connected to us
                        return;
                    }

                    // We are not a Registry and Received this from a Registry after failure
                    if (!thisNode.IsRegistry() && (sourceIsRegistry || sourceNodeEntry && sourceNodeEntry.IsRegistry())) {
                        // We must have learned from a new Registry; treat like an add
                        topologyPacket.data.LearnedFrom = srcNodeID;
                        if (thisNode.Debug) thisNode.log(`Connected to new Registry, overwriting LearnedFrom for ${topologyPacket.type} [${topologyPacket.id}] from [${targetTable[topologyPacket.id].LearnedFrom}] to [${srcNodeID}]`);
                        targetTable.AddEntry(topologyPacket.id, topologyPacket.data, thisNode.getTimestamp());
                    }
                    return;
                } else {
                    // If this is a Registry receiving a second hand advertisement about another Registry, ignore it
                    if (thisNode.IsRegistry() && topologyPacket.type === "node" && topologyPacket.data.Roles.indexOf("Registry") >= 0 && srcNodeID !== advertisedEntry.NodeID) return;

                    // If this is a Registry and the sender didn't get it from an authoritative source, ignore it
                    if (thisNode.IsRegistry() && topologyPacket.data.NodeID !== thisNode.NodeID && topologyPacket.data.LearnedFrom !== topologyPacket.data.NodeID && topologyPacket.data.LearnedFrom !== topologyPacket.data.ProxyNodeID) {
                        if (thisNode.Debug) thisNode.log(`Ignoring ${topologyPacket.type} table entry [${topologyPacket.id}] from Node [${srcNodeID}], not relayed from an authoritative source`);
                        return;
                    }

                    // If this is a service entry and we don't have a corresponding node table entry, ignore it
                    if (topologyPacket.type === "service" && !thisTopologyTracker.NodeTable[topologyPacket.data.NodeID]) {
                        if (thisNode.Debug) thisTopologyTracker.DRPNode.log(`Ignoring service table entry [${topologyPacket.id}], no matching node table entry`);
                        return;
                    }

                    // We don't have this one; add it and advertise
                    topologyPacket.data.LearnedFrom = srcNodeID;

                    targetTable.AddEntry(topologyPacket.id, topologyPacket.data, thisNode.getTimestamp());
                    topologyEntry = targetTable[topologyPacket.id];
                    if (topologyPacket.type === "service") {
                        //console.dir(topologyEntry);
                    }
                }
                break;
            case "update":
                if (targetTable[topologyPacket.id]) {
                    targetTable.UpdateEntry(topologyPacket.id, topologyPacket.data, thisNode.getTimestamp());
                    topologyEntry = targetTable[topologyPacket.id];
                } else {
                    if (thisNode.Debug) thisTopologyTracker.DRPNode.log(`Could not update non-existent ${topologyPacket.type} entry ${topologyPacket.id}`);
                    return;
                }
                break;
            case "delete":
                // Only delete if we learned the packet from the sourceID or if we are the source (due to disconnect)
                if (topologyPacket.id === thisNode.NodeID && topologyPacket.type === "node") {
                    if (thisNode.Debug) thisNode.log(`This node tried to delete itself.  Why?`);
                    //console.dir(topologyPacket);
                    return;
                }
                // Update this rule so that if the table LearnedFrom is another Registry, do not delete or relay!  We are no longer authoritative
                if (targetTable[topologyPacket.id] && (targetTable[topologyPacket.id].NodeID === srcNodeID || targetTable[topologyPacket.id].LearnedFrom === srcNodeID) || thisNode.NodeID === srcNodeID) {
                    topologyEntry = targetTable[topologyPacket.id];
                    delete targetTable[topologyPacket.id];
                    if (topologyPacket.type === "node") {
                        // Delete services from this node
                        let serviceIDList = Object.keys(thisTopologyTracker.ServiceTable);
                        for (let i = 0; i < serviceIDList.length; i++) {
                            /** @type {DRP_ServiceTableEntry} */
                            let serviceInstanceID = serviceIDList[i];
                            let thisServiceEntry = thisTopologyTracker.ServiceTable[serviceInstanceID];
                            if (thisServiceEntry.NodeID === topologyPacket.id || thisServiceEntry.LearnedFrom === topologyPacket.id) {
                                if (thisNode.Debug) thisNode.log(`Removing entries learned from Node[${topologyPacket.id}] -> Service[${serviceInstanceID}]`);
                                delete thisTopologyTracker.ServiceTable[serviceInstanceID];
                            }
                        }

                        // Remove any dependent Nodes
                        let nodeIDList = Object.keys(thisTopologyTracker.NodeTable);
                        for (let i = 0; i < nodeIDList.length; i++) {
                            let checkNodeID = nodeIDList[i];
                            let checkNodeEntry = thisTopologyTracker.NodeTable[checkNodeID];
                            if (checkNodeEntry.LearnedFrom === topologyPacket.id) {
                                // Delete this entry
                                if (thisNode.Debug) thisNode.log(`Removing entries learned from Node[${topologyPacket.id}] -> Node[${checkNodeEntry.NodeID}]`);
                                let nodeDeletePacket = new DRP_TopologyPacket(checkNodeEntry.NodeID, "delete", "node", checkNodeEntry.NodeID, checkNodeEntry.Scope, checkNodeEntry.Zone, checkNodeEntry);
                                thisTopologyTracker.ProcessPacket(nodeDeletePacket, checkNodeEntry.NodeID);
                            }
                        }
                    }
                } else {
                    // Ignore delete command
                    if (thisNode.Debug) thisNode.log(`Ignoring delete from Node[${srcNodeID}]`);
                    //console.dir(targetTable[topologyPacket.id]);
                    //console.dir(topologyPacket);
                    return;
                }
                break;
            default:
                return;
        }

        // Send to TopicManager
        thisNode.TopicManager.SendToTopic("TopologyTracker", topologyPacket);

        if (thisNode.Debug) thisTopologyTracker.DRPNode.log(`Imported topology packet from [${topologyPacket.originNodeID}] -> ${topologyPacket.cmd} ${topologyPacket.type}[${topologyPacket.id}]`, true);

        if (!topologyEntry) {
            if (thisNode.Debug) thisNode.log(`The topologyEntry is null!  Why?`);
            return;
            //console.dir(topologyPacket);
        }

        // Loop over all connected node endpoints
        let nodeIDList = Object.keys(thisTopologyTracker.DRPNode.NodeEndpoints);
        for (let i = 0; i < nodeIDList.length; i++) {

            // By default, do not relay the packet
            let relayPacket = false;

            // Get endpoint NodeID and object
            let targetNodeID = nodeIDList[i];

            // Check to see if we should relay this packet
            relayPacket = thisTopologyTracker.AdvertiseOutCheck(topologyEntry, targetNodeID);

            if (relayPacket) {
                thisTopologyTracker.DRPNode.NodeEndpoints[targetNodeID].SendCmd("DRP", "topologyUpdate", topologyPacket, false, null);
                if (thisNode.Debug) thisNode.log(`Relayed topology packet to node: [${targetNodeID}]`, true);
            } else {
                if (targetNodeID !== thisNode.NodeID) {
                    //thisNode.log(`Not relaying packet to node[${targetNodeID}], roles ${thisTopologyTracker.NodeTable[targetNodeID].Roles}`);
                    //console.dir(topologyPacket);
                }
            }
        }
    }

    /**
     * @returns {string[]} List of service names
     */
    ListServices() {
        let serviceNameSet = new Set();
        let serviceInstanceList = Object.keys(this.ServiceTable);
        for (let i = 0; i < serviceInstanceList.length; i++) {
            /** @type DRP_ServiceTableEntry */
            let serviceTableEntry = this.ServiceTable[serviceInstanceList[i]];
            serviceNameSet.add(serviceTableEntry.Name);
        }
        let returnList = [...serviceNameSet];
        return returnList;
    }

    /**
     * @returns {Object.<string,{ServiceName:string,Providers:string[]}>} Dictionary of service names and providers
     */
    GetServicesWithProviders() {
        let oReturnObject = {};
        let serviceInstanceList = Object.keys(this.ServiceTable);
        for (let i = 0; i < serviceInstanceList.length; i++) {
            /** @type DRP_ServiceTableEntry */
            let serviceTableEntry = this.ServiceTable[serviceInstanceList[i]];

            if (!oReturnObject[serviceTableEntry.Name]) oReturnObject[serviceTableEntry.Name] = {
                "ServiceName": serviceTableEntry.Name,
                "Providers": []
            };

            oReturnObject[serviceTableEntry.Name].Providers.push(serviceTableEntry.NodeID);
        }
        return oReturnObject;
    }

    /**
     * Return the most preferred instance of a service
     * @param {string} serviceName Name of Service to find
     * @param {string} serviceType Type of Service to find (optional)
     * @param {string} zone Name of zone (optional)
     * @param {string} nodeID Specify Node ID (optional)
     * @returns {DRP_ServiceTableEntry} Best Service Table entry
     */
    FindInstanceOfService(serviceName, serviceType, zone, nodeID) {
        let thisTopologyTracker = this;
        let thisNode = thisTopologyTracker.DRPNode;

        if (serviceName && typeof serviceName === "object" && serviceName.pathList) {
            // This was called from the CLI
            let remainingChildPath = serviceName.pathList;
            if (remainingChildPath && remainingChildPath.length > 0) {
                serviceName = remainingChildPath.shift();
                serviceType = remainingChildPath.shift();
                zone = remainingChildPath.shift();
            } else {
                serviceName = null;
            }
        }

        let checkZone = zone || thisNode.Zone;

        // If neither a name nor a type is specified, return null
        if (!serviceName && !serviceType) return null;

        /*
         * Status MUST be 1 (Ready)
         * Local zone is better than others (if specified, must match)
         * Lower priority is better
         * Higher weight is better
         */

        /** @type {DRP_ServiceTableEntry} */
        let bestServiceEntry = null;
        let candidateList = [];

        let serviceInstanceList = Object.keys(this.ServiceTable);
        for (let i = 0; i < serviceInstanceList.length; i++) {
            /** @type DRP_ServiceTableEntry */
            let serviceTableEntry = this.ServiceTable[serviceInstanceList[i]];

            // Skip if the service isn't ready
            if (serviceTableEntry.Status !== 1) continue;

            // Skip if the service name/type doesn't match
            if (serviceName && serviceName !== serviceTableEntry.Name) continue;
            if (serviceType && serviceType !== serviceTableEntry.Type) continue;

            // Skip if the node ID doesn't match
            if (nodeID && nodeID !== serviceTableEntry.NodeID) continue;

            // If we offer the service locally, select it and continue
            if (serviceTableEntry.NodeID === thisNode.NodeID) {
                return serviceTableEntry;
            }

            // Skip if the zone is specified and doesn't match
            switch (serviceTableEntry.Scope) {
                case "local":
                    break;
                case "global":
                    break;
                case "zone":
                    if (checkZone !== serviceTableEntry.Zone) continue;
                    break;
                default:
                    // Unrecognized scope option
                    continue;
            }

            // Delete if we don't have a corresponding Node entry
            if (!this.NodeTable[serviceTableEntry.NodeID]) {
                thisNode.log(`Deleted service table entry [${serviceTableEntry.InstanceID}], no matching node table entry`);
                delete this.ServiceTable[serviceTableEntry.InstanceID];
                continue;
            }

            // If this is the first candidate, set it and go
            if (!bestServiceEntry) {
                bestServiceEntry = serviceTableEntry;
                candidateList = [bestServiceEntry];
                continue;
            }

            // Check this against the current bestServiceEntry

            // Better zone?
            if (bestServiceEntry && bestServiceEntry.Zone !== checkZone && serviceTableEntry.Zone === checkZone) {
                // The service being evaluated is in a better zone
                bestServiceEntry = serviceTableEntry;
                candidateList = [bestServiceEntry];
                continue;
            } else if (bestServiceEntry && bestServiceEntry.Zone === checkZone && serviceTableEntry.Zone !== checkZone) {
                // The service being evaluated is in a different zone
                continue;
            }

            // Local preference?
            if (bestServiceEntry && bestServiceEntry.Scope !== "local" && serviceTableEntry.Scope === "local") {
                bestServiceEntry = serviceTableEntry;
                candidateList = [bestServiceEntry];
                continue;
            }

            // Lower Priority?
            if (bestServiceEntry.Priority > serviceTableEntry.Priority) {
                bestServiceEntry = serviceTableEntry;
                candidateList = [bestServiceEntry];
                continue;
            }

            // Weighted?
            if (bestServiceEntry.Priority === serviceTableEntry.Priority) {
                candidateList.push(serviceTableEntry);
            }
        }

        // Did we find a match?
        if (candidateList.length === 1) {
            // Single match
        } else if (candidateList.length > 1) {
            // Multiple matches; select based on weight

            // Multiply elements by its weight and create new array
            let weight = function (arr) {
                return [].concat(...arr.map((obj) => Array(obj.Weight).fill(obj)));
            };

            let pick = function (arr) {
                let weighted = weight(arr);
                return weighted[Math.floor(Math.random() * weighted.length)];
            };

            bestServiceEntry = pick(candidateList);
            if (thisNode.Debug) {
                let qualifierText = "";
                if (serviceName) qualifierText = `name[${serviceName}]`;
                if (serviceType) {
                    if (qualifierText.length !== 0) qualifierText = `${qualifierText}/`;
                    qualifierText = `${qualifierText}type[${serviceType}]`;
                }
                thisNode.log(`Need service ${qualifierText}, randomly selected [${bestServiceEntry.InstanceID}]`, true);
            }
        }

        return bestServiceEntry;
    }

    /**
     * Return the most preferred instance of a service
     * @param {string} serviceName Name of Service to find
     * @returns {DRP_ServiceTableEntry[]} List of Service Table entries
     */
    FindInstancesOfService(serviceName) {
        let thisTopologyTracker = this;

        if (serviceName && typeof serviceName === "object" && serviceName.pathList) {
            // This was called from the CLI
            let remainingChildPath = serviceName.pathList;
            if (remainingChildPath && remainingChildPath.length > 0) {
                serviceName = remainingChildPath.shift();
                zone = remainingChildPath.shift();
            } else {
                serviceName = null;
            }
        }

        // If neither a name nor a type is specified, return null
        if (!serviceName) return null;

        /** @type {DRP_ServiceTableEntry[]} */
        let serviceEntryList = [];

        let serviceInstanceList = Object.keys(this.ServiceTable);
        for (let i = 0; i < serviceInstanceList.length; i++) {
            let serviceTableEntry = this.ServiceTable[serviceInstanceList[i]];
            if (serviceTableEntry.Name === serviceName) serviceEntryList.push(serviceTableEntry);
        }

        return serviceEntryList;
    }

    /**
     * Find peers of a specified service instance
     * @param {string} serviceID Service instance to find peers of
     * @returns {string[]} List of peers
     */
    FindServicePeers(serviceID) {
        let thisTopologyTracker = this;
        let peerServiceIDList = [];

        // Get origin service
        let originServiceTableEntry = thisTopologyTracker.ServiceTable[serviceID];
        if (!originServiceTableEntry) return [];

        // A peer is another service with the same name and falls
        // under the specified scope
        let serviceInstanceList = Object.keys(thisTopologyTracker.ServiceTable);
        for (let i = 0; i < serviceInstanceList.length; i++) {
            /** @type DRP_ServiceTableEntry */
            let serviceTableEntry = thisTopologyTracker.ServiceTable[serviceInstanceList[i]];

            // Skip the instance specified
            if (serviceTableEntry.InstanceID === serviceID) continue;

            // Skip if the service isn't ready
            if (serviceTableEntry.Status !== 1) continue;

            // Skip if the service name/type doesn't match
            if (originServiceTableEntry.Name !== serviceTableEntry.Name) continue;
            if (originServiceTableEntry.Type !== serviceTableEntry.Type) continue;

            // Skip if the zone doesn't match
            switch (searchScope) {
                case "local":
                    if (originServiceTableEntry.NodeID !== serviceTableEntry.NodeID) continue;
                    break;
                case "global":
                    break;
                case "zone":
                    if (originServiceTableEntry.Zone !== serviceTableEntry.Zone) continue;
                    break;
                default:
                    // Unrecognized scope option
                    continue;
            }

            // We have a match
            peerServiceIDList.push(serviceTableEntry.InstanceID);
        }

        return peerServiceIDList;
    }

    /**
     * Determine whether or not we should advertise this entry
     * @param {DRP_TrackingTableEntry} topologyEntry Topology entry
     * @param {string} targetNodeID Node we're considering sending to
     * @returns {boolean} Should the item be advertised
     */
    AdvertiseOutCheck(topologyEntry, targetNodeID) {
        let thisTopologyTracker = this;
        let thisNode = thisTopologyTracker.DRPNode;
        let localNodeID = thisNode.NodeID;
        let doSend = false;

        let advertisedNodeID = topologyEntry.NodeID;
        let learnedFromNodeID = topologyEntry.LearnedFrom;
        let proxyNodeID = topologyEntry.ProxyNodeID;
        let advertisedScope = topologyEntry.Scope;
        let advertisedZone = topologyEntry.Zone;

        let localNodeEntry = thisTopologyTracker.NodeTable[localNodeID];
        let targetNodeEntry = thisTopologyTracker.NodeTable[targetNodeID];
        let advertisedNodeEntry = thisTopologyTracker.NodeTable[advertisedNodeID];
        let learnedFromNodeEntry = thisTopologyTracker.NodeTable[learnedFromNodeID];
        let proxyNodeEntry = thisTopologyTracker.NodeTable[proxyNodeID];

        try {

            // We don't recognize the target node; give them everything by default
            if (!targetNodeEntry) return true;

            // TODO - Add logic to support Proxied Nodes

            switch (advertisedScope) {
                case "local":
                    // Never advertise local
                    return false;
                case "zone":
                    // Do not proceed if the target isn't in the same zone
                    if (advertisedZone !== targetNodeEntry.Zone) {
                        //thisNode.log(`Not relaying because Node[${targetNodeID}] is not in the same zone!`);
                        return false;
                    }
                    break;
                case "global":
                    // Global services can be advertised anywhere
                    break;
                default:
                    // Unknown scope type
                    return false;
            }

            // Never send back to the origin
            if (advertisedNodeID === targetNodeID) return false;

            // Always relay locally sourced entries
            if (advertisedNodeID === localNodeID) return true;

            // Only send items for which we are authoritative
            if (localNodeEntry.IsRegistry()) {
                // The local node is a Registry

                /// Relay to connected non-Registry Nodes
                if (targetNodeEntry && !targetNodeEntry.IsRegistry()) return true;

                // Relay if the advertised node was locally connected
                if (topologyEntry.LearnedFrom === topologyEntry.NodeID) return true;

                // Relay if we know the target isn't a Registry
                if (targetNodeEntry && !targetNodeEntry.IsRegistry()) return true;

                // Do not relay
                //console.log(`Not relaying to Node[${targetNodeID}]`);
                //console.dir(topologyEntry);
                return false;
            }
        } catch (ex) {
            thisNode.log(`AdvertiseOutCheck could not evaluate topologyPacket for relay: <<<${ex}>>>`);
            console.dir(topologyEntry);
        }
        return doSend;
    }

    /**
     * Get the local registry
     * @param {string} requestingNodeID Node ID requesting the registry
     * @returns {DRP_TopologyTracker} Returns node & service entries
     */
    GetRegistry(requestingNodeID) {
        let thisTopologyTracker = this;
        let thisNode = thisTopologyTracker.DRPNode;

        let returnNodeTable = {};
        let returnServiceTable = {};

        if (!requestingNodeID) { requestingNodeID = ""; }

        try {
            // Loop over Node Table
            let nodeIDList = Object.keys(thisTopologyTracker.NodeTable);
            for (let i = 0; i < nodeIDList.length; i++) {

                let advertisedNodeID = nodeIDList[i];
                let advertisedNodeEntry = thisTopologyTracker.NodeTable[advertisedNodeID];

                // Check to see if we should relay this packet
                let relayPacket = thisTopologyTracker.AdvertiseOutCheck(advertisedNodeEntry, requestingNodeID);

                if (relayPacket) {
                    returnNodeTable[advertisedNodeID] = advertisedNodeEntry;
                } else {
                    //thisNode.log(`Not relaying to Node[${requestingNodeID}]`);
                    //console.dir(advertisedNodeEntry);
                }
            }
            // Loop over Service Table
            let serviceIDList = Object.keys(thisTopologyTracker.ServiceTable);
            for (let i = 0; i < serviceIDList.length; i++) {

                let advertisedServiceID = serviceIDList[i];
                let advertisedServiceEntry = thisTopologyTracker.ServiceTable[advertisedServiceID];

                // Check to see if we should relay this packet
                let relayPacket = thisTopologyTracker.AdvertiseOutCheck(advertisedServiceEntry, requestingNodeID);

                if (relayPacket) {
                    returnServiceTable[advertisedServiceID] = advertisedServiceEntry;
                }
            }
        } catch (ex) {
            thisTopologyTracker.DRPNode.log(`Exception while getting subset of Registry: ${ex}`);
        }

        let returnObj = {
            NodeTable: returnNodeTable,
            ServiceTable: returnServiceTable
        };
        return returnObj;
    }

    /**
     * 
     * @param {DRP_Endpoint} sourceEndpoint Source Endopint
     * @param {DRP_NodeDeclaration} declaration Source Declaration
     * @param {boolean} localNodeIsProxy Is the local Node a proxy for the remote Node?
     */
    async ProcessNodeConnect(sourceEndpoint, declaration, localNodeIsProxy) {
        let thisTopologyTracker = this;
        let thisNode = thisTopologyTracker.DRPNode;
        thisNode.log(`Connection established with Node [${declaration.NodeID}] (${declaration.NodeRoles})`);
        let returnData = await sourceEndpoint.SendCmd("DRP", "getRegistry", { "reqNodeID": thisNode.NodeID }, true, null, null);
        //console.dir(returnData, { depth: 4 });
        let sourceIsRegistry = false;
        let remoteRegistry = returnData.payload;
        let runCleanup = false;

        if (declaration.NodeRoles.indexOf("Registry") >= 0) {
            sourceIsRegistry = true;
        }

        // Import Nodes
        let nodeIDList = Object.keys(remoteRegistry.NodeTable);
        for (let i = 0; i < nodeIDList.length; i++) {
            /** @type {DRP_NodeTableEntry} */
            let thisNodeEntry = remoteRegistry.NodeTable[nodeIDList[i]];
            if (localNodeIsProxy) thisNodeEntry.ProxyNodeID = thisNode.NodeID;
            let nodeAddPacket = new DRP_TopologyPacket(declaration.NodeID, "add", "node", thisNodeEntry.NodeID, thisNodeEntry.Scope, thisNodeEntry.Zone, thisNodeEntry);
            thisNode.TopologyTracker.ProcessPacket(nodeAddPacket, sourceEndpoint.EndpointID, sourceIsRegistry);
        }

        // Import Services
        let serviceIDList = Object.keys(remoteRegistry.ServiceTable);
        for (let i = 0; i < serviceIDList.length; i++) {
            /** @type {DRP_ServiceTableEntry} */
            let thisServiceEntry = remoteRegistry.ServiceTable[serviceIDList[i]];
            if (localNodeIsProxy) thisServiceEntry.ProxyNodeID = thisNode.NodeID;
            let serviceAddPacket = new DRP_TopologyPacket(declaration.NodeID, "add", "service", thisServiceEntry.InstanceID, thisServiceEntry.Scope, thisServiceEntry.Zone, thisServiceEntry);
            thisNode.TopologyTracker.ProcessPacket(serviceAddPacket, sourceEndpoint.EndpointID, sourceIsRegistry);
        }

        // Execute onControlPlaneConnect callback
        if (!thisNode.IsRegistry() && sourceIsRegistry && !thisNode.ConnectedToControlPlane) {
            // We are connected to a Registry
            thisNode.ConnectedToControlPlane = true;
            runCleanup = true;
            if (thisNode.onControlPlaneConnect && typeof thisNode.onControlPlaneConnect === "function" && !thisNode.HasConnectedToMesh) thisNode.onControlPlaneConnect();
            thisNode.HasConnectedToMesh = true;
        }

        // Remove any stale entries if we're reconnecting to a new Registry
        if (runCleanup) thisNode.TopologyTracker.StaleEntryCleanup();
    }

    ProcessNodeDisconnect(disconnectedNodeID) {
        let thisTopologyTracker = this;
        let thisNode = thisTopologyTracker.DRPNode;
        // Remove node; this should trigger an autoremoval of entries learned from it

        // If we are not a Registry and we just disconnected from a Registry, hold off on this process!
        let thisNodeEntry = thisTopologyTracker.NodeTable[thisNode.NodeID];
        let disconnectedNodeEntry = thisTopologyTracker.NodeTable[disconnectedNodeID];

        if (!disconnectedNodeEntry) {
            thisNode.log(`Ran ProcessNodeDisconnect on non-existent Node [${disconnectedNodeID}]`);
            return;
        }

        thisNode.log(`Connection terminated with Node [${disconnectedNodeEntry.NodeID}] (${disconnectedNodeEntry.Roles})`);

        // If both the local and remote are non-Registry nodes, skip further processing.  May just be a direct connection timing out.
        if (!thisNodeEntry.IsRegistry() && !disconnectedNodeEntry.IsRegistry()) return;

        // See if we're connected to other Registry Nodes
        let hasAnotherRegistryConnection = thisTopologyTracker.ListConnectedRegistryNodes().length > 0;

        // Do we need to hold off on purging the Registry?
        if (!thisNodeEntry.IsRegistry() && disconnectedNodeEntry && disconnectedNodeEntry.IsRegistry() && !hasAnotherRegistryConnection) {
            // Do not go through with the cleanup process; delete only the disconnected Registry node
            // for now and we'll run the StaleEntryCleanup when we connect to the next Registry.
            thisNode.log(`We disconnected from Registry Node[${disconnectedNodeID}] and have no other Registry connections`);
            delete thisTopologyTracker.NodeTable[disconnectedNodeID];
            thisNode.ConnectedToControlPlane = false;
            return;
        }

        let nodeIDList = Object.keys(thisTopologyTracker.NodeTable);
        for (let i = 0; i < nodeIDList.length; i++) {
            /** @type {DRP_NodeTableEntry} */
            let checkNodeEntry = thisTopologyTracker.NodeTable[nodeIDList[i]];
            if (checkNodeEntry) {
                if (checkNodeEntry.NodeID === disconnectedNodeID || checkNodeEntry.LearnedFrom === disconnectedNodeID) {
                    let nodeDeletePacket = new DRP_TopologyPacket(checkNodeEntry.NodeID, "delete", "node", checkNodeEntry.NodeID, checkNodeEntry.Scope, checkNodeEntry.Zone, checkNodeEntry);
                    thisTopologyTracker.ProcessPacket(nodeDeletePacket, checkNodeEntry.NodeID);
                }
            } else {
                // Node has already been removed; maybe dupe delete commands
                if (thisNode.Debug) thisNode.log(`ProcessNodeDisconnect: Node[${disconnectedNodeID}] has already been removed`);
            }
        }

        if (thisNode.ConnectedToControlPlane) thisNode.TopologyTracker.StaleEntryCleanup();
    }

    /**
     * Get next hop for relaying a command
     * @param {string} targetNodeID Node ID we're ultimately trying to reach
     * @returns {string} Node ID of next hop
     */
    GetNextHop(targetNodeID) {
        let thisTopologyTracker = this;
        if (targetNodeID && typeof targetNodeID === "object" && targetNodeID.pathList) {
            // This was called from the CLI
            let remainingChildPath = targetNodeID.pathList;
            if (remainingChildPath && remainingChildPath.length > 0) {
                targetNodeID = remainingChildPath.shift();
            } else {
                targetNodeID = null;
            }
        }
        let nextHopNodeID = null;
        let targetNodeEntry = thisTopologyTracker.NodeTable[targetNodeID];
        if (targetNodeEntry) nextHopNodeID = targetNodeEntry.LearnedFrom;
        return nextHopNodeID;
    }

    /**
     * Validate Node ID
     * @param {string} checkNodeID Node ID we're trying to validate
     * @returns {boolean} Node ID of next hop
     */
    ValidateNodeID(checkNodeID) {
        let thisTopologyTracker = this;
        if (checkNodeID && typeof checkNodeID === "object" && checkNodeID.pathList) {
            // This was called from the CLI
            let remainingChildPath = checkNodeID.pathList;
            if (remainingChildPath && remainingChildPath.length > 0) {
                checkNodeID = remainingChildPath.shift();
            } else {
                checkNodeID = null;
            }
        }
        if (thisTopologyTracker.NodeTable[checkNodeID])
            return true;
        else
            return false;
    }

    GetNodeWithURL(checkNodeURL) {
        let thisTopologyTracker = this;
        let nodeIDList = Object.keys(thisTopologyTracker.NodeTable);
        for (let i = 0; i < nodeIDList.length; i++) {
            let checkNodeID = nodeIDList[i];
            let thisNodeEntry = thisTopologyTracker.NodeTable[checkNodeID];
            if (thisNodeEntry.NodeURL && thisNodeEntry.NodeURL === checkNodeURL) return checkNodeID;
        }
        return null;
    }

    StaleEntryCleanup() {
        let thisTopologyTracker = this;

        // Purge Node entries where the LearnedFrom Node is not present
        let nodeIDList = Object.keys(thisTopologyTracker.NodeTable);
        for (let i = 0; i < nodeIDList.length; i++) {
            let checkNodeID = nodeIDList[i];
            let checkNodeEntry = thisTopologyTracker.NodeTable[checkNodeID];
            if (!thisTopologyTracker.ValidateNodeID(checkNodeEntry.LearnedFrom)) {
                // Stale record - delete
                thisTopologyTracker.DRPNode.log(`Purged stale Node [${checkNodeID}], LearnedFrom Node [${checkNodeEntry.LearnedFrom}] not in Node table`);
                delete thisTopologyTracker.NodeTable[checkNodeID];
            }
        }

        // Purge Service entries where the Node is not present
        let serviceIDList = Object.keys(thisTopologyTracker.ServiceTable);
        for (let i = 0; i < serviceIDList.length; i++) {
            let checkServiceID = serviceIDList[i];
            let checkServiceEntry = thisTopologyTracker.ServiceTable[checkServiceID];
            if (!thisTopologyTracker.ValidateNodeID(checkServiceEntry.NodeID)) {
                // Stale record - delete
                thisTopologyTracker.DRPNode.log(`Purged stale Service [${checkServiceID}], LearnedFrom Node [${checkServiceEntry.LearnedFrom}] not in Node table`);
                delete thisTopologyTracker.ServiceTable[checkServiceID];
            }
        }
        return null;
    }

    ListConnectedRegistryNodes() {
        let thisTopologyTracker = this;
        let connectedRegistryList = [];
        // Look for entries with the Registry role and that are still connected
        let nodeIDList = Object.keys(thisTopologyTracker.NodeTable);
        for (let i = 0; i < nodeIDList.length; i++) {
            let thisNodeID = thisTopologyTracker.DRPNode.NodeID;
            let checkNodeID = nodeIDList[i];
            let checkNodeEntry = thisTopologyTracker.NodeTable[checkNodeID];
            if (checkNodeEntry.NodeID !== thisNodeID && checkNodeEntry.IsRegistry() && thisTopologyTracker.DRPNode.NodeEndpoints[checkNodeID]) {
                // Remote Node is a Registry and we are connected to it
                connectedRegistryList.push[checkNodeID];
            }
        }
        return connectedRegistryList;
    }

    FindRegistriesInZone(zoneName) {
        let thisTopologyTracker = this;
        if (zoneName && typeof zoneName === "object" && zoneName.pathList) {
            // This was called from the CLI
            let remainingChildPath = zoneName.pathList;
            if (remainingChildPath && remainingChildPath.length > 0) {
                zoneName = remainingChildPath.shift();
            } else {
                zoneName = null;
            }
        }
        let registryList = [];
        let nodeIDList = Object.keys(thisTopologyTracker.NodeTable);
        for (let i = 0; i < nodeIDList.length; i++) {
            let checkNodeEntry = thisTopologyTracker.NodeTable[nodeIDList[i]];
            if (checkNodeEntry.IsRegistry() && checkNodeEntry.Zone === zoneName) {
                registryList.push(checkNodeEntry.NodeURL);
            }
        }
        return registryList;
    }
}

class DRP_TrackingTableEntry {
    /**
    * 
    * @param {string} nodeID Node ID
    * @param {string} proxyNodeID Proxy Node ID
    * @param {string} scope Node Scope
    * @param {string} zone Node Zone
    * @param {string} learnedFrom NodeID that sent us this record
    * @param {string} lastModified Last Modified Timestamp
    */
    constructor(nodeID, proxyNodeID, scope, zone, learnedFrom, lastModified) {
        this.NodeID = nodeID;
        this.ProxyNodeID = proxyNodeID;
        this.Scope = scope || "zone";
        this.Zone = zone;
        this.LearnedFrom = learnedFrom;
        this.LastModified = lastModified;
    }
}

class DRP_NodeTable {
    /**
     * Add Node Table Entry
     * @param {string} entryID New table entry ID
     * @param {DRP_NodeTableEntry} entryData New table entry data
     * @param {string} lastModified Last Modified Timestamp
     */
    AddEntry(entryID, entryData, lastModified) {
        let thisTable = this;
        let newTableRecord = new DRP_NodeTableEntry();
        Object.assign(newTableRecord, entryData);
        thisTable[entryID] = newTableRecord;
        if (lastModified) thisTable[entryID].LastModified = lastModified;
    }

    UpdateEntry(entryID, updateData, lastModified) {
        let thisTable = this;
        Object.assign(thisTable[entryID], updateData);
        if (lastModified) thisTable[entryID].LastModified = lastModified;
    }
}

// Details of Node
class DRP_NodeTableEntry extends DRP_TrackingTableEntry {
    /**
     * 
     * @param {string} nodeID Node ID
     * @param {string} proxyNodeID Proxy Node ID
     * @param {string[]} roles Roles
     * @param {string} nodeURL Node URL
     * @param {string} scope Node Scope
     * @param {string} zone Node Zone
     * @param {string} learnedFrom NodeID that sent us this record
     * @param {string} hostID Host ID
     * @param {string} lastModified Last Modified Timestamp
     */
    constructor(nodeID, proxyNodeID, roles, nodeURL, scope, zone, learnedFrom, hostID, lastModified) {
        super(nodeID, proxyNodeID, scope, zone, learnedFrom, lastModified);
        this.Roles = roles;
        this.NodeURL = nodeURL;
        this.HostID = hostID;
    }

    IsRegistry() {
        let thisNodeTableEntry = this;
        let isRegistry = thisNodeTableEntry.Roles.indexOf("Registry") >= 0;
        return isRegistry;
    }

    IsBroker() {
        let thisNodeTableEntry = this;
        let isBroker = thisNodeTableEntry.Roles.indexOf("Broker") >= 0;
        return isBroker;
    }

    UsesProxy(checkNodeID) {
        let thisNodeTableEntry = this;
        let usesProxy = false;

        if (!checkNodeID && thisNodeTableEntry.ProxyNodeID) {
            usesProxy = true;
        }

        if (checkNodeID && thisNodeTableEntry.ProxyNodeID && thisNodeTableEntry.ProxyNodeID === checkNodeID) {
            usesProxy = true;
        }

        return usesProxy;
    }
}

class DRP_ServiceTable {
    /**
     * Add Service Table Entry
     * @param {string} entryID New table entry ID
     * @param {DRP_ServiceTableEntry} entryData New table entry data
     * @param {string} lastModified Last Modified Timestamp
     */
    AddEntry(entryID, entryData, lastModified) {
        let thisTable = this;
        let newTableRecord = new DRP_ServiceTableEntry();
        Object.assign(newTableRecord, entryData);
        thisTable[entryID] = newTableRecord;
        if (lastModified) thisTable[entryID].LastModified = lastModified;
    }

    UpdateEntry(entryID, updateData, lastModified) {
        let thisTable = this;
        Object.assign(thisTable[entryID], updateData);
        if (lastModified) thisTable[entryID].LastModified = lastModified;
    }
}

class DRP_ServiceTableEntry extends DRP_TrackingTableEntry {
    /**
     * 
     * @param {string} nodeID Origin Node ID
     * @param {string} proxyNodeID Proxy Node ID
     * @param {string} serviceName Name of service
     * @param {string} serviceType Type of service
     * @param {string} instanceID Global Instance ID
     * @param {string} zone Service Zone
     * @param {boolean} serviceSticky Service sticky
     * @param {number} servicePriority Service priority
     * @param {number} serviceWeight Service weight
     * @param {string} scope Service scope (Local|Zone|Global)
     * @param {string[]} serviceDependencies Services required for this one to operate
     * @param {string[]} streams Streams provided by this service
     * @param {number} serviceStatus Service status (0 down|1 up|2 pending)
     * @param {string} learnedFrom NodeID that sent us this record
     * @param {string} lastModified Last Modified Timestamp
     */
    constructor(nodeID, proxyNodeID, serviceName, serviceType, instanceID, zone, serviceSticky, servicePriority, serviceWeight, scope, serviceDependencies, streams, serviceStatus, learnedFrom, lastModified) {
        super(nodeID, proxyNodeID, scope, zone, learnedFrom, lastModified);
        this.Name = serviceName;
        this.Type = serviceType;
        this.InstanceID = instanceID;
        this.Sticky = serviceSticky;
        this.Priority = servicePriority;
        this.Weight = serviceWeight;
        this.Dependencies = serviceDependencies || [];
        this.Streams = streams || [];
        this.Status = serviceStatus;
    }
}

class DRP_TopologyPacket {
    /**
     * 
     * @param {string} originNodeID Source Node ID
     * @param {string} cmd Command [Add|Update|Delete]
     * @param {string} type Object Type [Node|Service]
     * @param {string} id Object ID
     * @param {string} scope Object Scope [Zone|Global]
     * @param {string} zone Object Zone Name [MyZone1,MyZone2,etc]
     * @param {object} data Data
     */
    constructor(originNodeID, cmd, type, id, scope, zone, data) {
        this.originNodeID = originNodeID;
        this.cmd = cmd;
        this.type = type;
        this.id = id;
        this.scope = scope;
        this.zone = zone;
        this.data = data;
    }
}

class DRP_RemoteSubscription extends DRP_SubscribableSource {
    /**
     * 
     * @param {string} targetNodeID Target Node ID
     * @param {string} topicName Topic Name
     * @param {string} streamToken Stream Token
     * @param {function} noSubscribersCallback Last subscriber disconnected callback
     */
    constructor(targetNodeID, topicName, streamToken, noSubscribersCallback) {
        super(targetNodeID, topicName);
        this.StreamToken = streamToken;
        this.NoSubscribersCallback = noSubscribersCallback;
    }

    /**
     *
     * @param {DRP_Subscriber} subscription Subscription to add
     */
    RemoveSubscription(subscription) {
        super.RemoveSubscription(subscription);

        // If there are no more subscribers, terminate this RemoteSubscription
        if (this.Subscriptions.size === 0) {
            if (this.NoSubscribersCallback && typeof this.NoSubscribersCallback === "function") this.NoSubscribersCallback();
        }
    }
}

class DRP_SubscriptionManager {
    /**
     * Tracks subscriptions to other Nodes.  Primary purpose is for stream deduplication on Brokers.
     * @param {DRP_Node} drpNode DRP Node
     */
    constructor(drpNode) {
        this.DRPNode = drpNode;
        /** @type Set<DRP_Subscriber> */
        this.Subscribers = new Set();

        /** @type Object.<string, DRP_RemoteSubscription> */
        this.RemoteSubscriptions = {};

        this.DRPNode.TopicManager.SubscribeToTopic(new DRP_Subscriber("TopologyTracker", null, null, null, (topologyPacket) => { this.ProcessTopologyPacket(topologyPacket.Message); }, null));
    }

    /**
     * Analyse topology changes; look for new services to subscribe to
     * @param {DRP_TopologyPacket} topologyPacket Topology packet
     */
    async ProcessTopologyPacket(topologyPacket) {
        let thisSubMgr = this;
        let thisNode = thisSubMgr.DRPNode;

        // Ignore if we don't have any Subscriptions to check against
        if (thisSubMgr.Subscribers.size === 0) return;

        // This is a service add
        if (topologyPacket.cmd === "add" && topologyPacket.type === "service") {

            // Get topologyData
            /** @type {DRP_ServiceTableEntry} */
            let serviceEntry = topologyPacket.data;

            for (let subscriber of thisSubMgr.Subscribers) {
                // Does the newly discovered service provide what this subscription is asking for?
                if (!thisSubMgr.EvaluateServiceTableEntry(serviceEntry, subscriber.topicName, subscriber.scope, thisNode.Zone)) continue;
                await this.RegisterSubscriberWithTargetSource(serviceEntry, subscriber);
            }
        }

        // This is a node delete
        if (topologyPacket.cmd === "delete" && topologyPacket.type === "node") {

            /** @type {DRP_NodeTableEntry} */
            let nodeEntry = topologyPacket.data;
            let checkNodeID = nodeEntry.NodeID;

            // Loop over all Subscribers and objects they're subscribed to
            let remoteSubscriptionIDList = Object.keys(thisSubMgr.RemoteSubscriptions);
            for (let i = 0; i < remoteSubscriptionIDList.length; i++) {
                let remoteSubscriptionID = remoteSubscriptionIDList[i];
                let thisRemoteSub = thisSubMgr.RemoteSubscriptions[remoteSubscriptionID];
                if (checkNodeID !== thisRemoteSub.NodeID) continue;
                for (let thisLocalSub of thisRemoteSub.Subscriptions) {
                    // This will remove the local sub from the remote and vice versa
                    thisRemoteSub.RemoveSubscription(thisLocalSub);
                }
                delete thisSubMgr.RemoteSubscriptions[remoteSubscriptionID];
            }
        }
    }

    /**
     * 
     * @param {DRP_Subscriber} subscriber Subscription
     * @returns {boolean} Registration success
     */
    async RegisterSubscription(subscriber) {
        let thisSubMgr = this;
        let thisNode = thisSubMgr.DRPNode;

        thisSubMgr.Subscribers.add(subscriber);

        if (!subscriber.targetNodeID && subscriber.scope === "local") {
            subscriber.targetNodeID = thisNode.NodeID;
        }

        if (subscriber.targetNodeID) {
            if (!thisNode.TopologyTracker.NodeTable[subscriber.targetNodeID]) {
                // We don't recognize the targetNodeID
                //thisNode.log(`Subscriber provided an invalid targetNodeID for a subscription: [${subscriber.targetNodeID}]`, true);
                return false;
            }
            // The subscriber has specified a targetNodeID that is in the Node Table
            await this.RegisterSubscriberWithTargetSource({ NodeID: subscriber.targetNodeID }, subscriber);
            //thisNode.log(`Subscriber successfully registered a targetNodeID subscription: [${subscriber.targetNodeID}]`, true);
            return true;
        } else {

            // We need to evaluate the service table, see if anyone provides the stream this subscriber is requesting
            let serviceTable = thisSubMgr.DRPNode.TopologyTracker.ServiceTable;
            let serviceEntryIDList = Object.keys(serviceTable);
            for (let i = 0; i < serviceEntryIDList.length; i++) {
                let serviceEntry = serviceTable[serviceEntryIDList[i]];

                // Does the service provide what this subscription is asking for?
                if (!thisSubMgr.EvaluateServiceTableEntry(serviceEntry, subscriber.topicName, subscriber.scope, thisNode.Zone)) continue;
                await this.RegisterSubscriberWithTargetSource(serviceEntry, subscriber);
            }
            return true;
        }
    }

    /**
     * 
     * @param {DRP_ServiceTableEntry} serviceEntry Service to check
     * @param {string} streamName Stream Name
     * @param {string} subscriptionScope Subscription Scope
     * @param {string} subscriptionZone Subscription Zone
     * @returns {boolean} Successful Match
     */
    EvaluateServiceTableEntry(serviceEntry, streamName, subscriptionScope, subscriptionZone) {
        // Return false if the service doesn't provide the topic
        if (serviceEntry.Streams.indexOf(streamName) < 0) return false;

        // Return false if the service scope is local and isn't on this node
        if (serviceEntry.Scope === "local" && serviceEntry.NodeID !== this.DRPNode.NodeID) return false;

        // Return false if we're looking in a specific zone and it doesn't match
        if (subscriptionScope === "zone" && subscriptionZone !== serviceEntry.Zone) return false;

        // Must be good
        return true;
    }

    async RegisterSubscriberWithTargetSource(serviceEntry, subscriber) {
        let thisSubMgr = this;
        let thisNode = thisSubMgr.DRPNode;

        /** @type {DRP_SubscribableSource} */
        let targetSource = null;

        // Is this local or remote?
        if (serviceEntry.NodeID === thisNode.NodeID) {
            targetSource = thisNode.TopicManager.GetTopic(subscriber.topicName);
        } else {
            // Verify we have a RemoteSubscription
            targetSource = await thisSubMgr.VerifyRemoteSubscription(serviceEntry.NodeID, subscriber.topicName);
        }

        // Are we aready subscribed?
        if (targetSource) {
            if (targetSource.Subscriptions.has(subscriber)) return;

            // Subscribe
            targetSource.AddSubscription(subscriber);
        }
    }

    /**
     * 
     * @param {string} targetNodeID Target Node ID
     * @param {string} topicName Topic Name
     * @returns {DRP_RemoteSubscription} Remote Subscription
     */
    async VerifyRemoteSubscription(targetNodeID, topicName) {
        let thisSubMgr = this;
        let returnSubscription = null;
        let remoteSubscriptionID = `${targetNodeID}-${topicName}`;
        if (!thisSubMgr.RemoteSubscriptions[remoteSubscriptionID]) {
            let newRemoteSubscription = new DRP_RemoteSubscription(targetNodeID, topicName, null, () => {
                // No Subscribers Callback
                delete thisSubMgr.RemoteSubscriptions[remoteSubscriptionID];
                thisSubMgr.DRPNode.UnsubscribeRemote(newRemoteSubscription.NodeID, newRemoteSubscription.StreamToken);
            });
            thisSubMgr.RemoteSubscriptions[remoteSubscriptionID] = newRemoteSubscription;
            let streamToken = await thisSubMgr.DRPNode.SubscribeRemote(targetNodeID, topicName, (streamPacket) => {
                // TODO - use streamPacket.status to see if this is the last packet?

                // If we're relaying a message from a topic, add the local NodeID to the route
                if (streamPacket.payload && streamPacket.payload.Route) streamPacket.payload.Route.push(thisSubMgr.DRPNode.NodeID);

                newRemoteSubscription.Send(streamPacket.payload);
            });
            if (streamToken) {
                returnSubscription = thisSubMgr.RemoteSubscriptions[remoteSubscriptionID];
                returnSubscription.StreamToken = streamToken;
            } else delete thisSubMgr.RemoteSubscriptions[remoteSubscriptionID];
        } else {
            returnSubscription = thisSubMgr.RemoteSubscriptions[remoteSubscriptionID];
        }
        return returnSubscription;
    }
}

module.exports = DRP_Node;