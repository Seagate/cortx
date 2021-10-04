'use strict';

// Had to remove this so we don't have a circular eval problem
//const DRP_Node = require("./node");
const DRP_Endpoint = require("./endpoint");

const WebSocket = require('ws');
const url = require('url');

class DRP_Client extends DRP_Endpoint {
    /**
     * 
     * @param {string} wsTarget WebSocket target
     * @param {string} proxy Proxy URL
     * @param {DRP_Node} drpNode DRP Node
     * @param {string} endpointID Remote Endpoint ID
     * @param {string} endpointType Remote Endpoint Type
     */
    constructor(wsTarget, proxy, drpNode, endpointID, endpointType) {
        super(null, drpNode, endpointID, endpointType);
        this.wsTarget = wsTarget;
        this.proxy = proxy;
        let thisClient = this;

        let wsMaxPayload = 512 * 1024 * 1024;

        // Create wsConn
        let wsConn = null;
        if (thisClient.proxy) {
            let opts = url.parse(thisClient.proxy);
            let agent = new HttpsProxyAgent(opts);
            wsConn = new WebSocket(thisClient.wsTarget, "drp", { agent: agent, maxPayload: wsMaxPayload });
        } else {
            wsConn = new WebSocket(thisClient.wsTarget, "drp", { maxPayload: wsMaxPayload });
        }
        this.wsConn = wsConn;

        wsConn.on('open', function () {
            setInterval(function ping() {
                wsConn.ping(function () { });
            }, 30000);
            thisClient.OpenHandler();
        });

        wsConn.on("message", function (message) {
            // Process command
            thisClient.ReceiveMessage(message);
        });

        wsConn.on("close", function (closeCode) {
            thisClient.CloseHandler(closeCode);
        });

        wsConn.on("error", function (error) { thisClient.ErrorHandler(error); });
    }

    async RetryConnection() {
        let thisClient = this;
        let wsConn = null;
        if (thisClient.proxy) {
            let opts = url.parse(thisClient.proxy);
            let agent = new HttpsProxyAgent(opts);
            wsConn = new WebSocket(thisClient.wsTarget, "drp", { agent: agent });
        } else {
            wsConn = new WebSocket(thisClient.wsTarget, "drp");
        }
        thisClient.wsConn = wsConn;

        wsConn.on('open', function () {
            setInterval(function ping() {
                wsConn.ping(function () { });
            }, 30000);
            thisClient.OpenHandler();
        });

        wsConn.on("message", function (message) {
            // Process command
            thisClient.ReceiveMessage(message);
        });

        wsConn.on("close", function (closeCode) { thisClient.CloseHandler(closeCode); });

        wsConn.on("error", function (error) { thisClient.ErrorHandler(error); });
    }
}

module.exports = DRP_Client;