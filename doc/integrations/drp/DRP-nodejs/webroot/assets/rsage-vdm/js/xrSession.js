/**
 * XR Applet Profile
 * @param {string} appletName Applet Name
 * @param {string} appletIcon Applet Icon
 */
class XRAppletProfile {
    constructor() {
        this.appletName = "";
        this.appletIcon = "";
        this.appletPath = "";
        this.appletScript = "";
        this.appletClass = null;
        this.showInMenu = true;
        this.startupScript = "";
        this.title = "";
    }
}

class XRApplet {
    constructor(appletProfile, xrSession) {
        // Attributes from profile
        this.appletName = appletProfile.appletName;
        this.appletPath = appletProfile.appletPath;
        this.xrSession = xrSession;
    }
    terminate() {
    }
}

class XRSession {
    /**
     * XRSession owns the XRServerAgent object
     */
    constructor(appletPath) {
        let thisXRSession = this;

        this.activeApplet = null;

        this.appletProfiles = {};

        this.appletPath = appletPath;

        /** @type XRServerAgent */
        this.drpClient = null;

        let body = document.body;

        body.innerHTML = '';

        this.menuDiv = document.createElement("div");
        this.menuDiv.style = `
            position: absolute;
            top: 0;
            left: 0;
            height: 100%;
            width: 100%;
            z-index: 2;
        `;
        body.appendChild(this.menuDiv);

        this.renderCanvas = document.createElement("canvas");
        this.renderCanvas.style = `
            position: absolute;
            top: 0;
            left: 0;
            height: 100%;
            width: 100%;
            touch-action: none;
            z-index: 1;
        `;
        body.appendChild(this.renderCanvas);

        this.babylonEngine = new BABYLON.Engine(this.renderCanvas, true);

        let css = `
            body > div > div {
                padding-top: 20px;
                padding-bottom: 20px;
                text-align: center;
            }
            body > div > div > span {
                background: rgb(170, 170, 170);
                font-size: xx-large;
                padding: 10px;
            }
            body > div > div > span:hover { background-color: #00ff00 }
        `;
        let style = document.createElement('style');

        if (style.styleSheet) {
            style.styleSheet.cssText = css;
        } else {
            style.appendChild(document.createTextNode(css));
        }

        document.getElementsByTagName('head')[0].appendChild(style);
    }

    showMenu() {

    }

    startSession(wsTarget) {
        let thisXRSession = this;
        thisXRSession.drpClient = new XRServerAgent(thisXRSession);
        thisXRSession.drpClient.connect(wsTarget);
    }

    // Add Client app profile
    /**
     * @param {XRAppletProfile} appletProfile Profile describing new Window
     */
    addAppletProfile(appletProfile) {
        let thisVDMDesktop = this;

        // Check to see if we have a name and the necessary attributes
        if (!appletProfile) {
            console.log("Cannot add app - No app definition");
        } else if (!appletProfile.appletName) {
            console.log("Cannot add app - App definition does not contain 'name' parameter");
        } else if (!appletProfile.appletScript) {
            console.log("Cannot add app '" + appletProfile.appletName + "' - App definition does not contain 'appletScript' parameter");
        } else {
            thisVDMDesktop.appletProfiles[appletProfile.appletName] = appletProfile;
        }
    }

    async loadAppletProfiles() {
        let thisXRSession = this;
        await thisXRSession.loadAppletScripts();
        let appletProfileNames = Object.keys(thisXRSession.appletProfiles);
        for (let i = 0; i < appletProfileNames.length; i++) {
            let thisAppletProfile = thisXRSession.appletProfiles[appletProfileNames[i]];
            if (thisAppletProfile.showInMenu) {
                let appletNameDiv = document.createElement("div");
                let appletNameSpan = document.createElement("span");
                appletNameDiv.appendChild(appletNameSpan);
                appletNameSpan.innerHTML = thisAppletProfile.appletName;
                appletNameSpan.onclick = function () { thisXRSession.runApplet(thisAppletProfile.appletName); };
                thisXRSession.menuDiv.appendChild(appletNameDiv);
            }
        }
    }

    async loadAppletScripts() {
        let thisXRSession = this;
        let appletProfileList = Object.keys(thisXRSession.appletProfiles);
        for (let i = 0; i < appletProfileList.length; i++) {
            var tmpAppletName = appletProfileList[i];
            var appletDefinition = thisXRSession.appletProfiles[tmpAppletName];
            var tmpScriptPath = appletDefinition.appletScript;
            if (!appletDefinition.appletPath) appletDefinition.appletPath = thisXRSession.appletPath;
            if (!appletDefinition.appletScript.match(/https?:\/\//)) {
                tmpScriptPath = thisXRSession.appletPath + '/' + appletDefinition.appletScript;
                let thisAppletScript = await thisXRSession.fetchURLResource(tmpScriptPath);
                appletDefinition.appletClass = thisXRSession.evalWithinContext(appletDefinition, thisAppletScript);
            }
        }
    }

    resetSession() {
        // If there is an active applet, destroy it
        if (this.activeApplet) {
            this.activeApplet.terminate();
        }

        this.menuDiv.style.zIndex = 2;
        this.renderCanvas.style.zIndex = 1;
    }

    runApplet(appletName) {
        let thisXRSession = this;
        thisXRSession.menuDiv.style.zIndex = 1;
        thisXRSession.renderCanvas.style.zIndex = 2;
        let appletDefinition = thisXRSession.appletProfiles[appletName];
        // Create new instance of applet
        let newApp = new appletDefinition.appletClass(appletDefinition, thisXRSession);
        thisXRSession.activeApplet = newApp;
        if (newApp.runStartup) {
            newApp.runStartup();
        }
    }

    evalWithinContext(context, code) {
        let outerResults = function (code) {
            let innerResults = eval(code);
            return innerResults;
        }.apply(context, [code]);
        return outerResults;
    }

    fetchURLResource(url) {
        return new Promise(function (resolve, reject) {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", url);
            xhr.onload = function () {
                if (this.status >= 200 && this.status < 300) {
                    resolve(xhr.responseText);
                } else {
                    reject({
                        status: this.status,
                        statusText: xhr.statusText
                    });
                }
            };
            xhr.onerror = function () {
                reject({
                    status: this.status,
                    statusText: xhr.statusText
                });
            };
            xhr.send();
        });
    }
}

class XRServerAgent extends DRP_Client_Browser {
    /**
     * Agent which connects to DRP_Node (Broker)
     * @param {XRSession} xrSession XR Session object
     * @param {string} userToken User token
     */
    constructor(xrSession) {
        super();

        this.xrSession = xrSession;
    }

    async OpenHandler(wsConn, req) {
        let thisDRPClient = this;
        console.log("XR Client to server [" + thisDRPClient.wsTarget + "] opened");

        this.wsConn = wsConn;

        let response = await thisDRPClient.SendCmd("DRP", "hello", {
            "token": thisDRPClient.userToken,
            "platform": thisDRPClient.platform,
            "userAgent": thisDRPClient.userAgent,
            "URL": thisDRPClient.URL
        }, true, null);

        if (!response) window.location.reload();

        // If we don't have any appletProfiles, request them
        if (Object.keys(thisDRPClient.xrSession.appletProfiles).length) return;
        let appletProfiles = {};
        let getAppletProfilesResponse = await thisDRPClient.SendCmd("VDM", "getXRAppletProfiles", null, true, null);
        if (getAppletProfilesResponse && getAppletProfilesResponse.payload) appletProfiles = getAppletProfilesResponse.payload;
        let appletProfileNames = Object.keys(appletProfiles);
        for (let i = 0; i < appletProfileNames.length; i++) {
            let thisAppletProfile = appletProfiles[appletProfileNames[i]];
            // Manually add the xrSession to the appletProfile
            thisAppletProfile.xrSession = thisDRPClient.xrSession;
            thisDRPClient.xrSession.addAppletProfile(thisAppletProfile);
        }

        await thisDRPClient.xrSession.loadAppletProfiles();
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

            //thisDRPClient.xrSession.vdmDesktop.changeLEDColor('red');
            window.location.reload();
        }
    }
}