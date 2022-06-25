const DRP_Node = require('drp-mesh').Node;
const DRP_Service = require('drp-mesh').Service;
const express = require('express');
const Express_Application = express.application;
const Express_Request = express.request;
const Express_Response = express.response;
const DRP_AuthResponse = require('drp-mesh').Auth.DRP_AuthResponse;
const basicAuth = require('express-basic-auth');
const fs = require('fs').promises;

/*
class VDMServer_UserAppInstance {
    constructor(conn, appletIndex, appletName, vdmServer) {
        this.conn = conn;
        this.appletIndex = appletIndex;
        this.appletName = appletName;
        this.vdmServer = vdmServer;
        this.subscriptions = [];
    }

    RemoveSubscriptions() {
        // Remove subscription hooks specific to app
        let userAppObj = this;
        for (let subIdx in userAppObj.subscriptions) {
            let publisherObj = userAppObj.subscriptions[subIdx];
            publisherObj['subscribers'].splice(publisherObj['subscribers'].indexOf(userAppObj), 1);
        }
        userAppObj['subscriptions'] = [];
    }

    SubscribeTo(publisherObj) {
        // Add subscription hook
        let userAppObj = this;
        publisherObj['subscribers'].push(userAppObj);
        userAppObj['subscriptions'].push(publisherObj);
    }

    UnsubscribeFrom(publisherObj) {
        // Remove subscription hook
        let userAppObj = this;
        publisherObj['subscribers'].splice(publisherObj['subscribers'].indexOf(userAppObj), 1);
        userAppObj['subscriptions'].splice(userAppObj['subscriptions'].indexOf(publisherObj), 1);
    }

    ToObject() {
        // Return this
        let returnObj = {};
        let userAppObj = this;
        returnObj.appletIndex = userAppObj.appletIndex;
        returnObj.appletName = userAppObj.appletName;
        //returnObj.remoteAddress = userAppObj.remoteAddress;
        return returnObj;
    }
}
*/

class VDMAppletProfile {
    constructor(appletName, title, sizeX, sizeY, appletIcon, showInMenu, appletScript, preReqs) {
        this.appletName = appletName;
        this.title = title;
        this.sizeX = sizeX;
        this.sizeY = sizeY;
        this.appletIcon = appletIcon;
        this.showInMenu = showInMenu;
        this.appletScript = appletScript;
        this.preReqs = preReqs || [];
    }
}

class XRAppletProfile {
    constructor(appletName, title, appletIcon, showInMenu, appletScript, preReqs) {
        this.appletName = appletName;
        this.title = title;
        this.appletIcon = appletIcon;
        this.showInMenu = showInMenu;
        this.appletScript = appletScript;
        this.preReqs = preReqs || [];
    }
}

class VDMServer extends DRP_Service {
    /**
     * 
     * @param {string} serviceName Service Name
     * @param {DRP_Node} drpNode DRP Node
     * @param {string} clientDirectory Client directory
     * @param {string} vdmAppletsDir VDMApplets directory
     * @param {string} xrAppletsDir XRApplets directory
     * @param {number} cookieTimeoutMinutes Timeout for x-api-token cookies
     */
    constructor(serviceName, drpNode, clientDirectory, vdmAppletsDir, xrAppletsDir, cookieTimeoutMinutes, desktopTitle) {
        super(serviceName, drpNode, "VDM", null, true, 10, 10, drpNode.Zone, "zone", null, ["RESTLogs"], 1);

        let thisVDMServer = this;

        /** @type {Express_Application} */
        this.expressApp = thisVDMServer.DRPNode.WebServer.expressApp;

        this.CookieTimeoutMinutes = cookieTimeoutMinutes || 30;

        // Serve up static docs
        this.clientStaticDir = clientDirectory;
        this.vdmAppletsDir = vdmAppletsDir || "vdmapplets";
        this.xrAppletsDir = xrAppletsDir || "xrapplets";
        this.desktopTitle = desktopTitle || "VDM Desktop";
        this.expressApp.use(express.static(clientDirectory));

        // Define Authorizer
        let asyncAuthorizer = async function (username, password, cb) {
            let authSucceeded = false;
            let newToken = await thisVDMServer.DRPNode.GetConsumerToken(username, password);
            if (newToken) authSucceeded = true;
            return cb(null, authSucceeded);
        };

        // Get default
        this.expressApp.get('/', basicAuth({
            challenge: true,
            authorizer: asyncAuthorizer,
            authorizeAsync: true,
            unauthorizedResponse: (req) => {
                return req.auth
                    ? 'Credentials rejected'
                    : 'No credentials provided';
            }
        }), (req, res) => {
            // The authorizer only returns success/fail, so we need to do a dirty workaround - look for last token issued for this user
            let userToken = thisVDMServer.DRPNode.GetLastTokenForUser(req.auth.user);

            // Pass the x-api-token in a cookie for the WebSockets connection
            res.cookie('x-api-token', userToken, {
                expires: new Date(Date.now() + thisVDMServer.CookieTimeoutMinutes * 60000) // cookie will be removed after 5 minutes
            });
            let userAgentString = req.headers['user-agent'];
            if (userAgentString.includes(" Quest") || req.query.forceVR) {
                //res.sendFile("oculus.html", { "root": clientDirectory });
                res.send(thisVDMServer.GetXRClientHtml());
            } else {
                //res.sendFile("client.html", { "root": clientDirectory });
                res.send(thisVDMServer.GetVDMClientHtml());
            }
            //res.redirect('client.html');
            return;
        });

        // Register Endpoint commands
        // (methods should return output and optionally accept [params, wsConn, token] for streaming)

        this.clientSessions = {};

        this.VDMAppletProfiles = {};
        this.XRAppletProfiles = {};

        this.ClientCmds = {
            "getVDMAppletProfiles": async (...args) => { return await thisVDMServer.GetVDMAppletProfiles(...args); },
            "getXRAppletProfiles": async (...args) => { return await thisVDMServer.GetXRAppletProfiles(...args); },
            "listClientSessions": function () {
                let returnObj = {};
                let clientSessionIDList = Object.keys(thisVDMServer.clientSessions);
                for (let i = 0; i < clientSessionIDList.length; i++) {
                    let thisClientSessionID = clientSessionIDList[i];
                    let thisClientObj = thisVDMServer.clientSessions[thisClientSessionID];

                    if (thisClientObj) {
                        let remoteAddress = null;
                        let readyState = null;
                        if (thisClientObj.wsConn && thisClientObj.wsConn._socket) {
                            remoteAddress = thisClientObj.wsConn._socket.remoteAddress + ":" + thisClientObj.wsConn._socket.remotePort;
                            readyState = thisClientObj.wsConn._socket.readyState;
                        }
                        returnObj[thisClientSessionID] = {
                            remoteAddress: remoteAddress,
                            readyState: readyState,
                            sessionID: thisClientObj['sessionID'],
                            userName: thisClientObj['userName'],
                            openApps: {}
                        };
                        let appKeys = Object.keys(thisClientObj.openApps);
                        for (let j = 0; j < appKeys.length; j++) {
                            let appObj = thisClientObj.openApps[appKeys[j]].ToObject();
                            returnObj[thisClientSessionID].openApps[appObj.appletIndex] = appObj;
                        }
                    } else {
                        returnObj[thisClientSessionID] = {
                            remoteAddress: null, //thisVDMServer.wsClients[i]._socket.remoteAddress,
                            sessionID: null,
                            userName: null,
                            openApps: {}
                        };
                    }
                }

                return returnObj;
            },
            /*
            "openUserApp": (params, wsConn) => {
                thisVDMServer.OpenUserApp(params, wsConn);
            },
            "closeUserApp": (params, wsConn) => {
                thisVDMServer.CloseUserApp(params, wsConn);
            },
            */
            "uploadVDMApplet": async (params, wsConn) => {
                // Create new Applet Profile
                let newAppletProfile = thisVDMServer.AddVDMAppletProfile(params.appletName, params.title, params.sizeX, params.sizeY, params.appletIcon, params.showInMenu, params.appletScript, params.preReqs);
                if (!newAppletProfile || !newAppletProfile.appletName) return "ERROR";

                // Save Applet Profile
                let outputJSONPath = `${thisVDMServer.clientStaticDir}/${thisVDMServer.vdmAppletsDir}/vdm-app-${newAppletProfile.appletName}.json`;
                await fs.writeFile(outputJSONPath, JSON.stringify(newAppletProfile));
                if (params.appletContents) {
                    // Save Applet Script
                    let outputScriptPath = `${thisVDMServer.clientStaticDir}/${thisVDMServer.vdmAppletsDir}/${params.appletScript}`;
                    let outputFileData = params.appletContents;
                    await fs.writeFile(outputScriptPath, outputFileData);
                    return null;
                } else {
                    return null;
                }
            },
            "removeVDMApplet": async (params, wsConn) => {
                return thisVDMServer.RemoveVDMApplet(params.appletName);
            }

        };

        this.expressApp.use(function vdmServerAttachHandler(req, res, next) {
            req.VDMServer = thisVDMServer;
            next();
        });

        this.LoadApplets();
    }

    async LoadApplets() {
        let thisVDMServer = this;
        // List VDM applet profiles
        let vdmDirData = await fs.readdir(thisVDMServer.clientStaticDir + '/' + thisVDMServer.vdmAppletsDir);
        for (var i = 0; i < vdmDirData.length; i++) {
            let fileName = vdmDirData[i];

            if (fileName.match(/^vdm-app-.*\.json$/)) {
                // Load each profile
                let fileData = await fs.readFile(thisVDMServer.clientStaticDir + '/' + thisVDMServer.vdmAppletsDir + '/' + fileName, 'utf8');
                /** @type {VDMAppletProfile} */
                let appletProfile = JSON.parse(fileData);
                //console.dir(appletProfile);
                thisVDMServer.AddVDMAppletProfile(appletProfile.appletName, appletProfile.title, appletProfile.sizeX, appletProfile.sizeY, appletProfile.appletIcon, appletProfile.showInMenu, appletProfile.appletScript, appletProfile.preReqs);
            }
        }

        // List XR applet profiles
        let xrDirData = await fs.readdir(thisVDMServer.clientStaticDir + '/' + thisVDMServer.xrAppletsDir);
        for (var i = 0; i < xrDirData.length; i++) {
            let fileName = xrDirData[i];

            if (fileName.match(/^xr-app-.*\.json$/)) {
                // Load each profile
                let fileData = await fs.readFile(thisVDMServer.clientStaticDir + '/' + thisVDMServer.xrAppletsDir + '/' + fileName, 'utf8');
                /** @type {VDMAppletProfile} */
                let appletProfile = JSON.parse(fileData);
                //console.dir(appletProfile);
                thisVDMServer.AddXRAppletProfile(appletProfile.appletName, appletProfile.title, appletProfile.appletIcon, appletProfile.showInMenu, appletProfile.appletScript, appletProfile.preReqs);
            }
        }
    }

    /**
     * 
     * @param {string} appletName Applet name
     * @param {string} title Window title
     * @param {integer} sizeX Window width
     * @param {interger} sizeY Window height
     * @param {any} appletIcon Applet icon
     * @param {any} showInMenu Should it show in menu
     * @param {any} appletScript Script to execute
     * @param {Object.<string,string>[]} preReqs Pre-requisites
     * @returns {VDMAppletProfile} New applet profile
     */
    AddVDMAppletProfile(appletName, title, sizeX, sizeY, appletIcon, showInMenu, appletScript, preReqs) {
        if (appletName && title && sizeX && sizeY && appletScript) {
            let newAppletProfile = new VDMAppletProfile(appletName, title, sizeX, sizeY, appletIcon, showInMenu, appletScript, preReqs);
            this.VDMAppletProfiles[newAppletProfile.appletName] = newAppletProfile;
            return newAppletProfile;
        }
    }

    /**
     * 
     * @param {string} appletName Applet name
     * @param {string} title Window title
     * @param {any} appletIcon Applet icon
     * @param {any} showInMenu Should it show in menu
     * @param {any} appletScript Script to execute
     * @param {Object.<string,string>[]} preReqs Pre-requisites
     * @returns {XRAppletProfile} New applet profile
     */
    AddXRAppletProfile(appletName, title, appletIcon, showInMenu, appletScript, preReqs) {
        if (appletName && title && appletScript) {
            let newAppletProfile = new XRAppletProfile(appletName, title, appletIcon, showInMenu, appletScript, preReqs);
            this.XRAppletProfiles[newAppletProfile.appletName] = newAppletProfile;
            return newAppletProfile;
        }
    }

    RemoveVDMApplet(appletName) {
        delete this.VDMAppletProfiles[appletName];
    }

    GetVDMClientHtml() {
        let thisVDMServer = this;
        let returnHtml = `<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>${thisVDMServer.desktopTitle}</title>
    <meta name="msapplication-TileColor" content="#5bc0de" />

    <!-- External CSS -->
    <link rel="stylesheet" href="https://stackpath.bootstrapcdn.com/bootstrap/4.3.1/css/bootstrap.min.css">
    <link rel="stylesheet" href="https://stackpath.bootstrapcdn.com/font-awesome/4.7.0/css/font-awesome.min.css">

    <!-- VDM -->
    <link rel="stylesheet" href="assets/rsage-vdm/css/vdm.css">

</head>
<body>

    <div id="vdmDesktop"></div>

    <!-- External Scripts -->
    <script src="https://cdnjs.cloudflare.com/ajax/libs/jquery/3.3.1/jquery.min.js"></script>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/jqueryui/1.12.1/jquery-ui.js"></script>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/popper.js/1.14.7/umd/popper.min.js"></script>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/twitter-bootstrap/4.3.1/js/bootstrap.min.js"></script>

    <!-- VDM -->
    <script src="assets/rsage-vdm/js/drpClient.js"></script>
    <script src="assets/rsage-vdm/js/vdmCore.js"></script>
    <script src="assets/rsage-vdm/js/vdmSession.js"></script>

    <!-- VDM Client script -->
    <script>
window.onload = function () {
    // Get target DIV
    let mainPage = document.getElementById('vdmDesktop');

    // Set applets path
    let vdmAppletsPath = "${thisVDMServer.vdmAppletsDir}";

    let vdmSession = new VDMSession(mainPage, "${thisVDMServer.desktopTitle}", vdmAppletsPath);

    vdmSession.startSession();
};
    </script>

</body>
</html>`;
        return returnHtml;
    }

    GetXRClientHtml() {
        let thisVDMServer = this;
        let returnHtml = `<!DOCTYPE html>
<html>
<head>
    <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />

    <title>DRP XR Interface</title>

    <script src="https://cdn.babylonjs.com/babylon.js"></script>
    <script src="https://cdn.babylonjs.com/loaders/babylonjs.loaders.min.js"></script>

    <script src="assets/rsage-vdm/js/drpClient.js"></script>
    <script src="assets/rsage-vdm/js/xrSession.js"></script>

    <!-- XR Client script -->
    <script>
window.onload = function () {

    // Set applets path
    let xrAppletsPath = "${thisVDMServer.xrAppletsDir}";

    let xrSession = new XRSession(xrAppletsPath);

    xrSession.startSession();
};
    </script>

    <style>
        html, body {
            overflow: hidden;
            width: 100%;
            height: 100%;
            margin: 0;
            padding: 0;
        }
    </style>
</head>
<body>
</body>
</html>`;
        return returnHtml;
    }

    GetVDMAppletProfiles() {
        return this.VDMAppletProfiles;
    }

    GetXRAppletProfiles() {
        return this.XRAppletProfiles;
    }
    /*
    OpenUserApp(params, wsConn) {
        let thisVDMServer = this;
        thisVDMServer.LogWSClientEvent(wsConn, "opened app '" + params["appletName"] + "' [" + params["appletIndex"] + "]");
        // Create object to represent open app under client connection['openApps'] object
        wsConn.clientObj.openApps[params["appletIndex"]] = new VDMServer_UserAppInstance(wsConn, params["appletIndex"], params["appletName"], thisVDMServer);
    }

    CloseUserApp(params, wsConn) {
        let thisVDMServer = this;
        thisVDMServer.LogWSClientEvent(wsConn, "closed app '" + params["appletName"] + "' [" + params["appletIndex"] + "]");
        // Remove Subscriptions
        wsConn.clientObj.openApps[params["appletIndex"]].RemoveSubscriptions();
        // Remove from user app hash
        delete wsConn.clientObj.openApps[params["appletIndex"]];
    }

    CloseAllUserApps(clientObj) {
        let thisVDMServer = this;
        Object.keys(clientObj.openApps).forEach(function (appletIndex) {
            thisVDMServer.LogWSClientEvent(clientObj.openApps[appletIndex]["conn"], "closed app '" + clientObj.openApps[appletIndex]["appletName"] + "' [" + clientObj.openApps[appletIndex]["appletIndex"] + "] - FORCED");
            // Remove Subscriptions
            clientObj.openApps[appletIndex].RemoveSubscriptions();
            // Remove from user app hash
            delete clientObj.openApps[appletIndex];
        });
    }
    */
    LogWSClientEvent(conn, logMsg) {
        let thisVDMServer = this;
        thisVDMServer.DRPNode.log(logMsg);
    }
}

module.exports = VDMServer;
