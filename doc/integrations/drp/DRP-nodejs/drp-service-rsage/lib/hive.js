const DRP_Service = require('drp-mesh').Service;
const DRP_Node = require('drp-mesh').Node;

class Hive extends DRP_Service {
    /**
     * 
     * @param {string} serviceName Service Name
     * @param {DRP_Node} drpNode DRP Node
     * @param {number} priority Priority (lower better)
     * @param {number} weight Weight (higher better)
     * @param {string} scope Scope [local|zone|global(defaut)]
     */
    constructor(serviceName, drpNode, priority, weight, scope) {
        super(serviceName, drpNode, "Hive", null, false, priority, weight, drpNode.Zone, scope, null, null, 1);
        let thisHive = this;
        this.cfgOpts = {};
        this.HiveClasses = {};
        this.HiveData = {};
        this.HiveIndexes = {};

        this.CollectorInstances = {};

        this.Start = this.Start;

        this.ClientCmds = {
            // Old Hive client commands
            listStereoTypes: function (params) {
                //let thisHive = this;
                return Object.keys(thisHive.HiveIndexes);
                //console.log("Sent stereotype list");
            },
            listStereoTypeKeys: function (params) {
                //let thisHive = this;
                return Object.keys(thisHive.HiveIndexes[params['StereoType']].IndexRecords);
                //console.log("Sent keys for stereotype '" + appData['StereoType'] + "'");
            },
            getStereoTypeAssocObj: function (params) {
                //let thisHive = this;
                let returnObj = {};
                let checkKey = params['Key'];
                if (typeof checkKey === "string") {
                    let tmpString = checkKey.toLowerCase();
                    checkKey = tmpString;
                }
                //console.log("Getting stereotype " + params['StereoType'] + "[" + checkKey + "]");
                if (typeof thisHive.HiveIndexes[params['StereoType']] === "undefined" || typeof thisHive.HiveIndexes[params['StereoType']].IndexRecords[checkKey] === "undefined") {
                    //console.log("    ...no index for item");
                } else {
                    // console.log("    ...found index");
                    let assocRoot = thisHive.HiveIndexes[params['StereoType']].IndexRecords[checkKey];
                    returnObj = {
                        MK: "",
                        FK: []
                    };
                    if (assocRoot.MK !== '') {
                        returnObj.MK = assocRoot.MK.data;
                    }
                    Object.keys(assocRoot.FK).forEach(function (fkKey) {
                        returnObj.FK.push(assocRoot.FK[fkKey].data);
                    });

                    returnObj['Key'] = params['Key'];
                    returnObj['StereoType'] = params['StereoType'];
                    return returnObj;
                }
            },
            getParentAssocObj: function (params) {
                //let thisHive = this;
                let returnObj = {};
                let checkKey = params['Key'];
                if (typeof checkKey === "string") {
                    let tmpString = checkKey.toLowerCase();
                    checkKey = tmpString;
                }
                //console.log("Getting parents of " + appData['StereoType'] + "[" + checkKey + "]");
                if (typeof thisHive.HiveIndexes[params['StereoType']] === "undefined" || typeof thisHive.HiveIndexes[params['StereoType']].IndexRecords[checkKey] === "undefined") {
                    //console.log("    ...no index for item");
                } else {
                    //console.log("    ...found index");
                    let assocRoot = thisHive.HiveIndexes[params['StereoType']].IndexRecords[checkKey];
                    returnObj = {
                        UK: []
                    };
                    if (assocRoot.MK !== '' && assocRoot.MK.FK) {
                        Object.keys(assocRoot.MK.FK).forEach(function (fkKey) {
                            let assocParentRoot = assocRoot.MK.FK[fkKey];
                            if (assocParentRoot.MK !== '') {
                                returnObj.UK.push(assocParentRoot.MK.data);
                            }
                        });
                    }

                    returnObj['Key'] = appData['Key'];
                    returnObj['StereoType'] = appData['StereoType'];
                    return returnObj;
                }
            },
            getUKMKFKObj: function (params) {
                //let thisHive = this;
                let returnObj = {
                    params: params,
                    records: {},
                    err: null
                };
                let checkKey = null;
                let stereoType = null;

                if (params['Key'] && params['StereoType']) {
                    checkKey = params['Key'];
                    stereoType = params['StereoType'];
                }

                if (params['pathList'] && params['pathList'].length > 1) {
                    stereoType = params['pathList'][0];
                    checkKey = params['pathList'][1];
                }

                if (typeof checkKey === "string") {
                    let tmpString = checkKey.toLowerCase();
                    checkKey = tmpString;
                }

                //console.log("Getting UKMKFK " + appData['StereoType'] + "[" + checkKey + "]");

                if (!checkKey || !stereoType || typeof thisHive.HiveIndexes[stereoType] === "undefined" || typeof thisHive.HiveIndexes[stereoType].IndexRecords[checkKey] === "undefined") {
                    //console.log("    ...no index for item");
                    returnObj["err"] = `No index for (${stereoType})[${checkKey}]`;
                } else {
                    //console.log("    ...found index");
                    let assocRoot = thisHive.HiveIndexes[stereoType].IndexRecords[checkKey];
                    returnObj.records = {
                        UK: [],
                        MK: "",
                        FK: []
                    };
                    if (assocRoot.MK !== '') {
                        returnObj.records.MK = assocRoot.MK.data;
                        if (assocRoot.MK.FK) {
                            Object.keys(assocRoot.MK.FK).forEach(function (fkKey) {
                                let assocParentRoot = assocRoot.MK.FK[fkKey];
                                if (assocParentRoot.MK !== '') {
                                    returnObj.records.UK.push(assocParentRoot.MK.data);
                                }
                            });
                        }
                    }
                    Object.keys(assocRoot.FK).forEach(function (fkKey) {
                        returnObj.records.FK.push(assocRoot.FK[fkKey].data);
                    });

                    returnObj.records['Key'] = checkKey;
                    returnObj.records['StereoType'] = stereoType;
                }
                return returnObj;
            },
            getUKMKFKGCObj: function (params) {
                //let thisHive = this;
                let returnObj = {
                    params: params,
                    records: {},
                    err: null
                };

                let checkKey = null;
                let stereoType = null;

                if (params['Key'] && params['StereoType']) {
                    checkKey = params['Key'];
                    stereoType = params['StereoType'];
                }

                if (params['pathList'] && params['pathList'].length > 1) {
                    stereoType = params['pathList'][0];
                    checkKey = params['pathList'][1];
                }

                if (typeof checkKey === "string") {
                    let tmpString = checkKey.toLowerCase();
                    checkKey = tmpString;
                }
                //console.log("Getting UKMKFKGC " + appData['StereoType'] + "[" + checkKey + "]");
                if (!checkKey || !stereoType || typeof thisHive.HiveIndexes[stereoType] === "undefined" || typeof thisHive.HiveIndexes[stereoType].IndexRecords[checkKey] === "undefined") {
                    //console.log("    ...no index for item");
                    returnObj["err"] = `No index for (${stereoType})[${checkKey}]`;
                } else {
                    //console.log("    ...found index");
                    let assocRoot = thisHive.HiveIndexes[stereoType].IndexRecords[checkKey];
                    returnObj.records = {
                        UK: [],
                        MK: "",
                        FK: [],
                        GC: []
                    };
                    if (assocRoot.MK !== '') {
                        returnObj.records.MK = assocRoot.MK.data;
                        if (assocRoot.MK.FK) {
                            Object.keys(assocRoot.MK.FK).forEach(function (fkKey) {
                                let assocParentRoot = assocRoot.MK.FK[fkKey];
                                if (assocParentRoot.MK !== '') {
                                    returnObj.records.UK.push(assocParentRoot.MK.data);
                                }
                            });
                        }
                    }
                    Object.keys(assocRoot.FK).forEach(function (fkKey) {
                        returnObj.records.FK.push(assocRoot.FK[fkKey].data);
                        if (assocRoot.FK[fkKey].MK) {
                            Object.keys(assocRoot.FK[fkKey].MK).forEach(function (mkKey) {
                                let assocChildRoot = assocRoot.FK[fkKey].MK[mkKey];
                                Object.keys(assocChildRoot.FK).forEach(function (gcKey) {
                                    returnObj.records.GC.push(assocChildRoot.FK[gcKey].data);
                                });
                            });
                        }
                    });

                    returnObj.records['Key'] = checkKey;
                    returnObj.records['StereoType'] = stereoType;
                }
                return returnObj;
            },
            searchStereotypeKeys: function (params) {
                //let thisHive = this;
                let returnObj = {
                    StereoType: params["StereoType"],
                    Key: params['Key'],
                    records: []
                };

                let checkKey = params['Key'];
                let sTypeKey = params['Stereotype'];
                if (typeof checkKey === "string") {
                    let tmpString = checkKey.toLowerCase();
                    checkKey = tmpString;
                }

                let checkIndexList = [];
                if (sTypeKey && thisHive.HiveIndexes[sTypeKey]) {
                    checkIndexList.push(sTypeKey);
                } else {
                    Object.keys(thisHive.HiveIndexes).forEach(function (idxKey) {
                        checkIndexList.push(idxKey);
                    });
                }

                for (let i = 0; i < checkIndexList.length; i++) {
                    let checkIdxKey = checkIndexList[i];
                    if (thisHive.HiveIndexes[checkIdxKey].IndexRecords[checkKey]) {
                        let assocRoot = thisHive.HiveIndexes[checkIdxKey].IndexRecords[checkKey];
                        let matchObj = {
                            UK: [],
                            MK: "",
                            FK: [],
                            GC: []
                        };
                        if (assocRoot.MK !== '') {
                            matchObj.MK = assocRoot.MK.data;
                            if (assocRoot.MK.FK) {
                                Object.keys(assocRoot.MK.FK).forEach(function (fkKey) {
                                    let assocParentRoot = assocRoot.MK.FK[fkKey];
                                    if (assocParentRoot.MK !== '') {
                                        matchObj.UK.push(assocParentRoot.MK.data);
                                    }
                                });
                            }
                        }
                        Object.keys(assocRoot.FK).forEach(function (fkKey) {
                            matchObj.FK.push(assocRoot.FK[fkKey].data);
                            if (assocRoot.FK[fkKey].MK) {
                                Object.keys(assocRoot.FK[fkKey].MK).forEach(function (mkKey) {
                                    let assocChildRoot = assocRoot.FK[fkKey].MK[mkKey];
                                    Object.keys(assocChildRoot.FK).forEach(function (gcKey) {
                                        matchObj.GC.push(assocChildRoot.FK[gcKey].data);
                                    });
                                });
                            }
                        });

                        matchObj['Key'] = params['Key'];
                        matchObj['StereoType'] = sTypeKey;

                        returnObj.records.push(matchObj);
                    }
                }

                return returnObj;
            },
            searchStereotypeKeysNested: function (params) {
                //let thisHive = this;
                let returnObj = {
                    StereoType: params["StereoType"],
                    Key: params['Key'],
                    records: []
                };

                let checkKey = params['Key'];
                let sTypeKey = params['Stereotype'];
                if (typeof checkKey === "string") {
                    let tmpString = checkKey.toLowerCase();
                    checkKey = tmpString;
                }

                let checkIndexList = [];
                if (sTypeKey && thisHive.HiveIndexes[sTypeKey]) {
                    checkIndexList.push(sTypeKey);
                } else {
                    Object.keys(thisHive.HiveIndexes).forEach(function (idxKey) {
                        checkIndexList.push(idxKey);
                    });
                }

                for (let i = 0; i < checkIndexList.length; i++) {
                    let checkIdxKey = checkIndexList[i];
                    if (thisHive.HiveIndexes[checkIdxKey].IndexRecords[checkKey]) {
                        let assocRoot = thisHive.HiveIndexes[checkIdxKey].IndexRecords[checkKey];
                        let rootObj = {
                            'classType': null,
                            'data': null,
                            'children': [],
                            'parents': []
                        };
                        if (assocRoot.MK !== '') {
                            rootObj.classType = assocRoot.MK.classType;
                            rootObj.data = assocRoot.MK.data;
                            if (assocRoot.MK.FK) {
                                Object.keys(assocRoot.MK.FK).forEach(function (fkKey) {
                                    let assocParentRoot = assocRoot.MK.FK[fkKey];
                                    if (assocParentRoot.MK !== '') {
                                        let ukObj = {
                                            'classType': assocParentRoot.MK.classType,
                                            'data': assocParentRoot.MK.data,
                                            'children': [],
                                            'parents': []
                                        };
                                        rootObj.parents.push(ukObj);
                                    }
                                });
                            }
                        }
                        Object.keys(assocRoot.FK).forEach(function (fkKey) {
                            let fkObj = {
                                'classType': assocRoot.FK[fkKey].classType,
                                'data': assocRoot.FK[fkKey].data,
                                'children': []
                            };
                            if (assocRoot.FK[fkKey].MK) {
                                Object.keys(assocRoot.FK[fkKey].MK).forEach(function (mkKey) {
                                    let assocChildRoot = assocRoot.FK[fkKey].MK[mkKey];
                                    Object.keys(assocChildRoot.FK).forEach(function (gcKey) {
                                        let valFound = 0;
                                        for (let i = 0; i < fkObj.children.length; i++) {
                                            let tmpObj = fkObj.children[i];
                                            if (tmpObj.data === assocChildRoot.FK[gcKey].data) {
                                                valFound = 1;
                                            }
                                        }
                                        if (!valFound) {
                                            let gcObj = {
                                                'classType': assocChildRoot.FK[gcKey].classType,
                                                'data': assocChildRoot.FK[gcKey].data,
                                                'children': [],
                                                'parents': []
                                            };
                                            fkObj.children.push(gcObj);
                                        }
                                    });
                                });
                            }
                            rootObj.children.push(fkObj);
                        });

                        rootObj['Key'] = params['Key'];
                        rootObj['StereoType'] = sTypeKey;

                        returnObj.records.push(rootObj);
                    }
                }
                return returnObj;
            },
            searchStereotypeKeysChildren: function (params) {
                //let thisHive = this;
                let returnObj = {
                    StereoType: params["StereoType"],
                    Key: params['Key'],
                    records: []
                };

                let checkKey = params['Key'];
                let sTypeKey = params['Stereotype'];
                if (typeof checkKey === "string") {
                    let tmpString = checkKey.toLowerCase();
                    checkKey = tmpString;
                }

                let checkIndexList = [];
                if (sTypeKey && thisHive.HiveIndexes[sTypeKey]) {
                    checkIndexList.push(sTypeKey);
                } else {
                    Object.keys(thisHive.HiveIndexes).forEach(function (idxKey) {
                        checkIndexList.push(idxKey);
                    });
                }

                for (let i = 0; i < checkIndexList.length; i++) {
                    let checkIdxKey = checkIndexList[i];
                    if (thisHive.HiveIndexes[checkIdxKey].IndexRecords[checkKey]) {
                        let assocRoot = thisHive.HiveIndexes[checkIdxKey].IndexRecords[checkKey];
                        let rootObj = {
                            'classType': null,
                            'data': null,
                            'children': [],
                            'parents': []
                        };
                        if (assocRoot.MK !== '') {
                            rootObj['classType'] = assocRoot.MK.classType;
                            rootObj['data'] = assocRoot.MK.data;
                            if (assocRoot.MK.FK) {
                                Object.keys(assocRoot.MK.FK).forEach(function (fkKey) {
                                    let assocParentRoot = assocRoot.MK.FK[fkKey];
                                    if (assocParentRoot.MK !== '') {
                                        let ukObj = {
                                            'classType': assocParentRoot.MK.classType,
                                            'data': assocParentRoot.MK.data,
                                            'children': [],
                                            'parents': []
                                        };
                                        rootObj.parents.push(ukObj);
                                    }
                                });
                            }
                        }
                        Object.keys(assocRoot.FK).forEach(function (fkKey) {
                            let fkObj = {
                                'classType': assocRoot.FK[fkKey].classType,
                                'data': assocRoot.FK[fkKey].data,
                                'children': []
                            };
                            rootObj.children.push(fkObj);
                        });

                        rootObj['Key'] = params['Key'];
                        rootObj['StereoType'] = sTypeKey;

                        returnObj.records.push(rootObj);
                    }
                }
                return returnObj;
            },
            listClassDataTypes: function (params) {
                //let thisHive = this;
                let returnObj = {};
                Object.keys(thisHive.HiveData).forEach(function (dataClass) {
                    returnObj[dataClass] = {
                        recCount: 0
                    };
                    Object.keys(thisHive.HiveData[dataClass]).forEach(function (collectorName) {
                        returnObj[dataClass].recCount = returnObj[dataClass].recCount + Object.keys(thisHive.HiveData[dataClass][collectorName].records).length;
                    });
                });
                return returnObj;
            },
            listClassDataKeys: function (params) {
                //let thisHive = this;
                let returnObj = {
                    ClassType: params['ClassType'],
                    records: []
                };
                if (thisHive.HiveData[params['ClassType']]) {
                    returnObj.records = Object.keys(thisHive.HiveData[params['ClassType']]);
                }

                //console.log("Sent keys for class '" + appData['ClassType'] + "'");
                return returnObj;
            },
            listClassDataObj: function (params) {
                //let thisHive = this;
                let returnObj = {
                    ClassType: params['ClassType'],
                    records: {}
                };
                if (typeof thisHive.HiveData[params['ClassType']] === "undefined") {
                    //console.log("No data list for class '" + appData['ClassType'] + "'");
                } else {
                    Object.keys(thisHive.HiveData[params['ClassType']]).forEach(function (collectorKey) {
                        Object.keys(thisHive.HiveData[params['ClassType']][collectorKey].records).forEach(function (recordKey) {
                            returnObj.records[recordKey] = thisHive.HiveData[params['ClassType']][collectorKey].records[recordKey].data;
                        });
                    });
                }

                return returnObj;
            },
            getClassDataObj: function (params) {
                //let thisHive = this;
                let returnObj = {
                    StereoType: params["ClassType"],
                    Key: params['Key'],
                    records: []
                };
                // Is this a valid ClassType?
                if (thisHive.HiveData[params['ClassType']]) {
                    // Loop over instances
                    Object.keys(thisHive.HiveData[params['ClassType']]).forEach(function (collectorKey) {
                        // Does this record exist?
                        if (thisHive.HiveData[params['ClassType']][collectorKey].records[params['Key']]) {
                            returnObj.records.push(thisHive.HiveData[params['ClassType']][collectorKey].records[params['Key']].data);
                        }
                    });
                }
                return returnObj;
            },
            getClassDataInstObj: function (params) {
                //let thisHive = this;
                let returnObj = {
                    objName: params['ClassType'],
                    instance: params['ClassInst'],
                    records: {}
                };
                returnObj.records = thisHive.HiveData[params['ClassType']][params['ClassInst']].records[params['Key']];
                return returnObj;
            },
            getClassDefinitions: function (params) {
                //let thisHive = this;
                return thisHive.HiveClasses;
            },
            runHiveQuery: function (params) {
                //let thisHive = this;
                let returnObj = {
                    query: params["query"],
                    records: []
                };
                //let icrQuery = new thisSvr.ICRQuery("LIST STEREOTYPE['ipAddress'] WHERE FK['SW.Nodes']['DeviceName'] = 'MEM0BIZDC04'");
                let icrQuery = new ICRQuery(params.query, thisHive);
                icrQuery.Run();
                if (icrQuery.errorStatus) {
                    //console.log("ICRQuery Error: [" + icrQuery.errorStatus + "]");
                    thisHive.DRPNode.log("ICRQuery Error: [" + icrQuery.errorStatus + "] --> " + params.query);
                } else {
                    returnObj.icrQuery = icrQuery.returnObj.icrQuery;
                    returnObj.records = icrQuery.returnObj.results;
                }
                return returnObj;
            },
            runMapQuery: function (params) {
                let mapQuery = new HiveMapQuery(thisHive, params.query, params.multiplicity);
                let mapQueryRunOutput = mapQuery.Run(params.keyStereotype, params.keyValue);
                let returnObj = {};
                if (mapQueryRunOutput.length > 0) {
                    returnObj = mapQueryRunOutput[0].data;
                }
                return returnObj;
            }
        };
    }

    async Start(callback) {
        let thisHive = this;

        this.HiveClasses = {};
        this.HiveData = {};
        this.HiveIndexes = {};
        thisHive.DRPNode.log(`Getting class definitions`);
        let serviceDefs = await thisHive.DRPNode.GetServiceDefinitions();
        thisHive.DRPNode.log(`Loading class definitions`);
        thisHive.HiveClasses = thisHive.LoadClasses(serviceDefs);
        thisHive.HiveIndexes = thisHive.GenerateIndexes(thisHive.HiveClasses);
        thisHive.DRPNode.log("Loaded class definitions");
        await thisHive.LoadClassRecords();
        thisHive.DRPNode.log("Loaded collector data");
        if (callback && typeof callback === "function") callback();
    }

    async LoadClassRecords() {
        /** @type {Hive} */
        let thisHive = this;

        let serviceDefs = await thisHive.DRPNode.GetServiceDefinitions();
        let serviceNames = Object.keys(serviceDefs);
        for (let i = 0; i < serviceNames.length; i++) {
            let serviceName = serviceNames[i];
            let serviceDefinition = serviceDefs[serviceName];

            let classNames = Object.keys(serviceDefinition.Classes);
            for (let j = 0; j < classNames.length; j++) {
                let className = classNames[j];

                let cmdResponse = await thisHive.DRPNode.GetClassRecords({ className: className, serviceName: serviceName});

                let classInstanceRecords = cmdResponse[serviceName];

                // Add data to Hive
                thisHive.DRPNode.log(`Retrieved [${className}] from ${serviceName}, Length: ${Object.keys(classInstanceRecords).length}`);
                let newRecordHash = {};
                Object.keys(classInstanceRecords).forEach(function (objPK) {
                    newRecordHash[objPK] = {
                        'classType': className,
                        'MK': [],
                        'FK': [],
                        'data': classInstanceRecords[objPK]
                    };
                    newRecordHash[objPK].data['_serviceName'] = serviceName;
                });
                // Add data to Hive
                if (!thisHive.HiveData[className]) {
                    thisHive.HiveData[className] = {};
                }
                thisHive.HiveData[className][serviceName] = {
                    'records': newRecordHash,
                    'classDef': thisHive.HiveClasses[className]
                };
                thisHive.BuildStereotypeIndexes_ClassDataInstance(thisHive.HiveData[className][serviceName]);
            }
        }
    }

    BuildStereotypeIndexes_ClassDataInstance(thisClassDataInstance) {
        let thisHive = this;

        // Get Class definition
        let thisClassDefinition = thisClassDataInstance.classDef;

        // Get list of records
        let recordKeyList = Object.keys(thisClassDataInstance.records);

        // Get hash of index fields with KeyType & Stereotype
        let indexKeyHash = thisHive.GetClassIndexKeys(thisClassDefinition);

        // Loop through all items in instance - add to index
        for (let m = 0; m < recordKeyList.length; m++) {

            // Get record reference
            let thisRecord = thisClassDataInstance.records[recordKeyList[m]];

            // Process index key fields
            thisHive.ProcessKeyFields(indexKeyHash, thisRecord);
        }
    }

    AddClassDataValue(collectorName, thisRecord) {
        let thisHive = this;

        // Class Type & Data Shortcuts
        let className = thisRecord.data['_objClass'];

        // Initialize HiveData[className] if it doesn't exist
        if (typeof thisHive.HiveData[className] === "undefined") {
            thisHive.HiveData[className] = {};
        }

        // Initialize HiveData[className][collectorName] if it doesn't exist
        if (typeof thisHive.HiveData[className][collectorName] === "undefined") {
            thisHive.HiveData[className][collectorName] = {
                'records': {},
                'classDef': thisHive.HiveClasses[className]
            };
        }

        let thisClassDataInstance = thisHive.HiveData[className][collectorName];

        // Get Class definition
        let thisClassDefinition = thisClassDataInstance.classDef;

        // Get PK value
        let pkFieldName = thisClassDefinition.PrimaryKey;
        let recordKey = thisRecord.data[pkFieldName];

        // Create Record Object
        thisClassDataInstance.records[recordKey] = thisRecord;

        // Get hash of index fields with KeyType & Stereotype
        let indexKeyHash = thisHive.GetClassIndexKeys(thisClassDefinition);

        // Process index key fields
        thisHive.ProcessKeyFields(indexKeyHash, thisRecord);
    }

    DeleteClassDataValue(csInst, csType, csRecordKey) {
        let thisHive = this;
        // If a data set has a field with MK, create...
        //  HiveIndexes[field stype].IndexRecords[value] = {MK:recordPtr, FK:[]}

        // Class Type & Data Shortcuts
        let thisClassType = thisHive.HiveClasses[csType];
        let thisClassData = thisHive.HiveData[csType];

        // Class Data Instance Shortcut
        let thisInst = thisClassData[csInst];

        // Get Record Object
        let thisRecord = thisInst.records[csRecordKey];
        if (!thisRecord)
            return null;

        // Remove references in Index objects
        for (let i = 0; i < thisRecord.MK.length; i++) {
            // Assign mkIdxObj and remove index
            let mkIdxObj = thisRecord.MK[i];
            mkIdxObj.MK = '';
        }

        for (let i = 0; i < thisRecord.FK.length; i++) {
            // Assign mkIdxObj and remove index
            let mkIdxObj = thisRecord.FK[i];
            for (let j = 0; j < mkIdxObj.FK.length; j++) {
                if (mkIdxObj.FK[j] === thisRecord) {
                    mkIdxObj.FK.splice(j, 1);
                }
            }
        }

        // Purge key/value pair from hash
        delete thisInst.records[csRecordKey];

        // 2018-01-31 : We need to add logic to handle Cortex object references pointing to this object!
    }

    GetClassIndexKeys(thisClassDefinition) {
        let thisHive = this;

        let fieldIndexKeys = {};

        // Loop over fields in class type definition - find MK,FK
        let attributeKeyList = Object.keys(thisClassDefinition.Attributes);
        for (let a = 0; a < attributeKeyList.length; a++) {

            // Get attribute key
            let attrKey = attributeKeyList[a];

            // Assign values as necessary
            let thisAttrDef = thisClassDefinition.Attributes[attrKey];
            if (thisAttrDef.Restrictions) {
                let keyArr = thisAttrDef.Restrictions.split(",");
                for (let k = 0; k < keyArr.length; k++) {
                    switch (keyArr[k]) {
                        case 'MK':
                            fieldIndexKeys[attrKey] = {
                                'KeyType': 'MK',
                                'Stereotype': thisAttrDef.Stereotype
                            };
                            break;
                        case 'FK':
                            fieldIndexKeys[attrKey] = {
                                'KeyType': 'FK',
                                'Stereotype': thisAttrDef.Stereotype
                            };
                            break;
                        case 'PK':
                            // Special case - this IS the object's array key
                            break;
                        default:
                            break;
                    }
                }
            }
        }

        return fieldIndexKeys;
    }

    ProcessKeyFields(indexKeyHash, thisRecord) {
        let thisHive = this;

        let indexKeyList = Object.keys(indexKeyHash);

        // Loop over fields in class type definition that are MK or FK
        for (let a = 0; a < indexKeyList.length; a++) {

            // Get attribute name
            let attrKey = indexKeyList[a];

            // Get KeyType & Stereotype
            let attrKeyDef = indexKeyHash[attrKey];

            // Get Field Value
            let fieldValue = thisRecord.data[attrKey];

            // Process
            if (toString.call(fieldValue) === '[object Array]') {
                for (let q = 0; q < fieldValue.length; q++) {
                    thisHive.ProcessKeyField(attrKeyDef.KeyType, thisRecord, attrKeyDef.Stereotype, fieldValue[q]);
                }
            } else {
                thisHive.ProcessKeyField(attrKeyDef.KeyType, thisRecord, attrKeyDef.Stereotype, fieldValue);
            }

        }
    }

    ProcessKeyField(keyType, recordRef, sTypeOfField, fieldValue) {
        let thisHive = this;

        if (fieldValue === null)
            return;

        // Check string values; reject if empty or set to literal 'null'
        if (typeof fieldValue === "string") {
            let tmpString = fieldValue.toLowerCase();
            fieldValue = tmpString;
            if (fieldValue === 'null' || fieldValue === '')
                return;
        }

        // See if the index key already exists
        if (typeof thisHive.HiveIndexes[sTypeOfField].IndexRecords[fieldValue] === "undefined") {
            // New index key; let's set the base attributes
            thisHive.HiveIndexes[sTypeOfField].IndexRecords[fieldValue] = {
                key: fieldValue,
                sType: sTypeOfField,
                MK: '',
                FK: []
            };
        }

        switch (keyType) {
            case 'MK':
                thisHive.HiveIndexes[sTypeOfField].IndexRecords[fieldValue].MK = recordRef;
                recordRef.MK.push(thisHive.HiveIndexes[sTypeOfField].IndexRecords[fieldValue]);
                break;
            case 'FK':
                thisHive.HiveIndexes[sTypeOfField].IndexRecords[fieldValue].FK.push(recordRef);
                recordRef.FK.push(thisHive.HiveIndexes[sTypeOfField].IndexRecords[fieldValue]);
                break;
        }

    }

    GetIndexKeys(sTypeOfField) {
        let thisHive = this;
        let returnVal = null;
        if (typeof thisHive.HiveIndexes[sTypeOfField] === "undefined") {
            returnVal = [];
        } else {
            returnVal = Object.keys(thisHive.HiveIndexes[sTypeOfField].IndexRecords);
        }
        return returnVal;
    }

    GetIndexEntry(sTypeOfField, key) {
        let thisHive = this;
        let returnVal = null;
        if (typeof thisHive.HiveIndexes[sTypeOfField] !== "undefined" && typeof thisHive.HiveIndexes[sTypeOfField].IndexRecords[key] !== "undefined") {
            returnVal = thisHive.HiveIndexes[sTypeOfField].IndexRecords[key];
        }
        return returnVal;
    }

    GetIndexEntries(sTypeOfField) {
        let thisHive = this;
        let returnVal = null;
        if (typeof thisHive.HiveIndexes[sTypeOfField] !== "undefined") {
            returnVal = thisHive.HiveIndexes[sTypeOfField].IndexRecords;
        }
        return returnVal;
    }

    LoadClasses(serviceDefinitions) {
        // Translate UMLClasses objects
        // We used to get a list of umlClasses; now we're getting Service Definitions
        let HiveClasses = {};

        // Loop over services
        let serviceNames = Object.keys(serviceDefinitions);
        for (let i = 0; i < serviceNames.length; i++) {
            // Loop over classes in service
            let serviceName = serviceNames[i];
            let serviceDefinition = serviceDefinitions[serviceName];
            let classNames = Object.keys(serviceDefinition.Classes);
            for (let j = 0; j < classNames.length; j++) {
                let className = classNames[j];
                let classDefinition = serviceDefinition.Classes[className];
                if (!HiveClasses[className]) HiveClasses[className] = classDefinition;
            }
        }
        return HiveClasses;
    }

    GenerateIndexes(HiveClasses) {
        // Get Index structure
        let HiveIndexes = {};
        let classKeys = Object.keys(HiveClasses);
        for (let i = 0; i < classKeys.length; i++) {
            let thisClass = HiveClasses[classKeys[i]];
            let attributeKeys = Object.keys(thisClass.Attributes);
            for (let j = 0; j < attributeKeys.length; j++) {
                let thisAttrDef = thisClass.Attributes[attributeKeys[j]];
                if (thisAttrDef.Restrictions) {
                    let keyArr = thisAttrDef.Restrictions.split(",");
                    // Loop over keys of attribute
                    for (let k = 0; k < keyArr.length; k++) {
                        switch (keyArr[k]) {
                            case 'MK':
                                this.CheckIdxEntry(HiveIndexes, thisAttrDef.Stereotype);
                                HiveIndexes[thisAttrDef.Stereotype].MKAttributes.push(thisAttrDef);
                                break;
                            case 'FK':
                                this.CheckIdxEntry(HiveIndexes, thisAttrDef.Stereotype);
                                HiveIndexes[thisAttrDef.Stereotype].FKAttributes.push(thisAttrDef);
                                break;
                            case 'PK':
                                break;
                            default:
                                break;
                        }
                    }
                }
            }
        }
        return HiveIndexes;
    }

    CheckIdxEntry(thisIdx, key) {
        if (typeof thisIdx[key] === "undefined") {
            thisIdx[key] = {
                MKAttributes: [],
                FKAttributes: [],
                IndexRecords: {}
            };
        }
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

class ICRQuery {
    constructor(queryText, hive) {
        this.queryText = queryText;
        this.Hive = hive;
        this.cmdParts = {};
        this.queryFunc = {};
        this.targetObj = {};
        this.returnObj = {};
        this.collectorName = null;
    }
    Run() {
        let thisQueryObj = this;
        this.errorStatus = 0;
        this.cmdParts = {};
        let cmdArr = /^(\w+) (\w+)(?:\[\'([\w\.]+)\'\])?(?:\[\'([\w\.]+)\'\])?(?:\.(FK|MK)\[\'([\w\.]+)\'\])?(?:\.KEY)?(?:\[\'([\w\.]+)\'\])?(?: KEY (\'[\w]+\'))?(?: (WHERE) (KEY|FK|MK)(?:\[\'([\w\.]+)\'\])?(?:\[\'([\w\.\s]+)\'\])?(?:\s?(\!)?(EXISTS|LIKE|=) ?(\"?[\w\.\*\%\s\,\=\(\)\@\'\-\_]+\"?)?))?$/.exec(thisQueryObj.queryText);

        if (cmdArr) {
            this.cmdParts = {
                verb: cmdArr[1],
                objType: cmdArr[2],
                classType: cmdArr[3],
                classKey: cmdArr[4],
                precedence1: cmdArr[5],
                classTypeSub: cmdArr[6],
                classKeySub: cmdArr[7],
                keyVal: cmdArr[8],
                filterName: cmdArr[9],
                precedence2: cmdArr[10],
                tgtClassType: cmdArr[11],
                tgtClassField: cmdArr[12],
                negate: cmdArr[13],
                comparator: cmdArr[14],
                compVal: cmdArr[15]
            };

            if (this.cmdParts.compVal) {
                this.cmdParts.compVal = this.cmdParts.compVal.replace(/^['"]/g, "");
                this.cmdParts.compVal = this.cmdParts.compVal.replace(/['"]$/g, "");
            }

            // Check Verb
            let verbName = this.cmdParts.verb;
            if (this.Verbs.hasOwnProperty(verbName)) {
                this.queryFunc = this.Verbs[verbName];
            } else {
                this.errorStatus = 2;
                return;
            }

            // Check Object Type
            let objType = this.cmdParts.objType;
            if (this.ObjectTypes.hasOwnProperty(objType)) {
                this.dataStructReader = this.ObjectTypes[objType];
                this.dataStructReader();
            } else {
                this.errorStatus = 3;
                return;
            }

            // Add checks that validate the target object classtype and value keys if passed

            // Check target classType
            let classType = this.cmdParts.classType;
            if (typeof classType !== undefined) {
                //console.dir(this);
                if (this.objRoot.hasOwnProperty(classType)) {
                    this.rootSType = classType;
                    if (typeof this.cmdParts.classKey !== 'undefined') {
                        this.rootSTypeKey = this.cmdParts.classKey;
                    }
                } else {
                    this.errorStatus = 4;
                    return;
                }
            } else {
                // Missing classType - this means we simply want to list the object's classTypes
            }

            // Run Query
            this.queryFunc();

        } else {
            this.errorStatus = 1;
        }
    }

    MatchGetParts(thisDataKey, thisDataObj, evalVar, doEval) {
        let thisQueryObj = this;
        // This function evaluates the selection portion of queries...
        // LIST STEREOTYPE['ipAddress'].FK['IP.ProtDNS']['absoluteName'] {WHERE ...}
        let returnList = [];

        if (!doEval || thisQueryObj.Evaluate(evalVar, thisQueryObj.cmdParts.negate, thisQueryObj.cmdParts.comparator, thisQueryObj.cmdParts.compVal)) {
            // Are we specifying a subpart to get?
            if (thisQueryObj.cmdParts.precedence1) {
                // Yes - see if we want the key, MK or FK
                switch (thisQueryObj.cmdParts.precedence1) {
                    case 'KEY':
                        // Return only the KEY
                        returnList.push(thisDataKey);
                        break;
                    case 'MK':
                        // See if the MK matches the classTypeSub
                        let thisItemObj = thisDataObj['MK'];
                        if (thisItemObj && thisItemObj.data['_objClass'] === thisQueryObj.cmdParts.classTypeSub) {
                            let thisItemClass = thisItemObj.data['_objClass'];
                            let pkFieldName = thisQueryObj.Hive.HiveClasses[thisItemClass].PrimaryKey;
                            let pkValue = thisItemObj.data[pkFieldName];
                            let returnItem = {};
                            // See if we have a specific field to return
                            if (thisQueryObj.cmdParts.classKeySub) {
                                // Yes - return only one field
                                let dataFieldName = '_objClass';
                                if (thisQueryObj.Hive.HiveClasses[thisItemClass].Attributes[thisQueryObj.cmdParts.classKeySub]) {
                                    dataFieldName = thisQueryObj.cmdParts.classKeySub;
                                }
                                returnItem[pkValue] = thisItemObj.data[dataFieldName];
                            } else {
                                // No - return the whole object
                                returnItem[pkValue] = thisItemObj.data;
                            }
                            returnList.push(returnItem);
                        }
                        break;
                    case 'FK':
                        // See if the FK matches the classTypeSub
                        Object.keys(thisDataObj['FK']).forEach(function (itemKey2) {
                            let thisItemObj = thisDataObj['FK'][itemKey2];
                            let thisItemClass = thisItemObj.data['_objClass'];
                            if (thisItemClass === thisQueryObj.cmdParts.classTypeSub) {
                                let pkFieldName = thisQueryObj.Hive.HiveClasses[thisItemClass].PrimaryKey;
                                let pkValue = thisItemObj.data[pkFieldName];
                                let returnItem = {};
                                // See if we have a specific field to return
                                if (thisQueryObj.cmdParts.classKeySub) {
                                    // Yes - return only one field
                                    let thisDataClass = thisQueryObj.Hive.HiveData[thisItemClass];
                                    let dataFieldName = '_objClass';
                                    if (thisQueryObj.Hive.HiveClasses[thisItemClass].Attributes[thisQueryObj.cmdParts.classKeySub]) {
                                        dataFieldName = thisQueryObj.cmdParts.classKeySub;
                                    }
                                    returnItem[pkValue] = thisItemObj.data[dataFieldName];
                                } else {
                                    // No - return the whole object
                                    returnItem[pkValue] = thisItemObj.data;
                                }
                                returnList.push(returnItem);
                            }
                        });
                        break;
                    default:
                }
            } else {
                returnList.push(thisDataKey);
            }
        }
        return returnList;
    }

    //        this.CheckReturn = function () {
    //            // If the classTypeSub and tgtClassType are the same, let's return only matching items
    //        }

    Evaluate_Item(value, negate, comparator, compVal) {
        let thisQueryObj = this;
        //console.log("Evaluating: " + value);
        let returnVal = false;

        switch (comparator) {
            case '=':
                if (thisQueryObj.cmdParts.compVal === 'null') {
                    if (!value) {
                        returnVal = true;
                    }
                } else {
                    if (typeof compVal === "string" && typeof value === "string") {
                        if (compVal.toUpperCase() === value.toUpperCase()) {
                            returnVal = true;
                        }
                    } else if (compVal === value) {
                        returnVal = true;
                    }
                }
                break;
            case '>':
                if (compVal > value) {
                    returnVal = true;
                }
                break;
            case '<':
                if (compVal < value) {
                    returnVal = true;
                }
                break;
            case 'EXISTS':
                if (value) {
                    if (typeof value === "string") {
                        if (value !== "") {
                            returnVal = true;
                        }
                    } else if (value) {
                        returnVal = true;
                    }
                }
                break;
            case 'LIKE':
                if (value) {
                    if (typeof compVal === "string" && typeof value === "string") {
                        let tmpPattern = compVal.replace('*', '.*');
                        tmpPattern = tmpPattern.replace('+', '\\+');
                        let matchPattern = '^' + tmpPattern + '$';
                        let regexObj = new RegExp(matchPattern, "i");

                        if (value.match(regexObj)) {
                            returnVal = true;
                        }
                    }
                }
                break;
            default:
                break;
        }

        if (negate) {
            if (returnVal === true) {
                returnVal = false;
            } else {
                returnVal = true;
            }
        }

        return returnVal;
    }

    Evaluate(value, negate, comparator, compVal) {
        let thisQueryObj = this;
        let returnVal = false;
        if (toString.call(value) === '[object Array]') {
            for (let i = 0; i < value.length; i++) {
                if (thisQueryObj.Evaluate_Item(value[i], negate, comparator, compVal)) {
                    returnVal = true;
                }
            }

        } else {
            return thisQueryObj.Evaluate_Item(value, negate, comparator, compVal);
        }
        return returnVal;
    }
}

ICRQuery.prototype.ObjectTypes = {
    STEREOTYPE: function () {
        let thisQueryObj = this;
        this.objRoot = thisQueryObj.Hive.HiveIndexes;
        this.objRecs = 'IndexRecords';
        this.rootSType = null;
        this.rootSTypeKey = null;
        this.ListSTypes = function () {
            let tmpList = [];
            Object.keys(objRoot).forEach(function (itemKey) {
                tmpList.push(itemKey);
            });
            return tmpList;
        };
        this.ListObjectsInScope = function () {
            let tmpList = [];
            let objBase = this.objRoot[this.rootSType][this.objRecs];
            if (this.rootSTypeKey) {
                if (typeof this.rootSTypeKey === "string") {
                    let tmpString = this.rootSTypeKey.toLowerCase();
                    this.rootSTypeKey = tmpString;
                }
                let pushVal = {
                    'objKey': this.rootSTypeKey,
                    'objRef': objBase[this.rootSTypeKey]
                };
                tmpList.push(pushVal);
            } else {
                // Loop over items
                Object.keys(objBase).forEach(function (itemKey) {
                    let pushVal = {
                        'objKey': itemKey,
                        'objRef': objBase[itemKey]
                    };
                    tmpList.push(pushVal);
                });
            }
            return tmpList;
        };
    },
    CLASSDATA: function () {
        let thisQueryObj = this;
        this.objRoot = thisQueryObj.Hive.HiveData;
        this.objRecs = 'records';
        this.objBase = null;
        this.rootSType = null;
        this.rootSTypeKey = null;
        this.ListSTypes = function () {
            let tmpList = [];
            Object.keys(objRoot).forEach(function (itemKey) {
                tmpList.push(itemKey);
            });
            return tmpList;
        };
        this.ListObjectsInScope = function () {
            let tmpList = [];
            // Loop over Collectors
            Object.keys(objRoot[rootSType]).forEach(function (collectorKey) {
                objBase = objRoot[rootSType][collectorKey][objRecs];
                if (rootSTypeKey) {
                    if (typeof rootSTypeKey === "string") {
                        let tmpString = fieldValue.toLowerCase();
                        rootSTypeKey = tmpString;
                    }
                    let pushVal = {
                        'objKey': rootSTypeKey,
                        'objRef': objBase[rootSTypeKey]
                    };
                    if (objBase[rootSTypeKey]) {
                        tmpList.push(pushVal);
                    }
                } else {
                    // Loop over items
                    Object.keys(objBase).forEach(function (itemKey) {
                        let pushVal = {
                            'objKey': itemKey,
                            'objRef': objBase[itemKey]
                        };
                        tmpList.push(pushVal);
                    });
                }
            });
            return tmpList;
        };
    }
};

ICRQuery.prototype.Verbs = {
    LIST: function () {
        let thisQueryObj = this;
        //console.log("Running a LIST command!");
        //console.dir(this.cmdParts);

        // We'll be returning a list of variables; instantiate array
        let returnList = [];
        let tmpList = [];
        let whereScope = {};

        // See if we just need to list the keys of the object
        if (!this.rootSType) {
            this.returnObj = this.ListSTypes();
            return;
        }

        tmpList = this.ListObjectsInScope();

        // Do we have a WHERE clause? If so we need to search on that.  Loop over the target scope elements and test each.
        if (typeof thisQueryObj.cmdParts.filterName !== 'undefined' && typeof tmpList[0] !== 'undefined') {
            for (let i = 0; i < tmpList.length; i++) {
                // Set key
                let thisKey = tmpList[i]['objKey'];

                // Set object
                let thisObj = tmpList[i]['objRef'];

                // Check compare scope
                switch (thisQueryObj.cmdParts.precedence2) {
                    case 'KEY':
                        let evalVar = thisKey;
                        // RunBigMatch - WITH_EVAL
                        let tmpArr = thisQueryObj.MatchGetParts(thisKey, thisObj, evalVar, true);
                        if (tmpArr.length) {
                            let tmpItem = {};
                            tmpItem[thisKey] = tmpArr;
                            returnList.push(tmpItem);
                        }

                        break;
                    case 'MK':
                        if (thisObj['MK'] !== '' && thisObj['MK'].data['_objClass'] === thisQueryObj.cmdParts.tgtClassType) {
                            // Get class field number using class type
                            let collectorName = '';
                            Object.keys(thisQueryObj.Hive.HiveData[thisQueryObj.cmdParts.tgtClassType]).forEach(function (itemKey) {
                                collectorName = itemKey;
                            });
                            let tgtCollector = thisQueryObj.Hive.HiveData[thisQueryObj.cmdParts.tgtClassType][collectorName];
                            let evalVar = thisObj['MK'].data[thisQueryObj.cmdParts.tgtClassField];
                            // RunBigMatch - WITH_EVAL
                            let tmpArr = thisQueryObj.MatchGetParts(thisKey, thisObj, evalVar, true);
                            if (tmpArr.length) {
                                let tmpItem = {};
                                tmpItem[thisKey] = tmpArr;
                                returnList.push(tmpItem);
                            }
                        } else if (thisObj['MK'] === '' && !thisQueryObj.cmdParts.tgtClassType && thisQueryObj.cmdParts.compVal === 'null' && !thisQueryObj.cmdParts.negate) {
                            // Handle MK = null
                            let tmpArr = thisQueryObj.MatchGetParts(thisKey, thisObj, evalVar, false);
                            if (tmpArr.length) {
                                let tmpItem = {};
                                tmpItem[thisKey] = tmpArr;
                                returnList.push(tmpItem);
                            }
                        } else if (thisObj['MK'] !== '' && !thisQueryObj.cmdParts.tgtClassType && thisQueryObj.cmdParts.compVal === 'null' && thisQueryObj.cmdParts.negate) {
                            // Handle MK = null
                            let tmpArr = thisQueryObj.MatchGetParts(thisKey, thisObj, evalVar, false);
                            if (tmpArr.length) {
                                let tmpItem = {};
                                tmpItem[thisKey] = tmpArr;
                                returnList.push(tmpItem);
                            }
                        } else {
                            let testVar = 0;
                        }
                        break;
                    case 'FK':
                        if (thisObj['FK'].length > 0) {
                            let thisKeyMatch = false;
                            Object.keys(thisObj['FK']).forEach(function (itemKey) {
                                if (thisKeyMatch === false && thisObj['FK'][itemKey].data['_objClass'] === thisQueryObj.cmdParts.tgtClassType) {
                                    let collectorName = '';
                                    Object.keys(thisQueryObj.Hive.HiveData[thisQueryObj.cmdParts.tgtClassType]).forEach(function (itemKey) {
                                        collectorName = itemKey;
                                    });
                                    let tgtCollector = thisQueryObj.Hive.HiveData[thisQueryObj.cmdParts.tgtClassType][collectorName];
                                    let evalVar = thisObj['FK'][itemKey].data[thisQueryObj.cmdParts.tgtClassField];
                                    // RunBigMatch - WITH_EVAL
                                    let tmpArr = thisQueryObj.MatchGetParts(thisKey, thisObj, evalVar, true);
                                    if (tmpArr.length) {
                                        let tmpItem = {};
                                        tmpItem[thisKey] = tmpArr;
                                        returnList.push(tmpItem);
                                    }
                                }
                            });
                        }
                        break;
                    default:
                        break;
                }
            }
        } else {
            // No WHERE clause
            for (let i = 0; i < tmpList.length; i++) {
                // Set key
                let thisKey = tmpList[i]['objKey'];

                // Set object
                let thisObj = tmpList[i]['objRef'];

                // RunBigMatch - WITHOUT_EVAL
                let tmpArr = thisQueryObj.MatchGetParts(thisKey, thisObj, null, false);
                if (tmpArr.length) {
                    let tmpItem = {};
                    tmpItem[thisKey] = tmpArr;
                    returnList.push(tmpItem);
                }
            }
        }

        // Finally, evaluate the classTypeSub and classKeySub.  If these are present we need to loop over the key result
        // set and find children matching these.
        //if (typeof this.cmdParts.classTypeSub !== undefined && typeof this.cmdParts.classKeySub !== undefined) { }

        this.returnObj = {
            'icrQuery': this.cmdParts,
            'results': returnList
        };
    }
};

module.exports = Hive;
