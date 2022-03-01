const DRP_Node = require('drp-mesh').Node;
const { DRP_AuthRequest, DRP_AuthResponse, DRP_Authenticator } = require('drp-mesh').Auth;
const ldapjs = require('ldapjs');

class DRP_LDAP extends DRP_Authenticator {
    /**
     * @param {string} serviceName Service Name
     * @param {DRP_Node} drpNode DRP Node
     * @param {number} priority Priority (lower better)
     * @param {number} weight Weight (higher better)
     * @param {string} scope Scope [local|zone|global(defaut)]
     * @param {string} ldapURL LDAP URL
     * @param {string} userBase LDAP User Base
     * @param {string} userContainerType User container type (cn or uid)
     * @param {string} filter Search filter
     * @param {string} searchScope Search Scope
     * @param {string[]} attributes Seach attributes
     */
    constructor(serviceName, drpNode, priority, weight, scope, ldapURL, userBase, userContainerType, filter, searchScope, attributes) {
        super(serviceName, drpNode, priority, weight, scope, 1);
        this.ldapURL = ldapURL;
        this.userBase = userBase;
        this.userContainerType = userContainerType;
        this.searchFilter = filter || "(&(objectClass=person))";
        this.searchScope = searchScope || "sub";
        this.searchAttributes = attributes || ["cn", "givenName", "sn", "memberOf"];
    }

    /**
     * @param {DRP_AuthRequest} authRequest Auth Request
     * @returns {DRP_AuthResponse} Auth Response
     */
    async Authenticate(authRequest) {
        let thisService = this;
        let authResponse = null;
        let bindUserDN = `${this.userContainerType}=${authRequest.UserName},${thisService.userBase}`;
        let ldapClient = ldapjs.createClient({
            url: this.ldapURL,
            tlsOptions: {
                //enableTrace: true,
                rejectUnauthorized: false
            }
        });
        //console.log("LDAP CLIENT CREATED");
        let results = await new Promise(function (resolve, reject) {
            let returnSet = [];
            ldapClient.bind(bindUserDN, authRequest.Password, function (err) {
                // Error binding, return null
                if (err) {
                    //console.dir(err);
                    if (thisService.DRPNode.Debug) thisService.DRPNode.log(`Authenticate [${authRequest.UserName}] -> FAILED`);
                    resolve(null);
                }

                //console.log("LDAP CLIENT BOUND");

                let opts = {
                    filter: `(&(objectClass=person)(${thisService.userContainerType}=${authRequest.UserName}))`,
                    scope: thisService.searchScope,
                    attributes: thisService.searchAttributes,
                    paged: true,
                    sizeLimit: 1000
                };

                ldapClient.search(thisService.userBase, opts, function (err, res) {
                    if (err) resolve(null);
                    //      console.log("Running LDAP query...");
                    res.on('searchEntry', function (entry) {
                        returnSet.push(entry);
                    });
                    res.on('page', function (result) {
                        //console.log('page end');
                    });
                    res.on('searchReference', function (referral) {
                        //console.log('referral: ' + referral.uris.join());
                        //process.exit(0);
                        resolve(null);
                    });
                    res.on('error', function (err) {
                        //console.error('LDAP error: ' + err.message);
                        //reject('LDAP error: ' + err.message)
                        //process.exit(0);
                        resolve(null);
                    });
                    res.on('end', function (result) {
                        //        console.log('LDAP complete, status: ' + result.status);
                        if (result.status === 0) {
                            ldapClient.unbind(function (err) {
                                //assert.ifError(err);
                            });
                            resolve(returnSet);
                        } else {
                            resolve(null);
                            //reject('LDAP status non-zero: ' + result.status);
                            //console.error('LDAP status non-zero: ' + result.status);
                            //process.exit(0);
                        }
                    });
                });
            });
        });
        //console.dir(results);
        if (results && results.length) {
            let userEntry = results.shift().object;
            authResponse = new DRP_AuthResponse(thisService.GetToken(), authRequest.UserName, userEntry.cn, userEntry.memberOf, null, thisService.serviceName, thisService.DRPNode.getTimestamp());
            if (thisService.DRPNode.Debug) thisService.DRPNode.log(`Authenticate [${authRequest.UserName}] -> SUCCEEDED`);
            thisService.DRPNode.TopicManager.SendToTopic("AuthLogs", authResponse);
            thisService.DRPNode.ServiceCmd("Logger", "writeLog", { serviceName: thisService.serviceName, logData: authResponse });
        }
        return authResponse;
    }
}

module.exports = DRP_LDAP;