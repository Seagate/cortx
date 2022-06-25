class VDMSession extends VDMDesktop {
    /**
     * VDMSession is a VDMDesktop and owns VDMServerAgent objects
     */
    constructor(vdmDiv, vdmTitle, appletPath) {

        super(vdmDiv, vdmTitle, appletPath);

        let thisVDMSession = this;

        /** @type VDMServerAgent */
        this.drpClient = null;
    }

    startSession(wsTarget) {
        let thisVDMSession = this;
        thisVDMSession.drpClient = new VDMServerAgent(thisVDMSession);
        thisVDMSession.drpClient.connect(wsTarget);
    }
}

class rSageApplet extends VDMApplet {
    constructor(appletProfile) {
        super(appletProfile);

        let thisApplet = this;

        // Link to rSageClient
        this.vdmSession = appletProfile.vdmSession;

        // Handler for asynchronous commands received from the VDM Server
        this.recvCmd = {};

        // To track stream handlers for when window closes
        this.streamHandlerTokens = [];

    }

    // Send applet close notification to VDM Server after open
    postOpenHandler() {
        let thisApplet = this;
        /*
        thisApplet.sendCmd("VDM", "openUserApp",
            {
                appletName: thisApplet.appletName,
                appletIndex: thisApplet.appletIndex
            },
            false
        );
        */
    }

    // Send applet close notification to VDM Server after closure
    postCloseHandler() {
        let thisApplet = this;
        // Delete stream handlers
        for (let i = 0; i < thisApplet.streamHandlerTokens.length; i++) {
            let thisStreamToken = thisApplet.streamHandlerTokens[i];
            thisApplet.sendCmd("DRP", "unsubscribe", { streamToken: thisStreamToken}, false);
            thisApplet.vdmSession.drpClient.DeleteReplyHandler(thisStreamToken);
        }
        // Delete from 
        delete thisApplet.vdmDesktop.appletInstances[thisApplet.appletIndex];
        /*
        thisApplet.sendCmd("VDM", "closeUserApp",
            {
                appletName: thisApplet.appletName,
                appletIndex: thisApplet.appletIndex
            },
            false
        );
        */
    }

    /**
     * 
     * @param {string} serviceName Service Name
     * @param {string} cmdName Command
     * @param {object} cmdData Data object
     * @param {boolean} awaitResponse Await response flag
     * @return {function} Returns Promise
     */
    async sendCmd(serviceName, cmdName, cmdData, awaitResponse) {
        let thisApplet = this;
        let returnData = null;

        let response = await thisApplet.vdmSession.drpClient.SendCmd(serviceName, cmdName, cmdData, awaitResponse, null);
        if (response) returnData = response.payload;

        return returnData;
    }

    async sendCmd_StreamHandler(serviceName, cmdName, cmdData, callback) {
        let thisApplet = this;
        let returnData = null;

        let response = await thisApplet.vdmSession.drpClient.SendCmd_StreamHandler(serviceName, cmdName, cmdData, callback, thisApplet);
        if (response) returnData = response.payload;
        return returnData;
    }
}

class VDMServerAgent extends DRP_Client_Browser {
    /**
     * Agent which connects to DRP_Node (Broker)
     * @param {VDMSession} vdmSession VDM Session object
     * @param {string} userToken User token
     */
    constructor(vdmSession) {
        super();

        this.vdmSession = vdmSession;

        // This is a test function for RickRolling users remotely via DRP
        this.RickRoll = function () {
            vdmSession.openApp("RickRoll", null);
        };
    }

    async OpenHandler(wsConn, req) {
        let thisDRPClient = this;
        console.log("VDM Client to server [" + thisDRPClient.wsTarget + "] opened");

        this.wsConn = wsConn;

        let response = await thisDRPClient.SendCmd("DRP", "hello", {
            "token": thisDRPClient.userToken,
            "platform": thisDRPClient.platform,
            "userAgent": thisDRPClient.userAgent,
            "URL": thisDRPClient.URL
        }, true, null);

        if (!response) window.location.reload();

        thisDRPClient.vdmSession.changeLEDColor('green');

        // If we don't have any appletProfiles, request them
        if (Object.keys(thisDRPClient.vdmSession.appletProfiles).length) return;
        let appletProfiles = {};
        let getAppletProfilesResponse = await thisDRPClient.SendCmd("VDM", "getVDMAppletProfiles", null, true, null);
        if (getAppletProfilesResponse && getAppletProfilesResponse.payload) appletProfiles = getAppletProfilesResponse.payload;
        let appletProfileNames = Object.keys(appletProfiles);
        for (let i = 0; i < appletProfileNames.length; i++) {
            let thisAppletProfile = appletProfiles[appletProfileNames[i]];
            // Manually add the vdmSession to the appletProfile
            thisAppletProfile.vdmSession = thisDRPClient.vdmSession;
            thisDRPClient.vdmSession.addAppletProfile(thisAppletProfile);
        }

        thisDRPClient.vdmSession.loadAppletProfiles();
    }

    async CloseHandler(closeCode) {
        let thisDRPClient = this;
        thisDRPClient.Disconnect();
    }

    async ErrorHandler(wsConn, error) {
        console.log("Consumer to Broker client encountered error [" + error + "]");
        window.location.reload();
    }

    Disconnect(isGraceful) {
        let thisDRPClient = this;

        if (!isGraceful) {
            console.log("Unexpected connection drop, waiting 10 seconds for reconnect");
            setTimeout(function () {
                //window.location.href = "/";

                // Retry websocket connection
                thisDRPClient.reconnect = true;
                thisDRPClient.wsConn = null;
                thisDRPClient.connect(thisDRPClient.wsTarget);
            }, 10000);

            thisDRPClient.vdmSession.changeLEDColor('red');
            window.location.reload();
        }
    }
}