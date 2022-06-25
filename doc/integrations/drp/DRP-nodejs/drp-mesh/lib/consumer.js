'use strict';

//const DRP_Node = require("./node");
const DRP_Client = require("./client");

class DRP_Consumer {
    /**
     * 
     * @param {string} wsTarget WebSocket target
     * @param {string} user User
     * @param {string} pass Password
     * @param {string} proxy Proxy URL
     * @param {function} openHandler Callback after open
     */
    constructor(wsTarget, user, pass, proxy, openHandler) {
        this.user = user;
        this.pass = pass;
        this.brokerURL = wsTarget;
        this.WebProxyURL = proxy;
        /** @type {DRP_Client} */
        this.BrokerClient = null;
        this.Start(openHandler);
    }

    async Start(openHandler) {
        this.BrokerClient = new DRP_ConsumerClient(this.brokerURL, this.user, this.pass, openHandler);
        this.BrokerClient.DRPNode = this;
    }
}

class DRP_ConsumerClient extends DRP_Client {
    constructor(wsTarget, user, pass, callback) {
        super(wsTarget);
        this.user = user;
        this.pass = pass;
        this.postOpenCallback = callback;
    }

    async OpenHandler() {
        super.OpenHandler();
        let thisNodeClient = this;
        await thisNodeClient.SendCmd("DRP", "hello", { "userAgent": "nodejs", "user": thisNodeClient.user, "pass": thisNodeClient.pass }, true, null);
        thisNodeClient.postOpenCallback();
    }

    async CloseHandler(closeCode) {
        console.log("Consumer to Node client [" + this.RemoteAddress() + ":" + this.RemotePort() + "] closed with code [" + closeCode + "]");
    }

    async ErrorHandler(error) {
        console.log("Consumer to Node client encountered error [" + error + "]");
    }

    /**
     * 
     * @param {string} topicName Stream to watch
     * @param {string} scope global|local
     * @param {function} streamHandler Function to process stream packets
     */
    async WatchStream(topicName, scope, streamHandler) {
        let thisEndpoint = this;

        let streamToken = thisEndpoint.AddReplyHandler(function (message) {
            if (message && message.payload) {
                streamHandler(message.payload);
            }
        });

        let response = await thisEndpoint.SendCmd("DRP", "subscribe", { "topicName": topicName, "streamToken": streamToken, "scope": scope }, true, null);

        if (response.status === 0) {
            thisEndpoint.DeleteReplyHandler(streamToken);
            thisEndpoint.log("Subscribe failed, deleted handler");
        } else {
            thisEndpoint.log(`Subscribed to ${topicName}[${streamToken}]`);
        }
    }
}

module.exports = DRP_Consumer;