'use strict';

class DRP_SubscribableSource {
    /**
     * Tracker for a local or remote source which can be subscribed to
     * @param {string} sourceNodeID Source NodeID
     * @param {string} topicName Source Topic Name
     */
    constructor(sourceNodeID, topicName) {
        this.NodeID = sourceNodeID;
        this.TopicName = topicName;
        /** @type {Set<DRP_Subscriber>} */
        this.Subscriptions = new Set();
    }

    /**
     * 
     * @param {DRP_Subscriber} subscription Subscription to add
     */
    AddSubscription(subscription) {
        subscription.subscribedTo.add(this);
        this.Subscriptions.add(subscription);
    }

    /**
     *
     * @param {DRP_Subscriber} subscription Subscription to add
     */
    RemoveSubscription(subscription) {
        subscription.subscribedTo.delete(this);
        this.Subscriptions.delete(subscription);
    }

    async Send(message, postSendSuccess, postSendFail) {
        let thisTopic = this;

        // Send to Subscribers
        for (let thisSubscriberObj of thisTopic.Subscriptions) {
            (async () => {
                let sendFailed = await thisSubscriberObj.Send(message);
                if (sendFailed) {
                    // Send failed
                    if (postSendFail && typeof postSendFail === 'function') postSendFail(sendFailed);
                } else {
                    // Send succeeded
                    if (postSendSuccess && typeof postSendSuccess === 'function') postSendSuccess();
                }
            })();
        }
    }
}

class DRP_Subscriber {
    /**
     * @param {string} topicName Topic name
     * @param {string} scope local|zone|global
     * @param {Object.<string,object>} filter Filter
     * @param {string} targetNodeID Subscribe to specific Node
     * @param {function(Object)} sendFunction Send function
     * @param {function} sendFailCallback Send fail callback
     */
    constructor(topicName, scope, filter, targetNodeID, sendFunction, sendFailCallback) {
        this.topicName = topicName;
        this.scope = scope || "local";
        this.filter = filter || null;
        this.targetNodeID = targetNodeID;
        this.sendFunction = sendFunction;
        this.sendFailCallback = sendFailCallback;
        /** @type {Set<DRP_SubscribableSource>} */
        this.subscribedTo = new Set();

        // If a sendFailCallback is not specified, execute Terminate by default
        if (!sendFailCallback || typeof this.sendFailCallback !== 'function') {
            this.sendFailCallback = () => {
                this.Terminate();
            };
        }
    }

    /**
    * Send a message to a subscription instance
    * @param {any} sendMsg Message to send
    * @returns {boolean} Send error
    */
    async Send(sendMsg) {
        let sendFailed = await this.sendFunction(sendMsg);
        if (sendFailed) {
            await this.sendFailCallback();
        }
        return sendFailed;
    }

    /**
    * Loop over all subscribedTo objects and remove subscriptions
    */
    Terminate() {
        let thisSubscription = this;
        for (let subscriptionTarget of thisSubscription.subscribedTo) {
            subscriptionTarget.RemoveSubscription(thisSubscription);
        }
    }
}

module.exports = {
    DRP_SubscribableSource: DRP_SubscribableSource,
    DRP_Subscriber: DRP_Subscriber
};