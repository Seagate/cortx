'use strict';
const DRP_Service = require('drp-mesh').Service;
var process = require('process');
let node_ssh = require('node-ssh');

const { Resolver } = require('dns').promises;
const resolver = new Resolver();

class vService {
    constructor(params) {
        this.name = params.name;
        this.server = params.server;
        this.protocol = params.protocol;
        this.port = params.port;
        this.options = params.options;
        this.bindings = params.bindings;
        /** @type {NetScalerSet} The config set */
        this.haPair = params.haPair;

        // Make methods visible via CLI
        this.Show = this.Show;
        this.Disable = this.Disable;
        this.Enable = this.Enable;
        this.Stat = this.Stat;
    }

    async Show() {
        try {
            let resultsText = await this.haPair.ExecuteCommand(`show service ${this.name} -format TEXT`);
            let returnObj = null;
            let objArray = NetScalerHost.prototype.TextToObjArray(resultsText);
            if (objArray.length > 0) returnObj = objArray[0];
            return returnObj;
        } catch (ex) {
            return ex;
        }
    }

    async Disable(params) {
        let delay = null;
        let graceful = false;
        if (params.pathList) {
            delay = params.pathList.shift();
            graceful = params.pathList.shift();
        }
        let cmdString = `disable service ${this.name}`;
        if (delay) cmdString += ` ${delay}`;
        if (graceful) cmdString += ` -graceful YES`;
        return await this.haPair.ExecuteCommand(cmdString);
    }

    async Enable() {
        return await this.haPair.ExecuteCommand(`enable service ${this.name}`);
    }

    async Stat() {
        let results = await this.haPair.ExecuteCommand(`stat service ${this.name} -fullValues`);
        return results;
    }
}

class vServer {
    constructor(params) {
        this.name = params.name;
        this.vip = params.vip;
        this.protocol = params.protocol;
        this.port = params.port;
        this.options = params.options;
        this.type = params.type;
        this.bindings = params.bindings;
        /** @type {NetScalerSet} The config set */
        this.haPair = params.haPair;

        // Make methods visible via CLI
        this.Show = this.Show;
        this.GetMembers = this.GetMembers;
        this.Disable = this.Disable;
        this.Enable = this.Enable;
        this.Stat = this.Stat;
        this.ConnectionTable = this.ConnectionTable;
    }

    async Show() {
        try {
            let resultsText = await this.haPair.ExecuteCommand(`show ${this.type} vserver ${this.name} -format TEXT`);
            let returnObj = null;
            let objArray = NetScalerHost.prototype.TextToObjArray(resultsText);
            if (objArray.length > 0) returnObj = objArray[0];
            return returnObj;
        } catch (ex) {
            return ex;
        }
    }

    async GetMembers() {
        try {
            let resultsText = await this.haPair.ExecuteCommand(`show ${this.type} vserver ${this.name}`);
            let returnObj = null;
            let objArray = NetScalerHost.prototype.TextToMemberArray(resultsText);
            return objArray;
        } catch (ex) {
            return ex;
        }
    }

    async Disable() {
        return await this.haPair.ExecuteCommand(`disable ${this.type} vserver ${this.name}`);
    }

    async Enable() {
        return await this.haPair.ExecuteCommand(`enable ${this.type} vserver ${this.name}`);
    }

    async Stat() {
        let results = await this.haPair.ExecuteCommand(`stat ${this.type} vserver ${this.name} -fullValues`);
        return results;
    }

    async ConnectionTable() {
        let cmd = "";
        if (this.haPair.members[this.haPair.activeMember].versionInfo.Version.startsWith("NS9.")) {
            cmd = `show ns connectiontable \"vsvrname == ${this.name}\"`;
        } else {
            cmd = `show ns connectiontable CONNECTION.LB_VSERVER.NAME.EQ(\"${this.name}\")`;
        }
        let results = await this.haPair.ExecuteCommand(cmd);
        return results;
    }
}

class NetScalerHost {
    /**
     * @param {string} mgmtIP NetScaler management IP Address
     * @param {string} sshKeyFileName SSH Private Key file name for nsroot
     * @param {NetScalerManager} nsManager NetScaler Manager Object
     * @param {NetScalerSet} haPair NetScaler management IP Address
     */
    constructor(mgmtIP, sshKeyFileName, nsManager, haPair) {
        /** @type {string} NetScaler Mgmt IP */
        this.mgmtIP = mgmtIP;
        /** @type {string} SSH Private Key file name for nsroot */
        this.sshKeyFileName = sshKeyFileName;
        /** @type {object} The config sections */
        this.parsedConfig = null;
        /** @type {string} The config dump */
        this.rawConfig = null;
        /** @type {NetScalerSet} The parent set */
        this.haPair = haPair;
        /** @type {NetScalerManager} The manager */
        this.manager = nsManager;


        // Make methods visible via CLI
        this.ShowVersion = this.ShowVersion;
        this.GetHAStatus = this.GetHAStatus;
        this.GetRunningConfig = this.GetRunningConfig;
    }

    // Show NS version
    async ShowVersion() {
        let thisNsHost = this;
        try {
            let resultsText = await thisNsHost.ExecuteCommand('show ns version');
            let returnObj = {};
            if (/NetScaler (.*): Build (.*), Date: (.*) {2}/.test(resultsText)) {
                returnObj = {
                    "Version": RegExp.$1,
                    "Build": RegExp.$2,
                    "Date": RegExp.$3
                };
            }
            return returnObj;
        } catch (ex) {
            return ex;
        }
    }

    // Get HA status
    async GetHAStatus() {
        let thisNsHost = this;
        try {
            let resultsText = await thisNsHost.ExecuteCommand('show ha node -format TEXT');
            let objArray = thisNsHost.TextToObjArray(resultsText);
            let returnObj = {};
            for (let i = 0; i < objArray.length; i++) {
                returnObj[objArray[i]['Id']] = objArray[i];
            }
            return returnObj;
        } catch (ex) {
            return ex;
        }
    }

    // Get running config
    async GetRunningConfig() {
        let thisNsHost = this;
        let resultsText = await thisNsHost.ExecuteCommand('show runningConfig');
        return resultsText;
    }

    // Load config and version info
    async LoadConfig() {
        let thisNsHost = this;
        thisNsHost.rawConfig = await thisNsHost.GetRunningConfig();
        thisNsHost.parsedConfig = thisNsHost.ParseConfigText(thisNsHost.rawConfig);
        thisNsHost.versionInfo = await thisNsHost.ShowVersion();
    }

    // Parse command output, translate into objects
    TextToObjArray(nsText) {
        let thisNsHost = this;
        let returnArray = [];
        let sectionPattern = new RegExp(/(?:\d+\)(\t(?:[^\n]+)(?:\n\t[^\n]+)+))/, "gm");
        let keyValPattern = new RegExp(/\s+([\S]+): {2}(\"[^\"]+\"|\S+)/, "gm");
        // Loop over groups
        let matchedSection = null;
        while ((matchedSection = sectionPattern.exec(nsText)) !== null) {
            let parsedObj = {};
            // Loop over keys and values
            let matchedKeyValue = null;
            let sectionText = matchedSection[1];
            while ((matchedKeyValue = keyValPattern.exec(sectionText)) !== null) {
                parsedObj[matchedKeyValue[1]] = matchedKeyValue[2];
            }
            returnArray.push(parsedObj);
        }
        return returnArray;
    }

    // Parse member list text, translate into members
    TextToMemberArray(nsText) {
        let thisNsHost = this;
        let returnArray = [];
        let memberPattern = new RegExp(/^(\d+)\) (\S+) \((\S+): (\d+)\) - (\S+) State: (\S.*\S)\s+Weight: (\d+)$/, "gm");
        // Loop over groups
        let memberInfo = null;
        while ((memberInfo = memberPattern.exec(nsText)) !== null) {
            let memberObj = {
                MemberIndex: memberInfo[1],
                ServiceName: memberInfo[2],
                ServiceIP: memberInfo[3],
                ServicePort: memberInfo[4],
                Protocol: memberInfo[5],
                State: memberInfo[6],
                Weight: memberInfo[7]
            };
            returnArray.push(memberObj);
        }
        return returnArray;
    }

    // Parse full config text, translate into objects
    ParseConfigText(rawConfig) {
        let thisNsHost = this;

        try {
            let nsHostConfig = {
                "MgmtIP": "",
                "HANodes": [],
                "Servers": {},
                "Services": {},
                "vServers": {},
                "lbMonitors": {},
                "csActions": {},
                "csPolicies": {}
            }; //thisProvider.configData[nsHost];
            let lineArray = rawConfig.split(/\r?\n/);
            for (let i = 0; i < lineArray.length; i++) {
                let lineData = lineArray[i];
                switch (true) {
                    case /^add HA node (?:\d+) (\S+)/.test(lineData):
                        //console.log(`NetScaler IP: ${RegExp.$1}`);
                        nsHostConfig.HANodes.push(RegExp.$1);
                        break;
                    case /^set ns config -IPAddress (\S+)/.test(lineData):
                        //console.log(`NetScaler IP: ${RegExp.$1}`);
                        nsHostConfig.MgmtIP = RegExp.$1;
                        break;
                    case /^add server (\"[^\"]*\"|\S+) (\S+)( .*)?/.test(lineData): {
                        let serverAlias = RegExp.$1,
                            serverIP = RegExp.$2,
                            options = thisNsHost.ParseOptions(RegExp.$3);
                        //console.log(`Server: ${serverAlias} -> ${serverIP} options [${options}]`);
                        nsHostConfig["Servers"][serverAlias] = {
                            "ipAddress": serverIP,
                            "options": options
                        };
                        break;
                    }
                    case /^add service (\"[^\"]*\"|\S+) (\S+) (\S+) (\S+)( .*)?/.test(lineData): {
                        let serviceName = RegExp.$1,
                            serverAlias = RegExp.$2,
                            protocol = RegExp.$3,
                            port = RegExp.$4,
                            options = thisNsHost.ParseOptions(RegExp.$5);
                        //console.log(`Service: ${serviceName} -> ${serverAlias} ${protocol}/${port} options [${options}]`);
                        nsHostConfig["Services"][serviceName] = new vService({
                            "name": serviceName,
                            "server": serverAlias,
                            "protocol": protocol,
                            "port": port,
                            "options": options,
                            "bindings": [],
                            "haPair": thisNsHost.haPair
                        });
                        break;
                    }
                    case /^add (\S+) vserver (\"[^\"]*\"|\S+) (\S+) (\S+) (\S+)( .*)?/.test(lineData): {
                        let type = RegExp.$1,
                            vServerName = RegExp.$2,
                            protocol = RegExp.$3,
                            vip = RegExp.$4,
                            port = RegExp.$5,
                            options = thisNsHost.ParseOptions(RegExp.$6);
                        //console.log(`vServer: ${vServerName} -> ${vip} ${protocol}/${port} options [${options}]`);
                        nsHostConfig["vServers"][vServerName] = new vServer({
                            "name": vServerName,
                            "vip": vip,
                            "protocol": protocol,
                            "port": port,
                            "options": options,
                            "bindings": [],
                            "haPair": thisNsHost.haPair,
                            "type": type
                        });
                        break;
                    }
                    case /^bind (\S+) vserver (\"[^\"]*\"|\S+)(?: (\"[^\"]*\"|[^-]\S+))?(?:( -.*)+)?/.test(lineData): {
                        let vServerName = RegExp.$2,
                            serviceName = RegExp.$3,
                            options = thisNsHost.ParseOptions(RegExp.$4);
                        //console.log(`vServerBind: ${vServerName} -> ${serviceName} options [${options}]`);
                        if (serviceName) {
                            nsHostConfig["vServers"][vServerName]["bindings"].push(serviceName);
                        } else {
                            // Must be policy based; forwarding?
                        }
                        break;
                    }
                    case /^add lb monitor (\"[^\"]*\"|\S+)(?: ([^-\s]\S+))?(?:( -.*)+)?/.test(lineData): {
                        let monitorName = RegExp.$1,
                            protocol = RegExp.$2,
                            options = thisNsHost.ParseOptions(RegExp.$3);
                        //console.log(`lbMonitor: ${monitorName} -> ${protocol} options [${options}]`);
                        nsHostConfig["lbMonitors"][monitorName] = {
                            "protocol": protocol,
                            "options": options,
                            "bindings": []
                        };
                        break;
                    }
                    case /^add cs action ("[^\\]+")(?:( -.*)+)?/.test(lineData): {
                        let actionName = RegExp.$1,
                            options = thisNsHost.ParseOptions(RegExp.$2);
                        //console.log(`lbMonitor: ${monitorName} -> ${protocol} options [${options}]`);
                        nsHostConfig["csActions"][actionName] = {
                            "options": options
                        };
                        break;
                    }
                    case /^add cs policy (\S+)(?:( -.*)+)?/.test(lineData): {
                        let policyName = RegExp.$1,
                            options = thisNsHost.ParseOptions(RegExp.$2);
                        //console.log(`lbMonitor: ${monitorName} -> ${protocol} options [${options}]`);
                        nsHostConfig["csPolicies"][policyName] = {
                            "options": options
                        };
                        break;
                    }
                    case /^/.test(lineData):
                        break;
                    default:
                }
            }
            return nsHostConfig;
        } catch (ex) {
            return ex;
        }
    }

    // Parse options from config line
    ParseOptions(rawOptions) {
        let parsedOptions = {};
        let rawOptionList = rawOptions.split(/ -/);
        for (let i = 0; i < rawOptionList.length; i++) {
            let optionString = rawOptionList[i];
            if (/(\S+) (.*)/.test(optionString)) {
                parsedOptions[RegExp.$1] = RegExp.$2;
            }
        }
        return parsedOptions;
    }

    // Execute command against this host
    async ExecuteCommand(command, parameters, options) {
        let thisNsHost = this;
        let ssh = new node_ssh();

        await ssh.connect({
            host: thisNsHost.mgmtIP,
            username: 'nsroot',
            privateKey: thisNsHost.sshKeyFileName,
            algorithms: {
                serverHostKey: ['ssh-rsa', 'ssh-dss']
            }
        });

        let resultsObj = await ssh.execCommand(command, parameters || [], options || {});
        /Done\n((?:.*\n)*) Done$/.test(resultsObj['stdout']);
        ssh.dispose();
        return RegExp.$1 || resultsObj['stderr'];
    }
}

class NetScalerSet {
    /**
     * @param {string} name NetScaler set name
     * @param {string[]} memberIPList List of NetScaler management IP addresses
     * @param {string} sshKeyFileName SSH Private Key file name for nsroot
     * @param {NetScalerManager} nsManager NetScaler Management object
     */
    constructor(name, memberIPList, sshKeyFileName, nsManager) {
        /** @type {String} */
        this.name = name;
        /** @type {Object<string,NetScalerHost>} */
        this.members = {};
        /** @type {String} */
        this.activeMember = null;

        this.config = null;

        this.__memberIPList = memberIPList;
        this.__sshKeyFileName = sshKeyFileName;
        this.__nsManager = nsManager;

        this.lastHAquery = {};
    }

    // Get the configs from each host then figure out which one is active
    async GetConfigs() {
        for (let i = 0; i < this.__memberIPList.length; i++) {
            let nsHostIP = this.__memberIPList[i];
            let nsHostObj = new NetScalerHost(nsHostIP, this.__sshKeyFileName, this.__nsManager, this);
            this.members[nsHostIP] = nsHostObj;
            this.__nsManager.nsHosts[nsHostIP] = nsHostObj;
            await nsHostObj.LoadConfig();
        }

        this.RunHAQuery();
    }

    // Figure out which member of an HA pair is active
    async RunHAQuery() {
        let thisSet = this;
        let memberIPs = Object.keys(thisSet.members);
        let checkMember = thisSet.members[memberIPs[0]];
        let activeMember = null;
        let results = await checkMember.GetHAStatus();
        // results is ObjHash with id (0,1) as key
        thisSet.lastHAquery = results;
        if (results && results['0']) {
            let haMemberIDlist = Object.keys(results);
            for (let i = 0; i < haMemberIDlist.length; i++) {
                let haMember = results[haMemberIDlist[i]];
                if (haMember['State'] === "Primary") {
                    activeMember = haMember['IPAddress'];
                }
            }
            thisSet.activeMember = activeMember;
            thisSet.config = thisSet.members[thisSet.activeMember].parsedConfig;
        } else {
            activeMember = "COULD NOT RETRIEVE";
        }
        return activeMember;
    }

    // Execute command against the active NS host
    async ExecuteCommand(command, parameters, options) {
        let thisSet = this;
        let targetMember = thisSet.members[thisSet.activeMember];
        return await targetMember.ExecuteCommand(command, parameters, options);
    }
}

class NetScalerManager extends DRP_Service {
    /**
     * 
     * @param {string} serviceName Service Name
     * @param {drpNode} drpNode DRP Node
     * @param {number} priority Priority (lower better)
     * @param {number} weight Weight (higher better)
     * @param {string} scope Scope [local|zone|global(defaut)]
     * @param {function} configLoadCallback Function to load config
     */
    constructor(serviceName, drpNode, priority, weight, scope, configLoadCallback) {
        super(serviceName, drpNode, "NetScalerManager", null, false, priority, weight, drpNode.Zone, scope, null, null, 1);
        let thisNsMgr = this;

        /** @type {Object<string,NetScalerSet>} */
        this.haPairs = {};
        /** @type {Array<NetScalerHost>} */
        this.nsHosts = {};
        this.__configLoadCallback = configLoadCallback;

        this.ClientCmds = {
            "saveConfigs": async function () {
                return thisNsMgr.SaveConfigs();
            },
            "refreshConfigs": async function () {
                return thisNsMgr.RefreshConfigs();
            },
            "lookupHost": async function (params) {
                let queryHost = null;
                let queryPort = null;
                let returnList = [];
                if (params.pathList && params.pathList.length >= 1) {
                    queryHost = params.pathList[0];
                    queryPort = params.pathList[1] || null;
                }
                if (params.queryHost) {
                    queryHost = params.queryHost;
                    queryPort = params.queryPort || null;
                }

                let queryIP = null;

                // Is this an FQDN or IP?
                if (/^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/.test(queryHost)) {
                    // This is an IP
                    queryIP = queryHost;
                } else {
                    // This is a host; resolve to IP
                    try {
                        const addresses = await resolver.resolve4(queryHost);
                        queryIP = addresses.shift();
                    } catch (ex) {
                        // Could not resolve address
                        return returnList;
                    }
                }

                // Loop over all sets
                let pairNames = Object.keys(thisNsMgr.haPairs);
                for (let i = 0; i < pairNames.length; i++) {
                    let thisHAPair = thisNsMgr.haPairs[pairNames[i]];

                    // Loop over vServers
                    let vServerNames = Object.keys(thisHAPair.config.vServers);
                    for (let j = 0; j < vServerNames.length; j++) {

                        let isMember = false;
                        let vServer = thisHAPair.config.vServers[vServerNames[j]];
                        let returnEntry = {
                            'haPair': pairNames[i],
                            'vServer': vServerNames[j],
                            'vip': vServer['vip'],
                            'port': vServer['port'],
                            'protocol': vServer['protocol'],
                            'options': vServer['options'],
                            'activeNS': `${thisHAPair.activeMember}`,
                            'members': []
                        };

                        // Loop over vServer members
                        for (let i = 0; i < vServer['bindings'].length; i++) {
                            let serviceName = vServer['bindings'][i];
                            // Make sure the Service is found - if not we have a config parsing problem
                            if (thisHAPair.config.Services[serviceName]) {
                                let serverName = thisHAPair.config.Services[serviceName]['server'];
                                let protocol = thisHAPair.config.Services[serviceName]['protocol'];
                                let port = thisHAPair.config.Services[serviceName]['port'];
                                let ipAddress = thisHAPair.config.Servers[serverName]['ipAddress'];
                                returnEntry.members.push(`${protocol}/${ipAddress}:${port}`);

                                // Is the IP we're checking a member of this vServer?
                                if (ipAddress === queryIP && (!queryPort || port === queryPort)) {
                                    isMember = true;
                                }

                            } else {
                                // Service could not be found!
                            }
                        }

                        // If the IP we're checking is either a member of this vServer or matches the VIP, return it
                        if (isMember || vServer['vip'] === queryIP && (!queryPort || vServer['port'] === queryPort)) {
                            returnList.push(returnEntry);
                        }
                    }
                }
                return returnList;
            },
            "lookupHostPortEntries": async function (params) {
                let queryHost = null;
                let queryPort = null;
                let returnList = [];
                if (params.pathList && params.pathList.length >= 2) {
                    queryHost = params.pathList[0];
                    queryPort = params.pathList[1];
                }
                if (params.queryHost && params.queryPort) {
                    queryHost = params.queryHost;
                    queryPort = params.queryPort;
                }

                let queryIP = null;

                // Is this an FQDN or IP?
                if (/^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/.test(queryHost)) {
                    // This is an IP
                    queryIP = queryHost;
                } else {
                    // This is a host; resolve to IP
                    try {
                        const addresses = await resolver.resolve4(queryHost);
                        queryIP = addresses.shift();
                    } catch (ex) {
                        // Could not resolve address
                        return returnList;
                    }
                }

                // Loop over all sets
                let pairNames = Object.keys(thisNsMgr.haPairs);
                for (let i = 0; i < pairNames.length; i++) {
                    let thisHAPair = thisNsMgr.haPairs[pairNames[i]];

                    // Loop over vServers
                    let vServerNames = Object.keys(thisHAPair.config.vServers);
                    for (let j = 0; j < vServerNames.length; j++) {

                        let vServer = thisHAPair.config.vServers[vServerNames[j]];
                        let returnEntry = {
                            'haPair': pairNames[i],
                            'vServer': vServerNames[j],
                            'vip': vServer['vip'],
                            'port': vServer['port'],
                            'protocol': vServer['protocol'],
                            'options': vServer['options'],
                            'activeNS': `${thisHAPair.activeMember}`,
                            'members': []
                        };

                        // Loop over vServer members
                        for (let i = 0; i < vServer['bindings'].length; i++) {
                            let serviceName = vServer['bindings'][i];
                            // Make sure the Service is found - if not we have a config parsing problem
                            if (thisHAPair.config.Services[serviceName]) {
                                let serverName = thisHAPair.config.Services[serviceName]['server'];
                                let protocol = thisHAPair.config.Services[serviceName]['protocol'];
                                let port = thisHAPair.config.Services[serviceName]['port'];
                                let ipAddress = thisHAPair.config.Servers[serverName]['ipAddress'];

                                // Is the IP we're checking a member of this vServer?
                                if (ipAddress === queryIP && port === queryPort) {
                                    returnList.push({ "nsPairName": thisHAPair.name, "entryType": "Service", "entryName": serviceName});
                                }
                            }
                        }

                        // If the IP we're checking is either a member of this vServer or matches the VIP, return it
                        if (vServer['vip'] === queryIP && vServer['port'] === queryPort) {
                            returnList.push({ "nsPairName": thisHAPair.name, "entryType": "vServer", "entryName": vServerNames[j] });
                        }
                    }
                }
                return returnList;
            },
            "enableVServer": async (params) => {
                let haPair = null;
                let vServer = null;
                if (params.pathList && params.pathList.length >= 2) {
                    haPair = params.pathList[0];
                    vServer = params.pathList[1] || null;
                }
                if (params.haPair && params.vServer) {
                    haPair = params.haPair;
                    vServer = params.vServer || null;
                }
                if (haPair && vServer && thisNsMgr.haPairs[haPair] && thisNsMgr.haPairs[haPair].config.vServers[vServer]) return thisNsMgr.haPairs[haPair].config.vServers[vServer].Enable();
                else return null;
            },
            "disableVServer": async (params) => {
                let haPair = null;
                let vServer = null;
                if (params.pathList && params.pathList.length >= 2) {
                    haPair = params.pathList[0];
                    vServer = params.pathList[1] || null;
                }
                if (params.haPair && params.vServer) {
                    haPair = params.haPair;
                    vServer = params.vServer || null;
                }
                if (haPair && vServer && thisNsMgr.haPairs[haPair] && thisNsMgr.haPairs[haPair].config.vServers[vServer]) return thisNsMgr.haPairs[haPair].config.vServers[vServer].Disable();
                else return null;
            },
            "enableService": async (params) => {
                let haPair = null;
                let service = null;
                if (params.pathList && params.pathList.length >= 2) {
                    haPair = params.pathList[0];
                    service = params.pathList[1] || null;
                }
                if (params.haPair && params.service) {
                    haPair = params.haPair;
                    service = params.service || null;
                }
                if (haPair && service && thisNsMgr.haPairs[haPair] && thisNsMgr.haPairs[haPair].config.Services[service]) return thisNsMgr.haPairs[haPair].config.Services[service].Enable();
                else return null;
            },
            "disableService": async (params) => {
                let haPair = null;
                let service = null;
                if (params.pathList && params.pathList.length >= 2) {
                    haPair = params.pathList[0];
                    service = params.pathList[1] || null;
                }
                if (params.haPair && params.service) {
                    haPair = params.haPair;
                    service = params.service || null;
                }
                if (haPair && service && thisNsMgr.haPairs[haPair] && thisNsMgr.haPairs[haPair].config.Services[service]) return thisNsMgr.haPairs[haPair].config.Services[service].Disable();
                else return null;
            }
        };

        if (configLoadCallback && typeof configLoadCallback === "function") configLoadCallback();
    }
    /**
     * @param {string} name NetScaler set name
     * @param {string[]} memberIPList List of NetScaler management IP addresses
     * @param {string} sshKeyFileName SSH Private Key file name for nsroot
     */
    async AddSet(name, memberIPList, sshKeyFileName) {
        let thisNsMgr = this;
        thisNsMgr.haPairs[name] = new NetScalerSet(name, memberIPList, sshKeyFileName, this);
        await thisNsMgr.haPairs[name].GetConfigs();
    }

    async SaveConfigs() {
        let thisNsMgr = this;
        let haPairNames = Object.keys(thisNsMgr.haPairs);
        for (let i = 0; i < haPairNames.length; i++) {
            let haPairName = haPairNames[i];
            thisNsMgr.haPairs[haPairName].ExecuteCommand('save ns config');
        }
    }

    async RefreshConfigs() {
        let thisNsMgr = this;
        let thisNode = thisNsMgr.DRPNode;
        thisNsMgr.haPairs = {};
        thisNsMgr.nsHosts = {};

        if (thisNsMgr.__configLoadCallback && typeof thisNsMgr.__configLoadCallback === "function") thisNsMgr.__configLoadCallback();
    }

}

module.exports = NetScalerManager;