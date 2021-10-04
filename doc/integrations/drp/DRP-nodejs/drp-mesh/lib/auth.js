'use strict';

// Had to remove this so we don't have a circular eval problem
//const DRP_Node = require("./node");
const DRP_Service = require("./service");
const { v4: uuidv4 } = require('uuid');

class DRP_AuthRequest {
    /**
     * A request should consist of a user/pass combo or a pre-shared token for services
     * @param {string} userName User name
     * @param {string} password User password
     * @param {string} token Service token
     */
    constructor(userName, password, token) {
        this.UserName = userName;
        this.Password = password;
        this.Token = token;
    }
}

class DRP_AuthResponse {
    /**
     * Response from authentication attempt
     * @param {string} token Token provided by authentication service
     * @param {string} userName User Name
     * @param {string} fullName Full Name
     * @param {string[]} groups Member of Groups
     * @param {Object.<string,object>} misc Miscellaneous Attributes
     * @param {string} authService Service used to authenticate
     * @param {string} authTimestamp Timestamp of authentication
     */
    constructor(token, userName, fullName, groups, misc, authService, authTimestamp) {
        this.Token = token;
        this.UserName = userName;
        this.FullName = fullName;
        this.Groups = groups;
        this.Misc = misc;
        this.AuthService = authService;
        this.AuthTimestamp = authTimestamp;
    }
}

/**
 * Placeholder for Authentication Function
 * @function
 * @param {DRP_AuthRequest} authRequest Parameters to authentication function
 * @returns {DRP_AuthResponse} Response from authentication function
 */
function DRP_AuthFunction(authRequest) {
    return new DRP_AuthResponse();
}

class DRP_Authenticator extends DRP_Service {
    /**
     * 
     * @param { string } serviceName Service Name
     * @param { DRP_Node } drpNode DRP Node
     * @param { number } priority Lower better
     * @param { number } weight Higher better
     * @param { string } scope Availability Local | Zone | Global
     * @param { number } status Service status[0 | 1 | 2]
     **/
    constructor(serviceName, drpNode, priority, weight, scope, status) {
        super(serviceName, drpNode, "Authenticator", null, false, priority, weight, drpNode.Zone, scope, null, ["AuthLogs"], status);
        let thisAuthenticator = this;

        this.ClientCmds = {
            authenticate: async function (authPacket) { return thisAuthenticator.Authenticate(authPacket); }
        };
    }

    /**
     * Authenticate User
     * @param {DRP_AuthRequest} authRequest Parameters to authentication function
     * @returns {DRP_AuthResponse} Response from authentication function
     */
    async Authenticate(authRequest) {
        let authResponse = new DRP_AuthResponse();
        return authResponse;
    }

    /**
     * @returns {string} New UUID
     */
    GetToken() {
        return uuidv4();
    }
}

module.exports = {
    DRP_AuthRequest: DRP_AuthRequest,
    DRP_AuthResponse: DRP_AuthResponse,
    DRP_AuthFunction: DRP_AuthFunction,
    DRP_Authenticator: DRP_Authenticator
};