const DRP_Service = require('drp-mesh').Service;
const DRP_Node = require('drp-mesh').Node;
const DRP_Hive = require('./hive');

class Cortex extends DRP_Service {
    /**
     * 
     * @param {string} serviceName Service Name
     * @param {DRP_Node} drpNode DRP Node
     * @param {DRP_Hive} hiveObj Hive
     * @param {function} postHiveLoad Post load function
     */
    constructor(serviceName, drpNode, hiveObj, postHiveLoad) {
        super(serviceName, drpNode, "Cortex", null, false, 10, 10, drpNode.Zone, "global", null, null, 1);
        let thisCortex = this;

        // Set DRP Broker client
        this.DRPNode = drpNode;

        // Initialize Managed Object Types Hash
        this.ObjectTypes = {};

        // Instantiate Hive
        this.Hive = hiveObj;

        // Start VDM Hive
        this.Hive.Start(function () {
            // After Hive finishes initializing....
            postHiveLoad();
            // Start add on modules
            //myModule.Start();
        });

        this.ClientCmds = {};
    }

    ListObjectTypes() {
        let thisCortex = this;
        let returnObj = {};
        let objectTypeList = Object.keys(thisCortex.ObjectTypes);
        for (let i = 0; i < objectTypeList.length; i++) {
            //returnObj[objectTypeList[i]] = thisCortex.ObjectTypes[objectTypeList[i]].Name;
            let objectType = objectTypeList[i];
            let objectManager = thisCortex.ObjectTypes[objectType];
            returnObj[objectTypeList[i]] = {
                "ManagedBy": objectManager.Name,
                "Count": Object.keys(objectManager.ManagedObjects[objectType]).length
            };
        }
        return returnObj;
    }

    ListObjectsOfType(objectType) {
        let thisCortex = this;
        let returnObj = {};
        let objectTypeMgr = thisCortex.ObjectTypes[objectType];
        if (objectTypeMgr && objectTypeMgr.ManagedObjects[objectType]) {
            let objectKeyList = Object.keys(objectTypeMgr.ManagedObjects[objectType]);

            for (let i = 0; i < objectKeyList.length; i++) {
                returnObj[objectKeyList[i]] = objectTypeMgr.ManagedObjects[objectType][objectKeyList[i]].ToPacket();

                // Only return the first 10
                if (i === 9) {
                    i = objectKeyList.length;
                }
            }
        }
        return returnObj;
    }

    GetObject(objectType, objectID) {
        let thisCortex = this;
        let returnObj = {};
        let objectTypeMgr = thisCortex.ObjectTypes[objectType];
        if (objectTypeMgr && objectTypeMgr.ManagedObjects[objectType] && objectTypeMgr.ManagedObjects[objectType][objectID]) {
            returnObj = objectTypeMgr.ManagedObjects[objectType][objectID].ToPacket();
        }
        return returnObj;
    }

}

// Define parent Cortex Object class
class CortexObject {
    constructor(objectID, objectManager) {
        this.key = objectID;
        this.ObjectManager = objectManager;
        if (this.key) {
            this.RunHiveMapQueries();
        }
    }

    RunHiveMapQueries() {
        let objectMapAttributes = Object.keys(this.HiveObjectMaps);
        for (let i = 0; i < objectMapAttributes.length; i++) {
            let thisAttribute = objectMapAttributes[i];
            let mapQuery = new HiveMapQuery(this.ObjectManager.CortexServer.Hive, this.HiveObjectMaps[thisAttribute].query, this.HiveObjectMaps[thisAttribute].multiplicity);
            let mapQueryRunOutput = mapQuery.Run(this.KeyStereotype, this.key);

            this[thisAttribute] = mapQueryRunOutput;
        }
    }

    ToJSON() {
        return JSON.stringify(this.ToPacket());
    }

    ToPacket() {
        let returnData = {};

        if (this.BaseAttributes) {
            // Add attributes
            for (let i = 0; i < this.BaseAttributes.length; i++) {
                let thisAttribute = this.BaseAttributes[i];
                returnData[thisAttribute] = this[thisAttribute];
            }
        }

        if (this.HiveObjectMaps) {
            // Add Linked Objects
            let keys = Object.keys(this.HiveObjectMaps);
            for (let i = 0; i < keys.length; i++) {

                // key = variable name to store object list
                let key = keys[i];

                // Add object array
                returnData[key] = [];

                // Loop over array items
                for (let j = 0; j < this[key].length; j++) {
                    returnData[key].push(this[key][j].data);
                }
            }
        }

        // Return object
        return returnData;
    }

    GetKeyObjAttr(attrName) {
        let returnVal = null;
        if (this.HasKeyObj()) {
            let keyObj = this[this.KeyClassName][0];
            if (keyObj.data.hasOwnProperty(attrName)) {
                returnVal = keyObj.data[attrName];
            }
        } else {
            let i = 0;
        }
        return returnVal;
    }

    HasKeyObj() {
        return this.KeyClassName && this[this.KeyClassName] && this[this.KeyClassName].length > 0;
    }
}

Object.defineProperties(CortexObject.prototype, {
    "KeyStereotype": { get: function () { return null; } },
    "BaseAttributes": { get: function () { return []; } },
    "HiveObjectMaps": { get: function () { return {}; } }
});

class CortexObjectQuery {

    // Define constructor
    constructor(matchType) {
        this.Conditions = [];
        this.MatchType = matchType;
        if (typeof this.MatchType === 'undefined') this.MatchType = 'ALL';
    }

    AddCondition(checkValue1, operator, checkValue2) {
        let conditionAdded = false;

        // See if we have an evaluator for the suppled operator
        if (this.ConditionEvaluators.hasOwnProperty(operator)) {
            let newCondition = new CortexObjectQuery_Condition(checkValue1, operator, checkValue2);
            this.Conditions.push(newCondition);
            conditionAdded = true;
        }
    }

    Evaluate(targetObject) {
        // Evaluate Conditions
        let returnVal = false;

        // The goal is to eval the condition values and autodetect literal vs object attribute values, but
        // for now we have to assume the left value is an object parameter and the right value is literal

        switch (this.MatchType) {
            case 'ANY':
                // On first match, return success
                returnVal = false;
                for (let i = 0; i < this.Conditions.length; i++) {
                    let thisCondition = this.Conditions[i];
                    let tmpLeftValue = this.GetValueFromObj(thisCondition.CheckValue1, targetObject);
                    let tmpRightValue = this.GetValueFromLiteral(thisCondition.CheckValue2);
                    if (this.ConditionEvaluators[thisCondition.Operator](tmpLeftValue, tmpRightValue)) {
                        returnVal = true;
                        break;
                    }
                }
                break;
            case 'ALL':
                // If match all, return success
                returnVal = false;
                for (let i = 0; i < this.Conditions.length; i++) {
                    let thisCondition = this.Conditions[i];
                    let tmpLeftValue = this.GetValueFromObj(thisCondition.CheckValue1, targetObject);
                    let tmpRightValue = this.GetValueFromLiteral(thisCondition.CheckValue2);

                    if (this.ConditionEvaluators[thisCondition.Operator](tmpLeftValue, tmpRightValue)) {
                        returnVal = true;
                    } else {
                        returnVal = false;
                        break;
                    }
                }
                break;
            case 'NONE':
                // If match none, return success
                returnVal = true;
                for (let i = 0; i < this.Conditions.length; i++) {
                    let thisCondition = this.Conditions[i];
                    let tmpLeftValue = this.GetValueFromObj(thisCondition.CheckValue1, targetObject);
                    let tmpRightValue = this.GetValueFromLiteral(thisCondition.CheckValue2);
                    if (this.ConditionEvaluators[thisCondition.Operator](tmpLeftValue, tmpRightValue)) {
                        returnVal = false;
                        break;
                    }
                }
                break;
            default:
                return false;
        }

        return returnVal;
    }

    GetValueFromObj(checkValue, targetObject) {
        let returnVal = null;
        if (checkValue !== null && checkValue !== "" && typeof targetObject[checkValue] !== 'undefined') {
            returnVal = targetObject[checkValue];
        }
        return returnVal;
    }

    GetValueFromLiteral(checkValue) {
        let returnVal = null;
        if (typeof checkValue === 'string') {
            if (checkValue.toLowerCase() === 'null') {
                // Set to null
                returnVal = null;
            } else if (checkValue.toLowerCase() === 'true') {
                // Set to bool true
                returnVal = true;
            } else if (checkValue.toLowerCase() === 'false') {
                // Set to bool false
                returnVal = false;
            } else {
                if (checkValue.match(/^\"/)) {
                    // Remove any enclosing double quotes, return remaining string
                    returnVal = checkValue.replace(/^\"|\"$/g, '');
                } else {
                    if (!isNaN(checkValue)) {
                        // Return number
                        returnVal = parseInt(checkValue);
                    } else {
                        // Return string
                        returnVal = checkValue;
                    }
                }
            }
        } else if (typeof checkValue === 'object') {
            returnVal = checkValue;
        }
        return returnVal;
    }
}

CortexObjectQuery.prototype.ConditionEvaluators = {
    '==': function (checkValue1, checkValue2) { return checkValue1 !== null && checkValue1.toLowerCase() === checkValue2.toLowerCase(); },
    '!=': function (checkValue1, checkValue2) { return checkValue1 !== checkValue2; },
    '<': function (checkValue1, checkValue2) { return checkValue1 < checkValue2; },
    '>': function (checkValue1, checkValue2) { return checkValue1 > checkValue2; },
    'in': function (checkValue1, checkValue2) { return checkValue2.contains(checkValue1); },
    'REGEX': function (checkValue1, checkValue2) {
        return checkValue1 !== null && checkValue1.match(checkValue2);
    }
};

class CortexObjectQuery_Condition {

    // Define constructor
    constructor(checkValue1, operator, checkValue2) {
        this.CheckValue1 = checkValue1;
        this.Operator = operator;
        this.CheckValue2 = checkValue2;
        return this;
    }
}

class HiveMapQuery {
    constructor(hive, queryString, multiplicity) {
        this.hive = hive;
        this.queryString = queryString;
        this.query = queryString.split('|');
        this.multiplicity = multiplicity;
    }

    Run(objectKeyType, objectKey) {
        this.rootIdxObj = this.GetIndexObj(objectKeyType, objectKey);

        // If we don't have a match, return null
        if (!this.rootIdxObj) return [];

        // We have a match, seed the candidates list
        this.candidates = [this.rootIdxObj];
        this.KeyStereotype = objectKeyType;
        this.KeyValue = objectKey;

        this.lastIndexType = objectKeyType;
        this.lastIndexKey = objectKey;

        // Loop over each query section
        for (let i = 0; i < this.query.length; i++) {
            //console.log("Check -> " + this.query[i]);
            this.Evaluate(this.query[i]);
        }

        return this.candidates;
    }

    Evaluate(queryPart) {
        let returnCandidates = [];
        let results = /^(MK|FK|IDX)\(([^\)]*)?\)$/.exec(queryPart);
        if (!results) {
            // Query not formatted properly
            return returnCandidates;
        }
        let evalCmd = results[1];   // MK, FK or IDX
        let qualifiers = this.ParseQualifiers(results[2]);   // class or linkedby
        //console.log("  Parsing check -> " + evalCmd + ":" + results[2]);
        switch (evalCmd) {
            case 'MK':
                returnCandidates = this.Evaluate_KEY('MK', qualifiers);
                break;
            case 'FK':
                returnCandidates = this.Evaluate_KEY('FK', qualifiers);
                break;
            case 'IDX':
                returnCandidates = this.Evaluate_IDX(qualifiers);
                break;
            default:
        }

        this.candidates = returnCandidates;
    }

    Evaluate_KEY(keyType, qualifiers) {
        let qualifiedList = [];
        let checkObjectList = [];
        // Loop over candidates
        for (let i = 0; i < this.candidates.length; i++) {
            let candidate = this.candidates[i];

            let compareIndex = candidate;

            // Get the keytype; is it an array?
            if (candidate[keyType].constructor === Array) {
                // It's an array; copy to out check list
                checkObjectList = candidate[keyType];
            } else {
                // Either single object or null
                if (candidate[keyType]) {
                    checkObjectList.push(candidate[keyType]);
                }
            }

            // Loop over object instances
            for (let j = 0; j < checkObjectList.length; j++) {
                let checkObject = checkObjectList[j];
                let qualified = true;
                for (let k = 0; k < qualifiers.length; k++) {
                    let qualifier = qualifiers[k];
                    qualified = this.EvaluateQualifier(checkObject, qualifier, compareIndex);
                    if (!qualified) {
                        break;
                    }
                }
                if (qualified) {
                    qualifiedList.push(checkObject);
                }
            }
        }
        return qualifiedList;
    }

    Evaluate_IDX(qualifiers) {
        let qualifiedList = [];
        let checkObjectList = this.candidates;

        // Loop over object instances
        for (let j = 0; j < checkObjectList.length; j++) {
            let checkObject = checkObjectList[j];
            let qualified = true;
            let linkedbyAttr = null;
            for (let k = 0; k < qualifiers.length; k++) {
                let qualifier = qualifiers[k];
                qualified = this.EvaluateQualifier(checkObject, qualifier, null);
                if (qualifier.type === "linkedby") {
                    linkedbyAttr = qualifier.value;
                }
                if (!qualified) {
                    break;
                }
            }
            if (qualified) {
                // If qualified, push the target index object instead of the candidate
                let classDef = this.hive.HiveClasses[checkObject.data['_objClass']];
                let attrDef = classDef.Attributes[linkedbyAttr];
                let sTypeName = attrDef['Stereotype'];
                qualifiedList.push(this.hive.HiveIndexes[sTypeName].IndexRecords[checkObject.data[linkedbyAttr]]);
            }
        }
        return qualifiedList;
    }


    EvaluateQualifier(candidate, qualifier, compareIndex) {
        let returnVal = false;
        let objType = "";
        if (candidate.data) {
            objType = "dataObj [" + candidate.data['_objClass'] + "]";
        } else {
            objType = "indexObj [" + candidate.sType + "]";
        }
        //console.log("    Evaluating for " + qualifier.type + ":" + qualifier.value + " against " + objType);
        switch (qualifier.type) {
            case 'class':
                if (!candidate.data) {
                    let bob = 1;
                }
                if (candidate.data['_objClass'] === qualifier.value) {
                    returnVal = true;
                }
                break;
            case 'linkedby':

                // Are we evaluating an index of data object?
                if (candidate.hasOwnProperty('data')) {
                    // The candidate is a data object; see if it backlinks to the compareIndex by the given attribute

                    // back to the index via this value
                    // ( FROM IDX )
                    // 1. Get the field stereotype and value
                    let classDef = this.hive.HiveClasses[candidate.data['_objClass']];
                    if (!classDef) {
                        return false;
                    }
                    let attrDef = classDef.Attributes[qualifier.value];
                    if (!attrDef) {
                        return false;
                    }
                    let sTypeName = attrDef['Stereotype'];
                    if (!sTypeName) {
                        return false;
                    }
                    if (!this.hive.HiveIndexes[sTypeName]) {
                        return false;
                    }
                    if (!candidate.data[qualifier.value]) {
                        return false;
                    }
                    if (compareIndex) {
                        if (candidate.data[qualifier.value].constructor === Array) {
                            for (let i = 0; i < candidate.data[qualifier.value].length; i++) {
                                if (candidate.data[qualifier.value][i].toLowerCase() === compareIndex.key) {
                                    return true;
                                }
                            }
                            return false;
                        } else {
                            if (candidate.data[qualifier.value].toLowerCase() === compareIndex.key) {
                                return true;
                            } else {
                                return false;
                            }
                        }
                    }
                    // We verified that the class exists, field is stereotyped with a valid MK/FK type and data is present

                    return true;

                } else {
                    // This is starting from an index object; candidate is a data object.  We need to see if the candidate attribute
                    // has a stereotype and is not null or empty
                    // ( FROM MK/FK )
                    // 1. Get the field stereotype and value
                    if (candidate.data[qualifier.value] && candidate.data[qualifier.value].toLowerCase() === compareIndex.key) {
                        return true;
                    } else {
                        return false;
                    }
                    /*
                    let classDef = this.hive.HiveClasses[candidate.data['_objClass']];
                    if (!classDef) {
                        return false;
                    }
                    let attrDef = classDef.Attributes[qualifier.value];
                    if (!attrDef) {
                        return false;
                    }
                    let sTypeName = attrDef['Stereotype'];
                    if (!sTypeName) {
                        return false;
                    }
                    if (!this.hive.HiveIndexes[sTypeName]) {
                        return false;
                    }
                    if (!candidate.data[qualifier.value]) {
                        return false;
                    }
                    */
                }

            //break;
            default:

        }
        return returnVal;
    }

    ParseQualifiers(qualifierText) {
        // Like JSON, the qualifiers should be "field:val" and comma separated
        let parsedQualifierList = [];
        if (qualifierText) {
            let rawQualifierList = qualifierText.split(',');
            for (let i = 0; i < rawQualifierList.length; i++) {
                let qualifierTypeValue = rawQualifierList[i].split(':');
                parsedQualifierList.push({ type: qualifierTypeValue[0], value: qualifierTypeValue[1] });
            }
        }
        return parsedQualifierList;
    }

    GetIndexObj(objectKeyType, objectKey) {
        let returnObj = null;
        if (this.hive.HiveIndexes[objectKeyType].IndexRecords[objectKey]) {
            returnObj = this.hive.HiveIndexes[objectKeyType].IndexRecords[objectKey];
        }
        return returnObj;
    }
}

class CortexObjectManager {
    /**
     * 
     * @param {Cortex} CortexServer Cortex
     * @param {string} ObjectManagerName Object Manager Name
     */
    constructor(CortexServer, ObjectManagerName) {
        let thisObjMgr = this;

        // Set CortexServer
        thisObjMgr.CortexServer = CortexServer;
        thisObjMgr.subscribeTo = [];
        thisObjMgr.ManagedObjects = {};
        thisObjMgr.InitializeManagedObjects();
        thisObjMgr.Name = ObjectManagerName;
        //thisObjMgr.ManagedObjectTypes = [];
        //thisObjMgr.SanityChecks = {};
        //thisObjMgr.OnObjectUpdate = {};
    }

    // Stub Start function
    Start() { }

    // Stub ReceiveMonitorPacket
    ReceiveMonitorPacket(packetObj) {
        // N/A
    }

    SendJSONCmd(conn, cmd, data) {
        conn.send(JSON.stringify({
            'cmd': cmd,
            'data': data
        }));
    }

    InitializeManagedObjects() {
        let thisObjMgr = this;
        for (let i = 0; i < thisObjMgr.ManagedObjectTypes.length; i++) {

            // Get ObjectType
            let objectType = thisObjMgr.ManagedObjectTypes[i];

            // Initialize ManagedObject set
            thisObjMgr.ManagedObjects[objectType] = {};

            // Register Object Type Association
            thisObjMgr.CortexServer.ObjectTypes[objectType] = thisObjMgr;
        }
    }

    PopulateObjects(objectType, objectClass) {
        let thisObjMgr = this;
        let referenceObj = new objectClass();
        let keyStereotype = referenceObj.KeyStereotype;
        let keyList = thisObjMgr.CortexServer.Hive.GetIndexKeys(keyStereotype);
        for (let i = 0, l = keyList.length; i < l; i++) {

            // Get Key
            let key = keyList[i];

            // Create new object
            let newObject = new objectClass(key, thisObjMgr);
            if (newObject.key) {
                thisObjMgr.ManagedObjects[objectType][key] = newObject;
            }
        }
        thisObjMgr.CortexServer.DRPNode.log("Populating Objects - " + objectType + " [" + keyList.length + "]");
    }
}

Object.defineProperties(CortexObjectManager.prototype, {
    "ManagedObjectTypes": { get: function () { return []; } },
    "SanityChecks": { get: function () { return []; } }
});

module.exports = Cortex;
