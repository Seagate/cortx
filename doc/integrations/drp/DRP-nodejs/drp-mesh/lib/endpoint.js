'use strict';

// Had to remove this so we don't have a circular eval problem
//const DRP_Node = require('./node');
//const DRP_SubscribableSource = require('./subscription').DRP_SubscribableSource;
const DRP_Subscriber = require('./subscription').DRP_Subscriber;
const { DRP_Packet, DRP_Cmd, DRP_Reply, DRP_RouteOptions } = require('./packet');

const WebSocket = require('ws');

class DRP_Endpoint {
    /**
     * 
     * @param {Websocket} wsConn Websocket connection
     * @param {DRP_Node} drpNode DRP Node
     * @param {string} endpointID Remote Endpoint ID
     * @param {string} endpointType Remote Endpoint Type
     */
    constructor(wsConn, drpNode, endpointID, endpointType) {
        let thisEndpoint = this;
        /** @type {WebSocket} */
        this.wsConn = wsConn || null;
        /** @type {DRP_Node} */
        this.DRPNode = drpNode;
        if (this.wsConn) {
            this.wsConn.drpEndpoint = this;
        }
        this.EndpointID = endpointID || null;
        this.EndpointType = endpointType || null;
        this.EndpointCmds = {};

        this.AuthInfo = {
            type: null,
            value: null,
            userInfo: null
        };

        /** @type Object<number,function> */
        this.ReplyHandlerQueue = {};

        this.TokenNum = 1;

        /** @type {Object.<string,DRP_Subscriber>} */
        this.Subscriptions = {};
        /** @type {function} */
        this.openCallback;
        /** @type {function} */
        this.closeCallback;
        this.RegisterMethod("getCmds", "GetCmds");

        this.RemoteAddress = this.RemoteAddress;
        this.RemotePort = this.RemotePort;
        this.RemoteFamily = this.RemoteFamily;
    }

    GetToken() {
        let token = this.TokenNum;
        this.TokenNum++;
        return token;
    }

    AddReplyHandler(callback) {
        let token = this.GetToken();
        this.ReplyHandlerQueue[token] = callback;
        return token;
    }

    DeleteReplyHandler(token) {
        delete this.ReplyHandlerQueue[token];
    }

    /**
     * Register Endpoint Command
     * @param {string} methodName Method Name
     * @param {function(Object.<string,object>, DRP_Endpoint, string)} method Function
     */
    RegisterMethod(methodName, method) {
        let thisEndpoint = this;
        // Need to do sanity checks; is the method actually a method?
        if (typeof method === 'function') {
            thisEndpoint.EndpointCmds[methodName] = method;
        } else if (typeof thisEndpoint[method] === 'function') {
            thisEndpoint.EndpointCmds[methodName] = function (...args) {
                return thisEndpoint[method](...args);
            };
        } else {
            thisEndpoint.log("Cannot add EndpointCmds[" + methodName + "]" + " -> sourceObj[" + method + "] of type " + typeof thisEndpoint[method]);
        }
    }

    /**
     * Send serialized packet data
     * @param {string} drpPacketString JSON string
     * @returns {number} Error code (0 good, 1 wsConn not open, 2 send error)
     */
    SendPacketString(drpPacketString) {
        let thisEndpoint = this;

        if (thisEndpoint.wsConn.readyState !== WebSocket.OPEN)
            //return "wsConn not OPEN";
            return 1;
        try {
            thisEndpoint.wsConn.send(drpPacketString);
            return 0;
        } catch (e) {
            //return e;
            return 2;
        }
    }

    /**
     * 
     * @param {string} serviceName DRP Service Name
     * @param {string} method Service Method
     * @param {Object} params Method Parameters
     * @param {boolean} promisify Should we promisify?
     * @param {function} callback Callback function
     * @param {DRP_RouteOptions} routeOptions Route Options
     * @param {string} serviceInstanceID Execute on specific Service Instance ID
     * @return {Promise} Returned promise
     */
    SendCmd(serviceName, method, params, promisify, callback, routeOptions, serviceInstanceID) {
        let thisEndpoint = this;
        let returnVal = null;
        let token = null;

        if (promisify) {
            // We expect a response, using await; add 'resolve' to queue
            returnVal = new Promise(function (resolve, reject) {
                token = thisEndpoint.AddReplyHandler(function (message) {
                    resolve(message);
                });
            });
        } else if (typeof callback === 'function') {
            // We expect a response, using callback; add callback to queue
            token = thisEndpoint.AddReplyHandler(callback);
        } else {
            // We don't expect a response; leave reply token null
        }
        let packetObj = new DRP_Cmd(serviceName, method, params, token, routeOptions, serviceInstanceID);
        //console.dir(packetObj);
        let packetString = JSON.stringify(packetObj);
        thisEndpoint.SendPacketString(packetString);
        return returnVal;
    }

    /**
     * Send reply to received command
     * @param {string} token Reply token
     * @param {number} status Reply status [0: fail, 1: success, 2: more data coming]
     * @param {any} payload Payload to send
     * @param {DRP_RouteOptions} routeOptions Route options
     * @returns {number} Error string
     */
    SendReply(token, status, payload, routeOptions) {
        let thisEndpoint = this;
        let packetString = null;
        let packetObj = null;

        try {
            packetObj = new DRP_Reply(token, status, payload, routeOptions);
            packetString = JSON.stringify(packetObj);
        } catch (e) {
            packetObj = new DRP_Reply(token, 0, `Failed to stringify response: ${e}`);
            packetString = JSON.stringify(packetObj);
        }
        return thisEndpoint.SendPacketString(packetString);
    }

    /**
     * Process inbound DRP Command
     * @param {DRP_Cmd} cmdPacket DRP Command
     */
    async ProcessCmd(cmdPacket) {
        let thisEndpoint = this;

        var cmdResults = {
            status: 0,
            output: null
        };

        //console.dir(cmdPacket);

        // Make sure params is an Object
        if (!cmdPacket.params || typeof cmdPacket.params !== 'object') cmdPacket.params = {};

        // Override AuthInfo if the remote end is a Consumer
        if (thisEndpoint.AuthInfo && (thisEndpoint.AuthInfo.type === "token" || thisEndpoint.AuthInfo.type === "key")) cmdPacket.params.AuthInfo = thisEndpoint.AuthInfo;

        // Execute method
        try {
            cmdResults.output = await thisEndpoint.DRPNode.ServiceCmd(cmdPacket.serviceName, cmdPacket.method, cmdPacket.params, null, cmdPacket.serviceInstanceID, false, true, thisEndpoint);
            cmdResults.status = 1;
        } catch (err) {
            cmdResults.output = err.message;
        }

        // Reply with results
        if (typeof cmdPacket.token !== "undefined" && cmdPacket.token !== null) {
            let routeOptions = null;
            if (cmdPacket.routeOptions && cmdPacket.routeOptions.tgtNodeID === thisEndpoint.DRPNode.NodeID) {
                routeOptions = new DRP_RouteOptions(thisEndpoint.DRPNode.NodeID, cmdPacket.routeOptions.srcNodeID);
            }
            thisEndpoint.SendReply(cmdPacket.token, cmdResults.status, cmdResults.output, routeOptions);
        }

        //console.dir(cmdResults);
    }

    /**
    * Process inbound DRP Reply
    * @param {DRP_Reply} replyPacket DRP Reply Packet
    */
    async ProcessReply(replyPacket) {
        let thisEndpoint = this;

        //console.dir(replyPacket, { "depth": 10 });

        // Yes - do we have the token?
        if (thisEndpoint.ReplyHandlerQueue.hasOwnProperty(replyPacket.token)) {

            // We have the token - execute the reply callback
            thisEndpoint.ReplyHandlerQueue[replyPacket.token](replyPacket);

            // Delete if the status < 2
            if (!replyPacket.status || replyPacket.status < 2) {
                delete thisEndpoint.ReplyHandlerQueue[replyPacket.token];
            }

        } else {
            // We do not have the token - tell the sender we do not honor this token
        }
    }

    /**
    * Check whether or not to relay the packet
    * @param {DRP_Packet} drpPacket DRP Packet
    * @returns {boolean} Should the packet be relayed?
    */
    ShouldRelay(drpPacket) {
        let thisEndpoint = this;
        /*
         * In order to be relayed, a packet should:
         *   - Have route options
         *   - Specify a tgtNodeID that is not the local Node
         *   - Come from an endpoint that has successfully peered as a Node
         */
        if (drpPacket.routeOptions && drpPacket.routeOptions.tgtNodeID && drpPacket.routeOptions.tgtNodeID !== thisEndpoint.DRPNode.NodeID && thisEndpoint.EndpointType && thisEndpoint.EndpointType === "Node")
            return true;
        else
            return false;
    }

    async ReceiveMessage(rawMessage) {
        let thisEndpoint = this;
        /** @type {DRP_Packet} */
        let drpPacket;
        try {
            drpPacket = JSON.parse(rawMessage);
        } catch (e) {
            thisEndpoint.log(`Received non-JSON message, disconnecting client endpoint[${thisEndpoint.EndpointID}] @ ${thisEndpoint.wsConn._socket.remoteAddress}`);
            thisEndpoint.log(rawMessage);
            thisEndpoint.wsConn.close();
            return;
        }

        // Should we relay the packet?
        if (thisEndpoint.ShouldRelay(drpPacket)) {
            // This is meant for another node
            thisEndpoint.RelayPacket(drpPacket);
            return;
        }

        // Process locally
        switch (drpPacket.type) {
            case 'cmd':
                thisEndpoint.ProcessCmd(drpPacket);
                break;
            case 'reply':
                thisEndpoint.ProcessReply(drpPacket);
                break;
            default:
                thisEndpoint.log("Invalid message.type; here's the JSON data..." + rawMessage);
        }
    }

    /**
     * Relay DRP Packet
     * @param {DRP_Packet} drpPacket Packet to relay
     */
    async RelayPacket(drpPacket) {
        let thisEndpoint = this;
        try {
            // Validate sending endpoint
            if (!thisEndpoint.EndpointID) {
                // Sending endpoint has not authenticated
                throw `sending endpoint has not authenticated`;
            }

            // Validate source node
            if (!thisEndpoint.DRPNode.TopologyTracker.ValidateNodeID(drpPacket.routeOptions.srcNodeID)) {
                // Source NodeID is invalid
                throw `srcNodeID ${drpPacket.routeOptions.srcNodeID} not found`;
            }

            // Validate destination node
            if (!thisEndpoint.DRPNode.TopologyTracker.ValidateNodeID(drpPacket.routeOptions.tgtNodeID)) {
                // Target NodeID is invalid
                throw `tgtNodeID ${drpPacket.routeOptions.tgtNodeID} not found`;
            }

            // Verify whether or not we SHOULD relay the node
            // if (thisEndpoint.DRPNode.IsRegistry() || thisEndpoint.DRPNode.IsRelay())

            let nextHopNodeID = thisEndpoint.DRPNode.TopologyTracker.GetNextHop(drpPacket.routeOptions.tgtNodeID);

            /** @type DRP_Endpoint */
            let targetNodeEndpoint = await thisEndpoint.DRPNode.VerifyNodeConnection(nextHopNodeID);

            // Add this node to the routing history
            drpPacket.routeOptions.routeHistory.push(thisEndpoint.DRPNode.NodeID);

            // We do not need to await the results; any target replies will automatically be routed
            targetNodeEndpoint.SendPacketString(JSON.stringify(drpPacket));
            thisEndpoint.DRPNode.PacketRelayCount++;
            //thisEndpoint.DRPNode.log(`Relaying packet...`);
            //console.dir(drpPacket);

        } catch (ex) {
            // Either could not get connection to node or command send attempt errored out
            thisEndpoint.DRPNode.log(`Could not relay message: ${ex}`);
        }
    }

    GetCmds() {
        return Object.keys(this.EndpointCmds);
    }

    async OpenHandler() {
        if (!this.wsConn) return null;
        this.RemoteAddressPortFamily = `${this.RemoteAddress()}|${this.RemotePort()}|${this.RemoteFamily()}`;
    }

    async CloseHandler() { }

    async ErrorHandler() { }

    Close() {
        if (!this.wsConn) return null;
        this.wsConn.close();
    }

    RemoveSubscriptions() {
        if (!this.wsConn) return null;
        let subscriptionIDList = Object.keys(this.Subscriptions);
        for (let i = 0; i < subscriptionIDList.length; i++) {
            let subscriptionID = subscriptionIDList[i];
            let subscriptionObject = this.Subscriptions[subscriptionID];
            subscriptionObject.Terminate();
            delete this.Subscriptions[subscriptionID];
            this.DRPNode.SubscriptionManager.Subscribers.delete(subscriptionObject);
        }
    }

    IsReady() {
        if (!this.wsConn) return null;
        if (this.wsConn && this.wsConn.readyState === 1)
            return true;
        else
            return false;
    }

    IsConnecting() {
        if (!this.wsConn) return null;
        if (this.wsConn && this.wsConn.readyState === 0)
            return true;
        else
            return false;
    }

    /**
    * @returns {string} Remote Address
    */
    RemoteAddress() {
        if (!this.wsConn) return null;
        let returnVal = null;
        if (this.wsConn && this.wsConn._socket) {
            returnVal = this.wsConn._socket.remoteAddress;
        }
        return returnVal;
    }

    /**
     * @returns {string} Remote Port
     */
    RemotePort() {
        if (!this.wsConn) return null;
        let returnVal = null;
        if (this.wsConn && this.wsConn._socket) {
            returnVal = this.wsConn._socket.remotePort;
        }
        return returnVal;
    }

    /**
     * @returns {string} Remote Family
     */
    RemoteFamily() {
        if (!this.wsConn) return null;
        let returnVal = null;
        if (this.wsConn && this.wsConn._socket) {
            returnVal = this.wsConn._socket.remoteFamily;
        }
        return returnVal;
    }

    /**
     * @returns {number} Uptime in seconds
     */
    UpTime() {
        if (!this.wsConn) return null;
        let currentTime = new Date().getTime();
        return Math.floor((currentTime - this.wsConn.openTime) / 1000);
    }

    /**
     * @returns {number} Ping time in milliseconds
     */
    PingTime() {
        if (!this.wsConn) return null;
        let returnVal = null;
        if (this.wsConn._socket) {
            returnVal = this.wsConn.pingTimeMs;
        }
        return returnVal;
    }

    ConnectionStats() {
        if (!this.wsConn) return null;
        return {
            pingTimeMs: this.PingTime(),
            uptimeSeconds: this.UpTime()
        };
    }

    IsServer() {
        if (!this.wsConn) return null;
        return this.wsConn._isServer;
    }

    log(logMessage) {
        let thisEndpoint = this;
        if (thisEndpoint.DRPNode) {
            thisEndpoint.DRPNode.log(logMessage);
        } else {
            console.log(logMessage);
        }
    }
}

module.exports = DRP_Endpoint;
