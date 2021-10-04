'use strict';

let IsTrue = function (value) {
    if (typeof (value) === 'string') {
        value = value.trim().toLowerCase();
    }
    switch (value) {
        case true:
        case "true":
        case 1:
        case "1":
        case "on":
        case "y":
        case "yes":
            return true;
        default:
            return false;
    }
}

class DRP_AuthInfo {
    /**
     * 
     * @param {string} type
     * @param {string} value
     * @param {any} userInfo
     */
    constructor(type, value, userInfo) {
        this.type = type || null;
        this.value = value || null;
        this.userInfo = userInfo || null;
    }
}

class DRP_Permission {
    /**
     * 
     * @param {boolean} read
     * @param {boolean} write
     * @param {boolean} execute
     */
    constructor(read, write, execute) {
        this.read = IsTrue(read);
        this.write = IsTrue(write);
        this.execute = IsTrue(execute);
    }
}

class DRP_PermissionSet {
    /**
     * 
     * @param {Object<string,DRP_Permission>} keys
     * @param {Object<string,DRP_Permission>} users
     * @param {Object<string,DRP_Permission>} groups
     */
    constructor(keys, users, groups) {
        this.Keys = keys || {};
        this.Users = users || {};
        this.Groups = groups || {};
    }
}

class DRP_Securable {
    /**
     * 
     * @param {DRP_PermissionSet} permissionSet Permission set for accessing object
     */
    constructor(permissionSet) {
        this.__permissionSet = permissionSet;
    }

    /**
     * 
     * @param {DRP_AuthInfo} callerAuthInfo
     * @param {string} operationType
     */
    __IsAllowed(callerAuthInfo, operationType) {
        try {
            if (callerAuthInfo && callerAuthInfo.type) {
                // Is it a token or a key?
                switch (callerAuthInfo.type) {
                    case 'key':
                        // Check API key permissions
                        return this.__CheckPermission(this.__permissionSet.Keys[callerAuthInfo.value], operationType);
                        break;
                    case 'token':
                        // Check individual permissions
                        if (this.__CheckPermission(this.__permissionSet.Users[callerAuthInfo.userInfo.UserName], operationType)) return true;

                        // Check group permissions
                        for (let i = 0; i < callerAuthInfo.userInfo.Groups.length; i++) {
                            let userGroupName = callerAuthInfo.userInfo.Groups[i];
                            if (this.__CheckPermission(this.__permissionSet.Groups[userGroupName], operationType)) return true;
                        }
                        break;
                    default:
                }
            }
            return false;
        } catch (ex) {
            return false;
        }
    }

    /**
     * 
     * @param {DRP_Permission} thisPermission
     * @param {string} operationType
     */
    __CheckPermission(thisPermission, operationType) {
        if (!thisPermission) return false;
        let isAllowed = false;
        switch (operationType) {
            case 'read':
                if (thisPermission.read) isAllowed = true;
                break;
            case 'write':
                if (thisPermission.write) isAllowed = true;
                break;
            case 'execute':
                if (thisPermission.execute) isAllowed = true;
                break;
            default:
        }
        return isAllowed;
    }

    async __Read(callerAuthInfo, ...params) {
    }

    async __Write(callerAuthInfo, ...params) {
    }

    async __Execute(callerAuthInfo, ...params) {
        let results = null;
        if (this.IsAllowed(callerAuthInfo)) {
            results = await Execute(...params);
        } else {
            results = "UNAUTHORIZED";
        }
        return results;
    }
}

class DRP_VirtualDirectory extends DRP_Securable {
    constructor(securedObject, permissionSet) {
        super(securedObject, permissionSet);
    }
}

module.exports = {
    DRP_Permission: DRP_Permission,
    DRP_PermissionSet: DRP_PermissionSet,
    DRP_Securable: DRP_Securable,
    DRP_VirtualDirectory: DRP_VirtualDirectory
}