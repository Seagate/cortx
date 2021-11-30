class UMLAttribute {
    /**
     * 
     * @param {string} name Attribute name
     * @param {string} stereotype Type of data contained
     * @param {string} visibility public|private
     * @param {bool} derived Is derived
     * @param {string} type string|int|bool
     * @param {object} defaultValue Set to this if unspecified
     * @param {string} multiplicity Number of allowed values
     * @param {string} restrictions PK,FK,MK
     */
    constructor(name, stereotype, visibility, derived, type, defaultValue, multiplicity, restrictions) {
        this.Name = name;
        this.Stereotype = stereotype;
        this.Visibility = visibility;
        this.Derived = derived;
        this.Type = type;
        this.Default = defaultValue;
        this.Multiplicity = multiplicity;
        this.Restrictions = restrictions;
    }
}

class UMLFunction {
    /**
     * 
     * @param {string} name Function name
     * @param {string} visibility public|private
     * @param {string[]} parameters Input parameters
     * @param {string} returnData Output value
     * @param {function} realFunction Actual function to execute
     */
    constructor(name, visibility, parameters, returnData, realFunction) {
        this.Name = name;
        this.Visibility = visibility;
        this.Parameters = parameters;
        this.Return = returnData;
        this.RealFunction = realFunction;
    }
}

class UMLDataObject {
    /**
     * 
     * @param {string} className UML Class name
     * @param {string} primaryKey Primary key value
     * @param {string} serviceName Source service Name
     * @param {string} snapTime Timestamp of acquisition from source
     */
    constructor(className, primaryKey, serviceName, snapTime) {
        this._objClass = className;
        this._objPK = primaryKey;
        this._serviceName = serviceName;
        this._snapTime = snapTime;
    }

    ToString() {
        return JSON.stringify(this);
    }
}

class UMLClass {
    /**
     * 
     * @param {string} name Class name
     * @param {string[]} stereotypes Stereotypes
     * @param {UMLAttribute[]} attributes Attributes
     * @param {UMLFunction[]} functions Functions
     */
    constructor(name, stereotypes, attributes, functions) {
        let thisClass = this;
        this.Name = name;
        this.Stereotypes = stereotypes;
        this.Attributes = {};
        attributes.map(item => { this.Attributes[item.Name] = item; });
        this.Functions = functions;
        this.PrimaryKey = null;
        this.query = null;
        /** @type {Object.<string,UMLDataObject} */
        this.cache = {};
        this.loadedCache = false;
        this.GetPK();

        this.GetRecords = this.GetRecords;
        this.GetDefinition = this.GetDefinition;
        this.AddRecord = this.AddRecord;
    }

    GetRecords() {
        let thisClass = this;
        // If we have records cached, return cache
        return thisClass.cache;
        // If not, query from source
    }

    GetDefinition() {
        let thisClass = this;
        return {
            "Name": thisClass.Name,
            "Stereotypes": thisClass.Stereotypes,
            "Attributes": thisClass.Attributes,
            "Functions": thisClass.Functions,
            "PrimaryKey": thisClass.PrimaryKey
        };
    }

    AddRecord(newRecordObj, serviceName, snapTime) {
        let thisClass = this;
        let tmpObj = null;
        if (newRecordObj.hasOwnProperty(thisClass.PrimaryKey)) {
            tmpObj = new UMLDataObject(thisClass.Name, null, serviceName, snapTime);
            Object.keys(thisClass.Attributes).map(item => { if (newRecordObj.hasOwnProperty(item)) tmpObj[item] = newRecordObj[item]; });
            tmpObj._objPK = tmpObj[thisClass.PrimaryKey];
            this.cache[tmpObj._objPK] = tmpObj;
        }
        return tmpObj;
    }

    GetPK() {
        let thisClass = this;

        let attributeKeys = Object.keys(thisClass.Attributes);
        for (let j = 0; j < attributeKeys.length; j++) {
            let thisAttribute = thisClass.Attributes[attributeKeys[j]];
            if (thisAttribute.Restrictions) {
                let keyArr = thisAttribute.Restrictions.split(",");
                // Loop over keys of attribute
                for (let k = 0; k < keyArr.length; k++) {
                    switch (keyArr[k]) {
                        case 'MK':
                            break;
                        case 'FK':
                            break;
                        case 'PK':
                            thisClass.PrimaryKey = thisAttribute.Name;
                            break;
                        default:
                            break;
                    }
                }
            }
        }
    }
}

module.exports = {
    Attribute: UMLAttribute,
    Function: UMLFunction,
    Class: UMLClass,
    DataObject: UMLDataObject
};
