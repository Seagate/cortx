'use strict';

const https = require('https');
const bodyParser = require('body-parser');
const express = require('express');
const expressWs = require('express-ws');
const cors = require('cors');
const fs = require('fs');

class DRP_WebServerConfig {
    constructor(port, bindingIP, sslEnabled, sslKeyFile, sslCrtFile, sslCrtFilePwd) {
        this.Port = port;
        this.BindingIP = bindingIP;
        this.SSLEnabled = sslEnabled;
        this.SSLKeyFile = sslKeyFile;
        this.SSLCrtFile = sslCrtFile;
        this.SSLCrtFilePwd = sslCrtFilePwd;
    }
}

// Instantiate Express instance
class DRP_WebServer {
    /**
     * 
     * @param {DRP_WebServerConfig} webServerConfig Web Server Configuration
     */
    constructor(webServerConfig) {
        let thisDRPWebServer = this;

        // Setup the Express web server
        this.config = webServerConfig;

        /** @type {Server} */
        this.server = null;
        this.expressApp = express();
        this.expressApp.use(cors());

        let wsMaxPayload = 512 * 1024 * 1024;

        // Is SSL enabled?
        if (thisDRPWebServer.config.SSLEnabled) {
            var optionsExpress = {
                key: fs.readFileSync(thisDRPWebServer.config.SSLKeyFile),
                cert: fs.readFileSync(thisDRPWebServer.config.SSLCrtFile),
                passphrase: thisDRPWebServer.config.SSLCrtFilePwd
            };
            let httpsServer = https.createServer(optionsExpress, thisDRPWebServer.expressApp);
            expressWs(thisDRPWebServer.expressApp, httpsServer, { wsOptions: { maxPayload: wsMaxPayload } });
            thisDRPWebServer.server = httpsServer;

        } else {
            expressWs(thisDRPWebServer.expressApp, null, { wsOptions: { maxPayload: wsMaxPayload } });
            thisDRPWebServer.server = thisDRPWebServer.expressApp;
        }

        thisDRPWebServer.expressApp.get('env');
        thisDRPWebServer.expressApp.use(bodyParser.urlencoded({
            extended: true
        }));
        thisDRPWebServer.expressApp.use(bodyParser.json());
    }

    start() {
        let thisDRPWebServer = this;
        return new Promise(function (resolve, reject) {
            try {
                let bindingIP = thisDRPWebServer.config.BindingIP || '0.0.0.0';
                thisDRPWebServer.server.listen(thisDRPWebServer.config.Port, bindingIP, function () {
                    resolve(null);
                });
            } catch (err) {
                reject(err);
            }
        });
    }
}

module.exports = {
    DRP_WebServer: DRP_WebServer,
    DRP_WebServerConfig: DRP_WebServerConfig
}