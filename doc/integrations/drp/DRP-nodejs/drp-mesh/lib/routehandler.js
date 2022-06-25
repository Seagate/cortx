'use strict';

const DRP_Node = require("./node");
const DRP_Endpoint = require("./endpoint");

class DRP_Endpoint_Server extends DRP_Endpoint {
    /**
     * 
     * @param {WebSocket} wsConn Websocket connection
     * @param {DRP_Node} drpNode DRP Node
     * @param {string} endpointID Remote Endpoint ID
     */
    constructor(wsConn, drpNode, endpointID) {
        super(wsConn, drpNode, endpointID);
        let thisEndpoint = this;

        this.RegisterMethod("hello", async function (...args) {
            return thisEndpoint.DRPNode.Hello(...args);
        });
    }

    async OpenHandler(req) {
        super.OpenHandler();
        let thisEndpoint = this;
    }

    async CloseHandler(closeCode) {
        let thisEndpoint = this;
        thisEndpoint.DRPNode.RemoveEndpoint(thisEndpoint, thisEndpoint.closeCallback);
    }

    async ErrorHandler(wsConn, error) {
        this.DRPNode.log("Node client [" + wsConn._socket.remoteAddress + ":" + wsConn._socket.remotePort + "] encountered error [" + error + "]");
    }
}

// Handles incoming DRP connections
class DRP_RouteHandler {
    /**
     * 
     * @param {DRP_Node} drpNode DRP Node Object
     * @param {string} route URL Route
     */
    constructor(drpNode, route) {

        let thisWebServerRoute = this;
        this.wsPingInterval = 10000;
        this.wsPingHistoryLength = 100;
        this.DRPNode = drpNode;

        if (thisWebServerRoute.DRPNode.WebServer && thisWebServerRoute.DRPNode.WebServer.expressApp && thisWebServerRoute.DRPNode.WebServer.expressApp.route !== null) {
            // This may be an Express server
            if (typeof this.DRPNode.WebServer.expressApp.ws === "undefined") {
                // Websockets aren't enabled
                throw new Error("Must enable ws on Express server");
            }
        } else {
            // Express server not present
            return;
        }

        thisWebServerRoute.DRPNode.WebServer.expressApp.ws(route, async function drpWebsocketHandler(wsConn, req) {

            // A new Websocket client has connected - create a DRP_Endpoint and assign the wsConn
            let remoteEndpoint = new DRP_Endpoint_Server(wsConn, thisWebServerRoute.DRPNode, null);

            await remoteEndpoint.OpenHandler(req);

            wsConn.on("message", function (message) {
                // Process command
                remoteEndpoint.ReceiveMessage(message);
            });

            wsConn.on("pong", function (message) {
                // Received pong; calculate time
                if (wsConn.pingSentTime) {
                    wsConn.pongRecvdTime = new Date().getTime();
                    wsConn.pingTimeMs = wsConn.pongRecvdTime - wsConn.pingSentTime;

                    // Clear values for next run
                    wsConn.pingSentTime = null;
                    wsConn.pongRecvdTime = null;
                    wsConn.missedPings = 0;

                    // Track ping history
                    if (wsConn.pingTimes.length >= thisWebServerRoute.wsPingHistoryLength) {
                        wsConn.pingTimes.shift();
                    }
                    wsConn.pingTimes.push(wsConn.pingTimeMs);
                }
            });

            wsConn.on("close", function (closeCode, reason) {
                remoteEndpoint.CloseHandler(closeCode);
                clearInterval(thisRollingPing);
            });

            wsConn.on("error", function (error) {
                remoteEndpoint.ErrorHandler(error);
            });

            // Note connection open time
            wsConn.openTime = new Date().getTime();

            // Set up wsPings tracking values
            wsConn.pingSentTime = null;
            wsConn.pongRecvdTime = null;
            wsConn.pingTimes = [];
            wsConn.missedPings = 0;

            // Set up wsPing Interval
            let thisRollingPing = setInterval(async () => {
                thisWebServerRoute.SendWsPing(wsConn, thisRollingPing);
            }, thisWebServerRoute.wsPingInterval);

            // Run wsPing now to get initial value
            thisWebServerRoute.SendWsPing(wsConn, thisRollingPing);
        });
    }

    /**
     * 
     * @param {WebSocket} wsConn
     * @param {any} intervalObj
     */
    SendWsPing(wsConn, intervalObj) {
        let thisWebServerRoute = this;
        //console.dir(wsConn);
        try {
            if (wsConn.pingSentTime) {
                // Did not receive response last interval; enter null value
                if (wsConn.pingTimes.length >= thisWebServerRoute.wsPingHistoryLength) {
                    wsConn.pingTimes.shift();
                }
                wsConn.pingTimes.push(null);

                thisWebServerRoute.DRPNode.log(`wsPing timed out to Endpoint ${wsConn.drpEndpoint.EndpointID}`);

                wsConn.missedPings++;

                if (wsConn.missedPings >= 3) {
                    wsConn.close(4000, "wsPing timeout threshold exceeded");
                }
            }
            wsConn.pingSentTime = new Date().getTime();
            wsConn.pongRecvdTime = null;
            wsConn.ping();
        } catch (ex) {
            thisWebServerRoute.DRPNode.log(`Error sending wsPing to Endpoint ${wsConn.drpEndpoint.EndpointID}: ${ex}`);
        }
    }
}

module.exports = DRP_RouteHandler;