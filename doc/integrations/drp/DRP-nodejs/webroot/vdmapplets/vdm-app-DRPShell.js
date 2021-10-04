(class extends rSageApplet {
    constructor(appletProfile) {
        super(appletProfile);
        let myApp = this;

        // Dropdown menu items
        myApp.menu = {
        };

        myApp.menuSearch = {
            "searchEmptyPlaceholder": "Search...",
            "searchField": null
        };
        /*
        myApp.menuQuery = {
        "queryEmptyPlaceholder": "Query...",
        "queryField": null
        }
         */

        myApp.appFuncs = {
            ShowUploadDiv: () => {
                myApp.appVars.dropWindowDiv.style["background-color"] = myApp.appVars.normalbgcolor;
                myApp.appVars.dropWindowDiv.style["z-index"] = 3;
                myApp.appVars.dropWindowDiv.style["opacity"] = 0.7;
                myApp.appVars.dropWindowDiv.focus();
            },
            HideUploadDiv: () => {
                myApp.appVars.dropWindowDiv.style["background-color"] = myApp.appVars.normalbgcolor;
                myApp.appVars.dropWindowDiv.style["z-index"] = -1;
                myApp.appVars.dropWindowDiv.style["opacity"] = 0;
                myApp.appVars.term.focus();
            }
        };

        myApp.appVars = {
            aliases: {
                '?': 'help',
                'dir': 'ls',
                'gi': 'cat',
                'cls': 'clear',
                'quit': 'exit'
            },
            term: null,
            termDiv: null,
            shellVars: {},
            dropWindowDiv: null,
            uploadPendingPromise: null,
            normalbgcolor: '#FFF',
            hoverbgcolor: '#F88'
        };

        myApp.recvCmd = {
        };
    }

    async runStartup() {
        let myApp = this;

        /* JSONPath 0.8.0 - XPath for JSON
        *
        * Copyright (c) 2007 Stefan Goessner (goessner.net)
        * Licensed under the MIT (MIT-LICENSE.txt) licence.
        */
        let jsonPath = function (obj, expr, arg) {
            var P = {
                resultType: arg && arg.resultType || "VALUE",
                result: [],
                normalize: function (expr) {
                    var subx = [];
                    return expr.replace(/[\['](\??\(.*?\))[\]']/g, function ($0, $1) { return "[#" + (subx.push($1) - 1) + "]"; })
                        .replace(/'?\.'?|\['?/g, ";")
                        .replace(/;;;|;;/g, ";..;")
                        .replace(/;$|'?\]|'$/g, "")
                        .replace(/#([0-9]+)/g, function ($0, $1) { return subx[$1]; });
                },
                asPath: function (path) {
                    var x = path.split(";"), p = "$";
                    for (var i = 1, n = x.length; i < n; i++)
                        p += /^[0-9*]+$/.test(x[i]) ? ("[" + x[i] + "]") : ("['" + x[i] + "']");
                    return p;
                },
                store: function (p, v) {
                    if (p) P.result[P.result.length] = P.resultType == "PATH" ? P.asPath(p) : v;
                    return !!p;
                },
                trace: function (expr, val, path) {
                    if (expr) {
                        var x = expr.split(";"), loc = x.shift();
                        x = x.join(";");
                        if (val && val.hasOwnProperty(loc))
                            P.trace(x, val[loc], path + ";" + loc);
                        else if (loc === "*")
                            P.walk(loc, x, val, path, function (m, l, x, v, p) { P.trace(m + ";" + x, v, p); });
                        else if (loc === "..") {
                            P.trace(x, val, path);
                            P.walk(loc, x, val, path, function (m, l, x, v, p) { typeof v[m] === "object" && P.trace("..;" + x, v[m], p + ";" + m); });
                        }
                        else if (/,/.test(loc)) { // [name1,name2,...]
                            for (var s = loc.split(/'?,'?/), i = 0, n = s.length; i < n; i++)
                                P.trace(s[i] + ";" + x, val, path);
                        }
                        else if (/^\(.*?\)$/.test(loc)) // [(expr)]
                            P.trace(P.eval(loc, val, path.substr(path.lastIndexOf(";") + 1)) + ";" + x, val, path);
                        else if (/^\?\(.*?\)$/.test(loc)) // [?(expr)]
                            P.walk(loc, x, val, path, function (m, l, x, v, p) { if (P.eval(l.replace(/^\?\((.*?)\)$/, "$1"), v[m], m)) P.trace(m + ";" + x, v, p); });
                        else if (/^(-?[0-9]*):(-?[0-9]*):?([0-9]*)$/.test(loc)) // [start:end:step]  phyton slice syntax
                            P.slice(loc, x, val, path);
                    }
                    else
                        P.store(path, val);
                },
                walk: function (loc, expr, val, path, f) {
                    if (val instanceof Array) {
                        for (var i = 0, n = val.length; i < n; i++)
                            if (i in val)
                                f(i, loc, expr, val, path);
                    }
                    else if (typeof val === "object") {
                        for (var m in val)
                            if (val.hasOwnProperty(m))
                                f(m, loc, expr, val, path);
                    }
                },
                slice: function (loc, expr, val, path) {
                    if (val instanceof Array) {
                        var len = val.length, start = 0, end = len, step = 1;
                        loc.replace(/^(-?[0-9]*):(-?[0-9]*):?(-?[0-9]*)$/g, function ($0, $1, $2, $3) { start = parseInt($1 || start); end = parseInt($2 || end); step = parseInt($3 || step); });
                        start = (start < 0) ? Math.max(0, start + len) : Math.min(len, start);
                        end = (end < 0) ? Math.max(0, end + len) : Math.min(len, end);
                        for (var i = start; i < end; i += step)
                            P.trace(i + ";" + expr, val, path);
                    }
                },
                eval: function (x, _v, _vname) {
                    try { return $ && _v && eval(x.replace(/@/g, "_v")); }
                    catch (e) { throw new SyntaxError("jsonPath: " + e.message + ": " + x.replace(/@/g, "_v").replace(/\^/g, "_a")); }
                }
            };

            var $ = obj;
            if (expr && obj && (P.resultType == "VALUE" || P.resultType == "PATH")) {
                P.trace(P.normalize(expr).replace(/^\$;/, ""), obj, "$");
                return P.result.length ? P.result : false;
            }
        }

        let watchWindowApplet = {
            appletName: "TopicWatch",
            title: "Topic Watch",
            sizeX: 700,
            sizeY: 411,
            vdmSession: myApp.vdmSession,
            appletClass: (class extends rSageApplet {
                constructor(appletProfile, startupParams) {
                    super(appletProfile);
                    let watchApp = this;

                    // Override Title
                    watchApp.title += ` - ${startupParams.topicName} (${startupParams.scope})`;
                    if (startupParams.targetNodeID) {
                        watchApp.title += ` @ ${startupParams.targetNodeID}`;
                    }

                    // Prerequisites
                    watchApp.preReqs = [
                    ];

                    // Dropdown menu items
                    watchApp.menu = {
                        "Stream": {
                            "Stop": () => {
                                watchApp.appFuncs.stopStream();
                            },
                            "Start": () => {
                                watchApp.appFuncs.startStream();
                            }
                        },
                        "Output": {
                            "Objects single line": () => {
                                watchApp.appVars.objectsMultiLine = false;
                            },
                            "Objects formatted": () => {
                                watchApp.appVars.objectsMultiLine = true;
                            }
                        }
                    };

                    watchApp.appFuncs = {
                        startStream: () => {
                            let topicName = watchApp.appVars.startupParams.topicName;
                            let scope = watchApp.appVars.startupParams.scope;
                            let targetNodeID = watchApp.appVars.startupParams.targetNodeID;
                            watchApp.sendCmd_StreamHandler("DRP", "subscribe", { topicName: topicName, scope: scope, targetNodeID: targetNodeID }, (streamData) => {
                                if (typeof streamData.payload.Message === "string") {
                                    watchApp.appVars.term.write(`\x1B[94m[${streamData.payload.TimeStamp}] \x1B[97m${streamData.payload.Message}\x1B[0m\r\n`);
                                } else {
                                    if (watchApp.appVars.objectsMultiLine) {
                                        watchApp.appVars.term.write(`\x1B[94m[${streamData.payload.TimeStamp}] \x1B[92m${JSON.stringify(streamData.payload.Message, null, 4).replace(/\n/g, "\r\n")}\x1B[0m\r\n`);
                                    } else {
                                        watchApp.appVars.term.write(`\x1B[94m[${streamData.payload.TimeStamp}] \x1B[92m${JSON.stringify(streamData.payload.Message)}\x1B[0m\r\n`);
                                    }
                                }
                            });
                        },
                        stopStream: () => {
                            let thisStreamToken = watchApp.streamHandlerTokens.pop();
                            if (thisStreamToken) {
                                watchApp.sendCmd("DRP", "unsubscribe", { streamToken: thisStreamToken }, false);
                                watchApp.vdmSession.drpClient.DeleteReplyHandler(thisStreamToken);
                            }
                        }
                    };

                    watchApp.appVars = {
                        startupParams: startupParams,
                        objectsMultiLine: false
                    };

                    watchApp.recvCmd = {
                    };

                }

                runStartup() {
                    let watchApp = this;

                    watchApp.appVars.termDiv = watchApp.windowParts["data"];
                    watchApp.appVars.termDiv.style.backgroundColor = "black";
                    let term = new Terminal();
                    watchApp.appVars.term = term;
                    watchApp.appVars.fitaddon = new FitAddon.FitAddon();
                    term.loadAddon(watchApp.appVars.fitaddon);
                    term.open(watchApp.appVars.termDiv);
                    term.setOption('cursorBlink', true);
                    term.setOption('bellStyle', 'sound');
                    //term.setOption('fontSize', 12);

                    watchApp.appFuncs.startStream();

                    watchApp.resizeMovingHook = function () {
                        watchApp.appVars.fitaddon.fit();
                    };

                    watchApp.appVars.fitaddon.fit();
                }
            })
        }

        class drpMethodSwitch {
            constructor(switchName, dataType, description) {
                this.switchName = switchName;
                this.dataType = dataType;
                this.description = description;
            }
        }

        class drpMethod {
            /**
             * 
             * @param {string} name
             * @param {string} description
             * @param {Object.<string,drpMethodSwitch>} switches
             * @param {Function} execute
             */
            constructor(name, description, usage, switches, execute) {
                this.name = name;
                this.description = description || '';
                this.usage = usage || '';
                this.switches = switches || {};
                this.execute = execute || (() => { })();
            }

            ShowHelp() {
                let output = `Usage: ${this.name} ${this.usage}\r\n`;
                output += `${this.description}\r\n\r\n`;
                output += "Optional arguments:\r\n";
                let switchesKeys = Object.keys(this.switches);
                if (switchesKeys.length > 0) {
                    for (let i = 0; i < switchesKeys.length; i++) {
                        output += `  -${switchesKeys[i]}\t${this.switches[switchesKeys[i]].description}\r\n`;
                    }
                } else {
                    output += "  (none)\r\n";
                }
                return output;
            }

            EvaluateStringForVariables(evalString) {
                // If the string matches a single variable, return that first - necessary for objects
                // Otherwise we'll need to evalute as a concatenated string
                if (!evalString) return evalString;

                // Remove leading and trailing whitespace
                evalString = evalString.replace(/^\s+|\s+$/g, '');
                if (!evalString) return evalString;

                /** Single Variable Match */
                let singleVarRegEx = /^\$(\w+)$/;
                let singleVarMatch = evalString.match(singleVarRegEx);
                if (singleVarMatch) {
                    let varName = singleVarMatch[1];
                    let replaceValue = "";
                    if (myApp.appVars.shellVars[varName]) {
                        replaceValue = myApp.appVars.shellVars[varName];
                    }
                    return replaceValue;
                }

                /** Multiple Variable Match */
                let envVarRegEx = /\$(\w+)/g;
                let envVarMatch;
                while (envVarMatch = envVarRegEx.exec(evalString)) {
                    let varName = envVarMatch[1];
                    let replaceValue = "";
                    // Does the variable exist?
                    if (myApp.appVars.shellVars[varName]) {
                        let varValue = myApp.appVars.shellVars[varName];
                        let varType = typeof varValue;
                        if (varType === "object" || ((varType === "string") && (varValue.match(/\n/)))) {
                            // Don't actually replace the variable
                            replaceValue = '$' + varName;
                        } else {
                            // Replace with contents of the variable
                            replaceValue = myApp.appVars.shellVars[varName];
                        }
                    }
                    evalString = evalString.replace('$' + varName, replaceValue);
                }

                return evalString;
            }

            ParseSwitchesAndData(switchesAndData, skipVarEval) {
                let returnObj = {
                    switches: {},
                    data: ""
                }
                if (!switchesAndData) return returnObj;
                // Built regex
                /**
                 * 1. Define empty array for switch regex patterns
                 * 2. Iterate over switches, add switch regex to array
                 * 3. Join with OR into string
                 * 4. Add to template
                 * 5. Evaluate
                 **/

                /** List containing  */
                let switchDataRegExList = [];
                if (this.switches) {
                    let switchList = Object.keys(this.switches);
                    for (let i = 0; i < switchList.length; i++) {
                        let thisSwitchDataRegEx;
                        let thisParameter = this.switches[switchList[i]];
                        if (thisParameter.dataType) {
                            thisSwitchDataRegEx = `(?: ?-(?:${thisParameter.switchName}) (?:(?:".*?")|(?:'.*?')|(?:[^-][^ ?]*)))`
                        } else {
                            thisSwitchDataRegEx = `(?: ?-(?:${thisParameter.switchName}))`
                        }
                        switchDataRegExList.push(thisSwitchDataRegEx);
                    }
                }
                let switchDataRegEx = new RegExp('^((?:' + switchDataRegExList.join('|') + ')*)?(?: ?([^-].*))?$');
                try {
                    let switchRegEx = / ?-(\w)(?: ((?:".*?")|(?:'.*?')|(?:[^-][^ ?]*)))?/g;
                    let switchDataMatch = switchesAndData.match(switchDataRegEx);
                    if (switchDataMatch) {
                        let switchHash = {};
                        let switchMatch;
                        while (switchMatch = switchRegEx.exec(switchDataMatch[1])) {
                            let varName = switchMatch[1];
                            let varValue = switchMatch[2] || null;
                            if (skipVarEval) {
                                switchHash[varName] = varValue;
                            } else {
                                switchHash[varName] = this.EvaluateStringForVariables(varValue);
                            }
                        }
                        returnObj.switches = switchHash;
                        if (skipVarEval) {
                            returnObj.data = switchDataMatch[2] || "";
                        } else {
                            returnObj.data = this.EvaluateStringForVariables(switchDataMatch[2]) || "";
                        }
                    }
                } catch (ex) {
                    let ted = 1;
                }
                return returnObj;
            }
        }

        class drpShell {
            constructor(vdmApp, term) {
                let thisShell = this;

                /** @type VDMApplet */
                this.vdmApp = vdmApp;
                /** @type Terminal */
                this.term = term;
                /** @type Object.<string,drpMethod> */
                this.drpMethods = {};

                this.AddMethod(new drpMethod("help",
                    "Show available commands", // Description
                    null,  // Usage
                    null,  // Switches
                    async function (switchesAndDataString, doPipeOut, pipeDataIn) {
                        term.write(this.ShowHelp());
                        return
                    }));

                this.drpMethods["help"].ShowHelp = function () {
                    let output = "";
                    let methodList = Object.keys(thisShell.drpMethods);
                    methodList.forEach(thisCmd => {
                        output += `\x1B[92m  ${thisCmd.padEnd(16)}\x1B[0m \x1B[94m${thisShell.drpMethods[thisCmd].description}\x1B[0m\r\n`;
                    });
                    return output;
                }

                this.AddMethod(new drpMethod("clear",
                    "Clear screen",
                    null,
                    null,
                    async function (switchesAndDataString, doPipeOut, pipeDataIn) {
                        let switchesAndData = this.ParseSwitchesAndData(switchesAndDataString);
                        term.clear();
                    }));

                this.AddMethod(new drpMethod("exit",
                    "Exit DRP Shell",
                    null,
                    null,
                    async function (switchesAndDataString, doPipeOut, pipeDataIn) {
                        let switchesAndData = this.ParseSwitchesAndData(switchesAndDataString);
                        vdmApp.vdmDesktop.closeWindow(myApp);
                    }));

                this.AddMethod(new drpMethod("ls",
                    "List path contents",
                    "[OPTIONS]... [PATH]",
                    { "h": new drpMethodSwitch("h", null, "Help") },
                    async function (switchesAndDataString, doPipeOut, pipeDataIn) {
                        let switchesAndData = this.ParseSwitchesAndData(switchesAndDataString);
                        if ("h" in switchesAndData.switches) {
                            term.write(this.ShowHelp());
                            return
                        }

                        let returnObj = "";
                        let dataOut = null;
                        let pathList = [];
                        if (switchesAndData.data.length > 0) pathList = switchesAndData.data.split(/[\/\\]/g);

                        let namePadSize = 0;
                        let typePadSize = 0;

                        // Remove leading empty entries
                        while (pathList.length > 0 && pathList[0] === "") pathList.shift();

                        // Remove trailing empty entries
                        while (pathList.length > 0 && pathList[pathList.length - 1] === "") pathList.pop();

                        let results = await myApp.sendCmd("DRP", "pathCmd", { pathList: pathList, listOnly: true }, true);
                        if (results && results.err) {
                            term.write(`\x1B[91m${results.err}\x1B[0m`);
                            return;
                        }
                        if (results && results.pathItemList && results.pathItemList.length > 0) {
                            // First, iterate over all and get the max length of the Name and Type fields
                            for (let i = 0; i < results.pathItemList.length; i++) {
                                let entryObj = results.pathItemList[i];
                                if (entryObj.Name && (!namePadSize || entryObj.Name.length > namePadSize)) {
                                    namePadSize = entryObj.Name.length;
                                }
                                if (entryObj.Type && (!typePadSize || entryObj.Type.length > typePadSize)) {
                                    typePadSize = entryObj.Type.length;
                                }
                            }

                            // We have a directory listing
                            for (let i = 0; i < results.pathItemList.length; i++) {
                                let entryObj = results.pathItemList[i];
                                if (!entryObj.Name) {
                                    console.log("This entry could not be printed, has a null name");
                                    console.dir(entryObj);
                                    continue;
                                }
                                switch (entryObj.Type) {
                                    case null:
                                    case 'Boolean':
                                    case 'Number':
                                    case 'String':
                                        dataOut = `${entryObj.Name.padEnd(namePadSize)}\t${entryObj.Type ? entryObj.Type.padEnd(typePadSize) : "null".padEnd(16)}\t${entryObj.Value}`;
                                        if (doPipeOut) returnObj += dataOut + "\r\n";
                                        else term.write(`\x1B[0m${dataOut}\x1B[0m\r\n`);
                                        break;
                                    case 'Function':
                                    case 'AsyncFunction':
                                        dataOut = `${entryObj.Name.padEnd(namePadSize)}\t${entryObj.Type.padEnd(typePadSize)}`;
                                        if (doPipeOut) returnObj += dataOut + "\r\n";
                                        else term.write(`\x1B[92m${dataOut}\x1B[0m\r\n`);
                                        break;
                                    default:
                                        // Must be some sort of object
                                        dataOut = `${entryObj.Name.padEnd(namePadSize)}\t${entryObj.Type.padEnd(typePadSize)}\t${entryObj.Value}`;
                                        if (doPipeOut) returnObj += dataOut + "\r\n";
                                        else term.write(`\x1B[1;34m${dataOut}\x1B[0m\r\n`);
                                        break;
                                }
                            }
                        } else {
                            dataOut = `No results`;
                            if (doPipeOut) {
                                returnObj += dataOut;
                            } else {
                                term.write(`\x1B[91m${dataOut}\x1B[0m`);
                            }
                        }
                        return returnObj;
                    }));

                this.AddMethod(new drpMethod("cat",
                    "Get object from path",
                    "[PATH]",
                    { "h": new drpMethodSwitch("h", null, "Help") },
                    async function (switchesAndDataString, doPipeOut, pipeDataIn) {
                        let switchesAndData = this.ParseSwitchesAndData(switchesAndDataString);

                        let returnObj = null;

                        // If an object was passed in a variable, output and return
                        if (typeof switchesAndData.data === "object") {
                            if (doPipeOut) {
                                return switchesAndData.data;
                            } else {
                                term.write(`\x1B[0m${JSON.stringify(switchesAndData.data, null, 4).replace(/([^\r])\n/g, "$1\r\n")}\x1B[0m\r\n`);
                                return returnObj;
                            }
                        }

                        let pathList = [];
                        if (switchesAndData.data.length > 0) pathList = switchesAndData.data.split(/[\/\\]/g);

                        // Remove leading empty entries
                        while (pathList.length > 0 && pathList[0] === "") pathList.shift();

                        // Remove trailing empty entries
                        while (pathList.length > 0 && pathList[pathList.length - 1] === "") pathList.pop();

                        if ("h" in switchesAndData.switches || pathList.length === 0) {
                            term.write(this.ShowHelp());
                            return
                        }

                        let results = await myApp.sendCmd("DRP", "pathCmd", { pathList: pathList, listOnly: false }, true);
                        if (typeof results === "string") {
                            // Error
                            term.write(`\x1B[91m${results}\x1B[0m\r\n`);
                        } else if (results && results.pathItem) {
                            // Have pathItem
                            if (doPipeOut) returnObj = results.pathItem;
                            else {
                                if (typeof results.pathItem === "object") {
                                    term.write(`\x1B[0m${JSON.stringify(results.pathItem, null, 4).replace(/([^\r])\n/g, "$1\r\n")}\x1B[0m\r\n`);
                                } else if (typeof results.pathItem === "string") {
                                    term.write(`\x1B[0m${results.pathItem.replace(/([^\r])\n/g, "$1\r\n")}\x1B[0m\r\n`);
                                } else {
                                    term.write(`\x1B[0m${results.pathItem}\x1B[0m\r\n`);
                                }
                            }
                        } else {
                            if (doPipeOut) returnObj = results;
                            else term.write(`\x1B[0m${results}\x1B[0m\r\n`);
                        }
                        return returnObj;
                    }));

                this.AddMethod(new drpMethod("topology",
                    "Get mesh topology",
                    null,
                    {
                        "h": new drpMethodSwitch("h", null, "Help menu"),
                        "l": new drpMethodSwitch("l", null, "Retrieve topology logs from all mesh nodes")
                    },
                    async function (switchesAndDataString, doPipeOut, pipeDataIn) {
                        let switchesAndData = this.ParseSwitchesAndData(switchesAndDataString);
                        let returnObj = null;
                        let tmpResults = null;

                        if ("h" in switchesAndData.switches) {
                            term.write(this.ShowHelp());
                            return
                        }

                        if ("l" in switchesAndData.switches) {
                            // Get topology logs from each node in mesh
                            tmpResults = {};
                            let pathString = `Mesh/Nodes`;
                            let pathList = pathString.split(/[\/\\]/g);
                            let nodeListDir = await myApp.sendCmd("DRP", "pathCmd", { pathList: pathList, listOnly: true }, true);
                            if (nodeListDir && nodeListDir.pathItemList && nodeListDir.pathItemList.length > 0) {
                                for (let i = 0; i < nodeListDir.pathItemList.length; i++) {
                                    let entryObj = nodeListDir.pathItemList[i];
                                    if (!entryObj.Name) {
                                        console.log("This entry could not be printed, has a null name");
                                        console.dir(entryObj);
                                        continue;
                                    }
                                    let nodeID = entryObj.Name;
                                    pathString = `Mesh/Nodes/${nodeID}/DRPNode/TopicManager/Topics/TopologyTracker/History`;
                                    pathList = pathString.split(/[\/\\]/g);
                                    let nodeListGet = await myApp.sendCmd("DRP", "pathCmd", { pathList: pathList, listOnly: false }, true);
                                    tmpResults[nodeID] = nodeListGet.pathItem;
                                }
                            }
                        } else {
                            tmpResults = await myApp.sendCmd("DRP", "getTopology", null, true);
                        }
                        if (doPipeOut) returnObj = tmpResults;
                        else term.write(`\x1B[96m${JSON.stringify(tmpResults, null, 4).replace(/\n/g, "\r\n")}\x1B[0m\r\n`);

                        return returnObj;
                    }));

                this.AddMethod(new drpMethod("whoami",
                    "Get my info",
                    null,
                    null,
                    async function (switchesAndDataString, doPipeOut, pipeDataIn) {
                        let switchesAndData = this.ParseSwitchesAndData(switchesAndDataString);
                        let returnObj = null;
                        if (doPipeOut) {
                            returnObj = `UserName: ${myApp.appVars.UserInfo.UserName}`;
                            returnObj += `\r\nFullName: ${myApp.appVars.UserInfo.FullName}`
                            returnObj += `\r\n  Groups: ${myApp.appVars.UserInfo.Groups.join('\r\n          ')}`
                        } else {
                            term.write(`\x1B[33mUserName: \x1B[0m${myApp.appVars.UserInfo.UserName}`);
                            term.write(`\r\n\x1B[33mFullName: \x1B[0m${myApp.appVars.UserInfo.FullName}`);
                            term.write(`\r\n\x1B[33m  Groups: \x1B[0m${myApp.appVars.UserInfo.Groups.join('\r\n          ')}`);
                            term.write(`\r\n`);
                        }
                        return returnObj;
                    }));

                this.AddMethod(new drpMethod("token",
                    "Get session token",
                    null,
                    null,
                    async function (switchesAndDataString, doPipeOut, pipeDataIn) {
                        let switchesAndData = this.ParseSwitchesAndData(switchesAndDataString);
                        let returnObj = null;
                        if (doPipeOut) {
                            returnObj = `Token: ${myApp.appVars.UserInfo.Token}`;
                        } else {
                            term.write(`\x1B[33mToken: \x1B[0m${myApp.appVars.UserInfo.Token}`);
                            term.write(`\r\n`);
                        }
                        return returnObj;
                    }));

                this.AddMethod(new drpMethod("endpointid",
                    "Get session endpointid",
                    null,
                    null,
                    async function (switchesAndDataString, doPipeOut, pipeDataIn) {
                        let switchesAndData = this.ParseSwitchesAndData(switchesAndDataString);
                        let returnObj = null;
                        if (doPipeOut) {
                            returnObj = `EndpointID: ${myApp.appVars.EndpointID}`;
                        } else {
                            term.write(`\x1B[33mEndpointID: \x1B[0m${myApp.appVars.EndpointID}`);
                            term.write(`\r\n`);
                        }
                        return returnObj;
                    }));

                this.AddMethod(new drpMethod("download",
                    "Download piped contents as file",
                    "[FILENAME]",
                    null,
                    async function (switchesAndDataString, doPipeOut, pipeDataIn) {
                        let switchesAndData = this.ParseSwitchesAndData(switchesAndDataString);
                        term.write(`\x1B[33mDownloading output\x1B[0m`);
                        term.write(`\r\n`);
                        let downloadFileName = "download.txt";
                        if (switchesAndData.data) downloadFileName = switchesAndData.data;
                        var pom = document.createElement('a');
                        let downloadData = null;
                        if (typeof pipeDataIn === 'string') {
                            downloadData = pipeDataIn;
                        } else {
                            downloadData = JSON.stringify(pipeDataIn, null, 2);
                        }
                        pom.setAttribute('href', 'data:application/xml;charset=utf-8,' + encodeURIComponent(downloadData));
                        pom.setAttribute('download', downloadFileName);
                        pom.click();
                    }));

                this.AddMethod(new drpMethod("watch",
                    "Subscribe to topic name and output the data stream",
                    "[OPTIONS]... [STREAM]",
                    {
                        "s": new drpMethodSwitch("s", "string", "Scope [local(default)|zone|global]"),
                        "z": new drpMethodSwitch("z", null, "Switch scope to zone"),
                        "g": new drpMethodSwitch("g", null, "Switch scope to global"),
                        "n": new drpMethodSwitch("n", "string", "Target NodeID"),
                        "l": new drpMethodSwitch("l", null, "List available streams"),
                    },
                    async function (switchesAndDataString, doPipeOut, pipeDataIn) {
                        let switchesAndData = this.ParseSwitchesAndData(switchesAndDataString);
                        let output = "";
                        if ("l" in switchesAndData.switches) {

                            let headerLabels = ["Stream Name", "Service Instance", "Scope", "Zone"];
                            /*
                            let fieldMaxLengths = [];
                            for (let i = 0; i < headerLabels.length; i++) {
                                fieldMaxLengths.push(headerLabels[i]);
                            }
                            */

                            let headerLengths = Object.assign({}, ...headerLabels.map((x) => ({ [x]: x.length })));

                            let topologyData = await myApp.sendCmd("DRP", "getTopology", null, true);
                            let streamTable = {};
                            // Loop over nodes
                            let nodeList = Object.keys(topologyData);
                            for (let i = 0; i < nodeList.length; i++) {
                                let nodeEntry = topologyData[nodeList[i]];
                                let serviceList = Object.keys(nodeEntry.Services);
                                for (let j = 0; j < serviceList.length; j++) {
                                    let serviceEntry = nodeEntry.Services[serviceList[j]];
                                    for (let k = 0; k < serviceEntry.Streams.length; k++) {
                                        let thisStreamName = serviceEntry.Streams[k];
                                        if (!streamTable[thisStreamName]) {
                                            streamTable[thisStreamName] = [];
                                            headerLengths["Stream Name"] = Math.max(headerLengths["Stream Name"], thisStreamName.length);
                                        }
                                        headerLengths["Service Instance"] = Math.max(headerLengths["Service Instance"], serviceEntry.InstanceID.length);
                                        headerLengths["Scope"] = Math.max(headerLengths["Scope"], serviceEntry.Scope.length);
                                        headerLengths["Zone"] = Math.max(headerLengths["Zone"], serviceEntry.Scope.length);
                                        streamTable[thisStreamName].push(serviceEntry);
                                    }
                                }
                            }

                            //let headerColorCtrl = '\x1B[40;96m';
                            let headerColorCtrl = '\x1B[40;1;95m';
                            output+= `\r\n` +
                                `${headerColorCtrl}${headerLabels[0].padEnd(headerLengths[headerLabels[0]], ' ')}\x1B[0m ` +
                                `${headerColorCtrl}${headerLabels[1].padEnd(headerLengths[headerLabels[1]], ' ')}\x1B[0m ` +
                                `${headerColorCtrl}${headerLabels[2].padEnd(headerLengths[headerLabels[2]], ' ')}\x1B[0m ` +
                                `${headerColorCtrl}${headerLabels[3].padEnd(headerLengths[headerLabels[3]], ' ')}\x1B[0m` +
                                `\r\n`;

                            // Output stream list
                            let streamNameList = Object.keys(streamTable);
                            for (let i = 0; i < streamNameList.length; i++) {
                                let thisStreamName = streamNameList[i];
                                let thisServiceArray = streamTable[thisStreamName];
                                for (let j = 0; j < thisServiceArray.length; j++) {
                                    let thisServiceObj = thisServiceArray[j];
                                    let thisStreamNameText = thisStreamName.padEnd(headerLengths["Stream Name"]);
                                    let thisInstanceIDText = thisServiceObj.InstanceID.padEnd(headerLengths["Service Instance"]);
                                    let thisScopeText = thisServiceObj.Scope.padEnd(headerLengths["Scope"]);
                                    let thisZoneText = thisServiceObj.Zone;
                                    let scopeColor = "";
                                    switch (thisServiceObj.Scope) {
                                        case "local":
                                            scopeColor = "93";
                                            break;
                                        case "zone":
                                            //scopeColor = "94";
                                            scopeColor = "96";
                                            break;
                                        case "global":
                                            scopeColor = "92";
                                            break;
                                        default:
                                            scopeColor = "39";
                                    }
                                    output+= `\x1B[37m${thisStreamNameText} \x1B[0;37m${thisInstanceIDText}\x1B[0m \x1B[${scopeColor}m${thisScopeText}\x1B[0m ${thisZoneText}\r\n`;
                                }
                            }

                            if (doPipeOut) {
                                // Sanitize output by removing terminal control characters
                                output = output.replace(/\x1b\[\d{1,2}(?:;\d{1,2})*m/g, '');
                            } else {
                                term.write(output);
                            }

                            return output;
                        }

                        if (switchesAndData.data) {
                            // Open a new window and stream output
                            let topicName = switchesAndData.data;
                            let scope = switchesAndData.switches["s"] || "local";
                            let targetNodeID = switchesAndData.switches["n"] || null;

                            // If a specific NodeID is set, override the scope
                            if (targetNodeID) {
                                scope = "local";
                            } else {
                                if ("z" in switchesAndData.switches) {
                                    scope = "zone";
                                }

                                if ("g" in switchesAndData.switches) {
                                    scope = "global";
                                }
                            }

                            switch (scope) {
                                case "local":
                                case "zone":
                                case "global":
                                    break;
                                default:
                                    term.write(`\x1B[91mInvalid scope: ${scope}\x1B[0m\r\n`);
                                    term.write(`\x1B[91mSyntax: watch [-s local(default)|zone|global] {streamName}\x1B[0m\r\n`);
                                    return;
                            }

                            let newApp = new watchWindowApplet.appletClass(watchWindowApplet, { topicName: topicName, scope: scope, targetNodeID: targetNodeID });
                            await myApp.vdmDesktop.newWindow(newApp);
                            myApp.vdmDesktop.appletInstances[newApp.appletIndex] = newApp;

                            //term.write(`\x1B[33mSubscribed to stream ${topicName}\x1B[0m`);
                            term.write(`\x1B[33mOpened new window for streaming data\x1B[0m`);
                            term.write(`\r\n`);
                        } else {
                            term.write(this.ShowHelp());
                            return
                        }
                    }));

                this.AddMethod(new drpMethod("scrollback",
                    "Set or get terminal scrollback",
                    "[MAXLINES]",
                    null,
                    async function (switchesAndDataString, doPipeOut, pipeDataIn) {
                        let switchesAndData = this.ParseSwitchesAndData(switchesAndDataString);
                        let returnObj = null;
                        if (switchesAndData.data) {
                            myApp.appVars.term.setOption('scrollback', switchesAndData.data);
                            term.write(`\x1B[33mScrollback set to \x1B[0m${switchesAndData.data}\x1B[33m lines.`);
                            term.write(`\r\n`);
                        } else {
                            let scrollbackLinesCount = myApp.appVars.term.getOption('scrollback');
                            term.write(`\x1B[33mScrollback currently \x1B[0m${scrollbackLinesCount}\x1B[33m lines.`);
                            term.write(`\r\n`);
                        }
                        return returnObj;
                    }));

                this.AddMethod(new drpMethod("grep",
                    "Grep piped contents or path",
                    "[OPTIONS]...",
                    {
                        "h": new drpMethodSwitch("h", null, "Help"),
                        "i": new drpMethodSwitch("i", null, "Case Insensitive"),
                        "v": new drpMethodSwitch("v", null, "Select non-matching lines"),
                        "n": new drpMethodSwitch("n", null, "Output line number"),
                        "A": new drpMethodSwitch("A", "integer", "Print lines before match"),
                        "B": new drpMethodSwitch("B", "integer", "Print lines after match"),
                        "C": new drpMethodSwitch("C", "integer", "Print lines before and after match")
                    },
                    async function (switchesAndDataString, doPipeOut, pipeDataIn) {
                        let switchesAndData = this.ParseSwitchesAndData(switchesAndDataString);
                        let output = "";
                        let printingContext = false;
                        let switchA = Number.parseInt(switchesAndData.switches["A"]) || 0;
                        let switchB = Number.parseInt(switchesAndData.switches["B"]) || 0;
                        let switchC = Number.parseInt(switchesAndData.switches["C"]) || 0;
                        let contextLinesBefore = Math.max(switchA, switchC) || 0;
                        let contextLinesAfter = Math.max(switchB, switchC) || 0;

                        if (contextLinesBefore || contextLinesAfter) {
                            printingContext = true;
                        }

                        // Function to output line
                        let printLine = (inputLine, matchLine, lineNumber) => {
                            let returnLine = '';
                            let outputLineNumber = '';
                            if ("n" in switchesAndData.switches) {
                                outputLineNumber += `\x1b\[92m${lineNumber}\x1b\[0m`;
                                if (matchLine) {
                                    outputLineNumber += `\x1b\[94m:\x1b\[0m`;
                                } else {
                                    outputLineNumber += `\x1b\[94m-\x1b\[0m`;
                                }
                            }
                            if (matchLine) {
                                returnLine = `${outputLineNumber}\x1b\[93m${inputLine}\x1b\[0m\r\n`;
                            } else {
                                returnLine = `${outputLineNumber}\x1b\[97m${inputLine}\x1b\[0m\r\n`;
                            }
                            return returnLine;
                        }

                        // If any context switches specified, insert '--' between matching sections

                        let linesPrinted = [];
                        let grepData = null;
                        if ("h" in switchesAndData.switches || !pipeDataIn) {
                            term.write(this.ShowHelp());
                            return
                        }
                        if (typeof pipeDataIn === 'string') {
                            grepData = pipeDataIn;
                        } else {
                            grepData = JSON.stringify(pipeDataIn, null, 2);
                        }
                        let regexFlags = "";
                        if ("i" in switchesAndData.switches) regexFlags += "i";
                        let checkRegEx = new RegExp(switchesAndData.data, regexFlags);
                        let lineArray = grepData.split('\n')
                        for (let i = 0; i < lineArray.length; i++) {
                            let cleanLine = lineArray[i].replace('\r', '');
                            let lineMatches = checkRegEx.test(cleanLine);
                            let doInvert = ("v" in switchesAndData.switches);
                            if ((lineMatches && !doInvert) || (!lineMatches && doInvert)) {
                                // We need to print this line

                                if (printingContext && linesPrinted.length) {

                                    // Get the last line we printed
                                    let lastPrintedLine = linesPrinted[linesPrinted.length - 1];

                                    // Did we hit a break between sections?
                                    if (i > lastPrintedLine + 1) {

                                        // We've already printed a section, add a break
                                        output+= `\x1b\[94m--\x1b\[0m\r\n`;
                                    }
                                }

                                // Are we printing context before?
                                if (contextLinesBefore) {
                                    // Calculate the starting and ending lines
                                    let linesToFetch = Math.min(contextLinesBefore, i);
                                    for (let j = linesToFetch; j > 0; j--) {
                                        let targetLine = i - j;
                                        if (!linesPrinted.includes(targetLine)) {
                                            let cleanLine = lineArray[targetLine].replace('\r', '');
                                            output += printLine(cleanLine, false, targetLine);
                                            linesPrinted.push(targetLine);
                                        }
                                    }
                                }

                                // If we've already printed this line, skip
                                if (!linesPrinted.includes(i)) {
                                    output += printLine(cleanLine, true, i);
                                    linesPrinted.push(i);
                                }

                                // Are we printing context after?
                                if (contextLinesAfter) {
                                    // Calculate the starting and ending lines

                                    /**
                                     * 
                                     * length = 10
                                     * linesAfter = 5
                                     * i = 6 (7th line, only 3 left)
                                     * 
                                     * */

                                    let linesToFetch = Math.min(contextLinesAfter, lineArray.length - (i + 1));
                                    for (let j = 1; j <= linesToFetch; j++) {
                                        let targetLine = i + j;
                                        if (!linesPrinted.includes(targetLine)) {
                                            let cleanLine = lineArray[targetLine].replace('\r', '');
                                            // Need to eval; possible this is a matching line
                                            let matchingLine = checkRegEx.test(cleanLine);
                                            output += printLine(cleanLine, doPipeOut, matchingLine, targetLine);
                                            linesPrinted.push(targetLine);
                                        }
                                    }
                                }
                            }
                        }
                        output = output.replace(/\r\n$/, '');

                        if (doPipeOut) {
                            // Sanitize output by removing terminal control characters
                            output = output.replace(/\x1b\[\d{1,2}(?:;\d{1,2})*m/g, '');
                        } else {
                            term.write(output);
                        }

                        return output;
                    }));

                this.AddMethod(new drpMethod("head",
                    "Output first 10 lines of piped contents or path",
                    "[OPTIONS]...",
                    {
                        "h": new drpMethodSwitch("h", null, "Help"),
                        "n": new drpMethodSwitch("n", "integer", "Number of lines"),
                        //"c": new drpMethodSwitch("c", null, "Number of characters"),
                    },
                    async function (switchesAndDataString, doPipeOut, pipeDataIn) {
                        let switchesAndData = this.ParseSwitchesAndData(switchesAndDataString);
                        let output = "";
                        let headData = null;
                        if ("h" in switchesAndData.switches || (!switchesAndData.data && !pipeDataIn)) {
                            term.write(this.ShowHelp());
                            return
                        }
                        if (typeof pipeDataIn === 'string') {
                            headData = pipeDataIn;
                        } else {
                            headData = JSON.stringify(pipeDataIn, null, 2);
                        }
                        let lineArray = headData.split('\n');
                        let lineFetchCount = 10;
                        if (switchesAndData.switches["n"]) {
                            lineFetchCount = Number.parseInt(switchesAndData.switches["n"]);
                        } else {
                            let switchRegEx = /^-([\d]+)/;
                            let switchDataMatch = switchesAndDataString.match(switchRegEx);
                            if (switchDataMatch) {
                                lineFetchCount = switchDataMatch[1];
                            }
                        }

                        if (typeof lineFetchCount === "string") {
                            try {
                                lineFetchCount = Number.parseInt(lineFetchCount);
                            } catch (ex) {
                                term.write(this.ShowHelp());
                                return
                            }
                        }

                        if (lineArray.length < lineFetchCount) {
                            // There are fewer lines than we want to get
                            lineFetchCount = lineArray.length;
                        }
                        for (let i = 0; i < lineFetchCount; i++) {
                            let cleanLine = lineArray[i].replace('\r', '');
                            if (doPipeOut) {
                                output += `${cleanLine}\r\n`;
                            } else {
                                term.write(`${cleanLine}\r\n`);
                            }
                        }
                        return output;
                    }));


                this.AddMethod(new drpMethod("tail",
                    "Output last 10 lines of piped contents or path",
                    "[OPTIONS]...",
                    {
                        "h": new drpMethodSwitch("h", null, "Help"),
                        "n": new drpMethodSwitch("n", "integer", "Number of lines"),
                        //"c": new drpMethodSwitch("c", null, "Number of characters"),
                    },
                    async function (switchesAndDataString, doPipeOut, pipeDataIn) {
                        let switchesAndData = this.ParseSwitchesAndData(switchesAndDataString);
                        let output = "";
                        let tailData = null;
                        if ("h" in switchesAndData.switches || (!switchesAndData.data && !pipeDataIn)) {
                            term.write(this.ShowHelp());
                            return
                        }
                        if (typeof pipeDataIn === 'string') {
                            tailData = pipeDataIn;
                        } else {
                            tailData = JSON.stringify(pipeDataIn, null, 2);
                        }
                        let lineArray = tailData.split('\n');
                        let lineFetchCount = 10;
                        if (switchesAndData.switches["n"]) {
                            lineFetchCount = Number.parseInt(switchesAndData.switches["n"]);
                        } else {
                            let switchRegEx = /^-([\d]+)/;
                            let switchDataMatch = switchesAndDataString.match(switchRegEx);
                            if (switchDataMatch) {
                                lineFetchCount = switchDataMatch[1];
                            }
                        }

                        if (typeof lineFetchCount === "string") {
                            try {
                                lineFetchCount = Number.parseInt(lineFetchCount);
                            } catch (ex) {
                                term.write(this.ShowHelp());
                                return
                            }
                        }

                        let lineStart = 0;

                        if (lineArray.length < lineFetchCount) {
                            // There are fewer lines than we want to get
                            lineFetchCount = lineArray.length;
                        } else {
                            lineStart = lineArray.length - lineFetchCount;
                        }

                        for (let i = lineStart; i < lineFetchCount + lineStart; i++) {
                            let cleanLine = lineArray[i].replace('\r', '');
                            if (doPipeOut) {
                                output += `${cleanLine}\r\n`;
                            } else {
                                term.write(`${cleanLine}\r\n`);
                            }
                        }
                        return output;
                    }));

                this.AddMethod(new drpMethod("set",
                    "Set or list shell ENV variables",
                    "[VARIABLE]=[VALUE]",
                    { "h": new drpMethodSwitch("h", null, "Help menu") },
                    async function (switchesAndDataString, doPipeOut, pipeDataIn) {
                        let switchesAndData = this.ParseSwitchesAndData(switchesAndDataString);
                        let output = "";

                        if ("h" in switchesAndData.switches) {
                            term.write(this.ShowHelp());
                            return
                        }

                        if (switchesAndData.data) {
                            // The a parameter name (possibly value) is present
                            // Was a value passed as well?  If not, did we get pipeDataIn?
                            if (switchesAndData.data.indexOf('=') > 0) {
                                let varName = switchesAndData.data.substr(0, switchesAndData.data.indexOf('='));
                                let varValue = switchesAndData.data.substr(switchesAndData.data.indexOf('=') + 1);
                                myApp.appVars.shellVars[varName] = varValue;
                            } else {
                                let varName = switchesAndData.data;
                                if (pipeDataIn) {
                                    myApp.appVars.shellVars[varName] = pipeDataIn;
                                } else {
                                    delete myApp.appVars.shellVars[varName];
                                }
                            }
                        } else {
                            // No ENV variable name provided, list all variables and values
                            output += `\x1B[33mShell variables:\x1B[0m\r\n`;
                            let shellVarNames = Object.keys(myApp.appVars.shellVars);
                            for (let i = 0; i < shellVarNames.length; i++) {
                                let printVal = "";
                                let varValue = myApp.appVars.shellVars[shellVarNames[i]];
                                let varType = Object.prototype.toString.call(varValue).match(/^\[object (.*)\]$/)[1];

                                switch (varType) {
                                    case "Object":
                                        printVal = `[${varType}:${Object.keys(varValue).length}]`;
                                        break;
                                    case "Array":
                                        printVal = `[${varType}:${varValue.length}]`;
                                        break;
                                    case "Set":
                                        printVal = `[${varType}:${varValue.size}]`;
                                        break;
                                    case "Function":
                                        printVal = `[${varType}]`;
                                        break;
                                    case "String":
                                        printVal = JSON.stringify(varValue.substr(0, 60)); //.replaceAll(/\r/g, '\\r')
                                        break;
                                    default:
                                        returnVal = varType;
                                }
                                output += `${shellVarNames[i]}=${printVal}\r\n`;
                            }
                        }
                        if (!doPipeOut) {
                            term.write(output);
                        }
                        return output;
                    }));

                this.AddMethod(new drpMethod("echo",
                    "Output data",
                    "[OUTPUT]",
                    { "h": new drpMethodSwitch("h", null, "Help menu") },
                    async function (switchesAndDataString, doPipeOut, pipeDataIn) {
                        let switchesAndData = this.ParseSwitchesAndData(switchesAndDataString);
                        let output = "";

                        if ("h" in switchesAndData.switches) {
                            term.write(this.ShowHelp());
                            return
                        }

                        output += switchesAndData.data;

                        if (!doPipeOut) {
                            term.write(output);
                        }
                        return output;
                    }));

                this.AddMethod(new drpMethod("jsonpath",
                    "Retrieve data from JSON object",
                    "[OPTIONS]",
                    { "q": new drpMethodSwitch("q", "string", "JSONPath query") },
                    async function (switchesAndDataString, doPipeOut, pipeDataIn) {
                        let switchesAndData = this.ParseSwitchesAndData(switchesAndDataString);
                        let output = "";

                        let inputObj = switchesAndData.data || pipeDataIn;

                        if (typeof inputObj === "string") {
                            // Try to parse as JSON
                            try {
                                inputObj = JSON.parse(inputObj);
                            } catch (ex) {
                                term.write(`\x1B[91mInput could not be parsed as JSON:\x1B[0m\r\n\x1B[37m${inputObj}\x1B[0m\r\n`);
                                return
                            }
                        }

                        if ("h" in switchesAndData.switches || !inputObj) {
                            term.write(this.ShowHelp());
                            return
                        }

                        let jsonPathQuery = switchesAndData.switches["q"];
                        jsonPathQuery = jsonPathQuery.replace(/^"|"$/g, '');
                        jsonPathQuery = jsonPathQuery.replace(/^'|'$/g, '');

                        output = JSON.stringify(jsonPath(inputObj, jsonPathQuery), null, 4).replace(/\n/g, "\r\n");

                        if (!doPipeOut) {
                            term.write(output);
                        }
                        return output;
                    }));

                this.AddMethod(new drpMethod("colors",
                    "Show terminal color test pattern",
                    "",
                    { "h": new drpMethodSwitch("h", null, "Help menu") },
                    async function (switchesAndDataString, doPipeOut, pipeDataIn) {
                        let switchesAndData = this.ParseSwitchesAndData(switchesAndDataString);
                        let output = "";

                        if ("h" in switchesAndData.switches) {
                            term.write(this.ShowHelp());
                            return
                        }

                        for (let i = 0; i < 100; i++) {
                            output += `\x1B[${i}m${i}\x1B[0m `;
                            if (i % 10 === 9) {
                                output += `\r\n`;
                            }
                        }

                        if (!doPipeOut) {
                            term.write(output);
                        }
                        return output;
                    }));

                this.AddMethod(new drpMethod("upload",
                    "Upload data for processing",
                    "",
                    {
                        "h": new drpMethodSwitch("h", null, "Help menu"),
                        "j": new drpMethodSwitch("j", null, "Convert JSON to object")
                    },
                    async function (switchesAndDataString, doPipeOut, pipeDataIn) {
                        let switchesAndData = this.ParseSwitchesAndData(switchesAndDataString);
                        let output = "";

                        if ("h" in switchesAndData.switches) {
                            term.write(this.ShowHelp());
                            return
                        }

                        // WAIT FOR DATA TO BE UPLOADED
                        myApp.appFuncs.ShowUploadDiv();

                        try {
                            let uploadData = await new Promise(function (resolve, reject) {
                                myApp.appVars.uploadPendingPromise = function (message, cancelled) {
                                    if (cancelled) {
                                        reject();
                                    } else {
                                        resolve(message);
                                    }
                                };
                            });
                            if ("j" in switchesAndData.switches) {
                                // Convert JSON to object
                                output = JSON.parse(uploadData);
                            } else {
                                output = uploadData;
                            }
                        } catch (ex) {
                            // Must have cancelled operation
                            let thisError = ex;
                        }

                        if (!doPipeOut) {
                            term.write(output);
                        }
                        return output;
                    }));

                this.AddMethod(new drpMethod("logout",
                    "Log out of DRP Desktop",
                    "",
                    {
                        "h": new drpMethodSwitch("h", null, "Help menu")
                    },
                    async function (switchesAndDataString, doPipeOut, pipeDataIn) {
                        let switchesAndData = this.ParseSwitchesAndData(switchesAndDataString);

                        if ("h" in switchesAndData.switches) {
                            term.write(this.ShowHelp());
                            return
                        }

                        // Run logout
                        myApp.vdmSession.drpClient.eraseCookie('x-api-token');
                        myApp.vdmSession.drpClient.Disconnect();
                    }));
            }
            /**
             * Add Method
             * @param {drpMethod} methodObject
             */
            AddMethod(methodObject) {
                this.drpMethods[methodObject.name] = methodObject;
            }

            async ExecuteCLICommand(commandLine) {
                let pipeData = null;
                term.write(`\r\n`);

                let cmdArray = commandLine.split(" | ");
                for (let i = 0; i < cmdArray.length; i++) {
                    let cmdParts = cmdArray[i].match(/^(\S*)(?: (.*))?$/);
                    if (cmdParts) {
                        let methodName = cmdParts[1];

                        // Replace aliases
                        if (myApp.appVars.aliases && myApp.appVars.aliases[methodName]) {
                            methodName = myApp.appVars.aliases[methodName]
                        }

                        let switchesAndData = cmdParts[2] || "";
                        let doPipeOut = (i + 1 < cmdArray.length);
                        let pipeDataIn = pipeData;
                        pipeData = "";

                        try {
                            if (!this.drpMethods[methodName]) {
                                // Write error to terminal; unknown method
                                this.term.write(`\x1B[91mInvalid command [${methodName}]\x1B[0m`);
                                this.term.write(`\r\n`);
                                return;
                            }
                            pipeData = await this.drpMethods[methodName].execute(switchesAndData, doPipeOut, pipeDataIn);

                        } catch (ex) {
                            term.write(`\x1B[91mError executing command [${methodName}]: ${ex}\x1B[0m\r\n`);
                            break;
                        }
                    }
                }
            }
        }

        myApp.appVars.termDiv = myApp.windowParts["data"];
        myApp.appVars.termDiv.style.backgroundColor = "black";
        let term = new Terminal();
        myApp.appVars.term = term;
        myApp.appVars.fitaddon = new FitAddon.FitAddon();
        term.loadAddon(myApp.appVars.fitaddon);
        term.open(myApp.appVars.termDiv);
        term.setOption('cursorBlink', true);
        term.setOption('bellStyle', 'sound');

        let writeNewPrompt = (supressNewline) => {
            if (!supressNewline) term.write('\n');
            term.write('\x1B[2K\r\x1B[95mdsh>\x1B[0m ');
        };

        let lineBufferHistory = [];
        let lineBuffer = "";
        let lineCursorIndex = 0;
        let scrollbackIndex = 0;
        let insertMode = true;

        myApp.appVars.drpShell = new drpShell(this, term);

        myApp.appVars.EndpointID = await myApp.sendCmd("DRP", "getEndpointID", null, true);

        myApp.appVars.UserInfo = await myApp.sendCmd("DRP", "getUserInfo", null, true);
        term.write(`\x1B[2K\r\x1B[97mWelcome to the DRP Shell, \x1B[33m${myApp.appVars.UserInfo.UserName}`);
        term.write(`\r\n`);
        writeNewPrompt();

        myApp.appVars.term.onKey(async (e) => {
            //let termBuffer = term.buffer.normal;
            //console.log(`${termBuffer.cursorX},${termBuffer.cursorY}`);
            let charCode = e.key.charCodeAt(0);
            let code2 = e.key.charCodeAt(1);
            let code3 = e.key.charCodeAt(2);
            //console.log(`${charCode}, ${code2}, ${code3}`);
            switch (charCode) {
                case 3:
                    // Ctrl-C
                    navigator.clipboard.writeText(term.getSelection());
                    break;
                case 22:
                    // Ctrl-V
                    let clipboardText = await navigator.clipboard.readText();
                    lineBuffer += clipboardText;
                    term.write(clipboardText);
                    lineCursorIndex += clipboardText.length;
                    break;
                case 24:
                    // Ctrl-X
                    break;
                case 9:
                    // Tab
                    break;
                case 13:
                    // Execute what's in the line buffer
                    if (lineBuffer.length > 0) {
                        // Add to lineBufferHistory
                        lineBufferHistory.unshift(lineBuffer);
                        // If the buffer is full, pop the last one
                        if (lineBufferHistory.length > 100) {
                            lineBufferHistory.pop();
                        }
                        //await myApp.appFuncs.execDRPShell(term, lineBuffer);
                        await myApp.appVars.drpShell.ExecuteCLICommand(lineBuffer);
                    }
                    lineBuffer = "";
                    lineCursorIndex = 0;
                    scrollbackIndex = 0;
                    writeNewPrompt();
                    break;
                case 27:
                    // Special character
                    if (!code2) {
                        // Escape
                        if (lineBuffer.length) {
                            lineBuffer = "";
                            lineCursorIndex = 0;
                            scrollbackIndex = 0;
                            writeNewPrompt(true);
                        } else {
                            term.write('\x07');
                        }
                    } else if (code2 === 91 && code3 === 50) {
                        // Insert
                        insertMode = !insertMode;

                        if (insertMode) term.setOption('cursorStyle', 'block');
                        else term.setOption('cursorStyle', 'underline');

                        break;
                    } else if (code2 === 91 && code3 === 51) {
                        // Delete
                        if (lineCursorIndex < lineBuffer.length) {
                            let part1 = lineBuffer.substr(0, lineCursorIndex);
                            let part2 = lineBuffer.substr(lineCursorIndex + 1);
                            lineBuffer = part1 + part2;
                            term.write(part2 + " ");
                            let goBackString = "\b";
                            for (let i = 0; i < part2.length; i++) {
                                goBackString = goBackString + "\b";
                            }
                            term.write(goBackString);
                        }
                        break;
                    } else if (code2 === 91 && code3 === 65) {
                        // Arrow up
                        if (scrollbackIndex < lineBufferHistory.length) {
                            writeNewPrompt(true);
                            lineBuffer = lineBufferHistory[scrollbackIndex];
                            lineCursorIndex = lineBuffer.length;
                            term.write(lineBuffer);
                            if (scrollbackIndex < lineBufferHistory.length) scrollbackIndex++;
                        }
                        break;
                    } else if (code2 === 91 && code3 === 66) {
                        // Arrow down
                        if (scrollbackIndex > 0) {
                            scrollbackIndex--;
                            writeNewPrompt(true);
                            lineBuffer = lineBufferHistory[scrollbackIndex];
                            lineCursorIndex = lineBuffer.length;
                            term.write(lineBuffer);
                        }
                        break;
                    } else if (code2 === 91 && code3 === 67) {
                        // Arrow right
                        if (lineCursorIndex < lineBuffer.length) {
                            lineCursorIndex++;
                            term.write(e.key);
                        }
                        break;
                    } else if (code2 === 91 && code3 === 68) {
                        // Arrow left
                        if (lineCursorIndex > 0) {
                            lineCursorIndex--;
                            term.write(e.key);
                        }
                        break;
                    } else if (code2 === 91 && code3 === 70) {
                        // End
                        for (let i = 0; i < lineBuffer.length - lineCursorIndex; i++) {
                            term.write(('\x1b[C'));
                        }
                        lineCursorIndex = lineBuffer.length;
                        break;
                    } else if (code2 === 91 && code3 === 72) {
                        // Home
                        let goBackString = "";
                        for (let i = 0; i < lineCursorIndex; i++) {
                            goBackString = goBackString + "\b";
                        }
                        term.write(goBackString);
                        lineCursorIndex = 0;
                        break;
                    } else {
                        term.write(e.key);
                    }
                    break;
                case 127:
                    // Backspace
                    if (lineCursorIndex > 0) {
                        let part1 = lineBuffer.substr(0, lineCursorIndex - 1);
                        let part2 = lineBuffer.substr(lineCursorIndex);
                        lineBuffer = part1 + part2;
                        lineCursorIndex--;
                        term.write("\b");
                        //term.write(part2);
                        term.write(part2 + " ");
                        let goBackString = "\b";
                        for (let i = 0; i < part2.length; i++) {
                            goBackString = goBackString + "\b";
                        }
                        term.write(goBackString);
                        //term.write(" ");
                    }
                    break;
                default:
                    if (lineCursorIndex < lineBuffer.length) {
                        if (insertMode) {
                            // Insert char at index
                            let part1 = lineBuffer.substr(0, lineCursorIndex);
                            let part2 = lineBuffer.substr(lineCursorIndex);
                            lineBuffer = part1 + e.key + part2;
                            term.write(e.key);
                            term.write(part2 + " ");
                            let goBackString = "\b";
                            for (let i = 0; i < part2.length; i++) {
                                goBackString = goBackString + "\b";
                            }
                            term.write(goBackString);
                        } else {
                            // Replace char at index
                            lineBuffer = lineBuffer.substr(0, lineCursorIndex) + e.key + lineBuffer.substr(lineCursorIndex + 1);
                            term.write(e.key);
                        }
                    } else {
                        lineBuffer += e.key;
                        term.write(e.key);
                    }
                    lineCursorIndex++;
            }
            //console.log(`${termBuffer.cursorX},${termBuffer.cursorY}`);
        });

        myApp.resizeMovingHook = function () {
            myApp.appVars.fitaddon.fit();
        };

        myApp.appVars.fitaddon.fit();

        myApp.appVars.term.focus();

        // Add the drop window
        let dropWindowDiv = document.createElement('div');
        dropWindowDiv.tabIndex = 991;
        dropWindowDiv.className = "uploadDiv";
        dropWindowDiv.style = `position: absolute;left: 0px;top: 0px;width: 100%;height: 100%;`;

        let dropWindowP = document.createElement('p');
        dropWindowP.style = "margin: 0; position: absolute; top: 50%; left: 50%; transform: translate(-50%, -50%); text-align: center; font-size: x-large; line-height: normal; color: forestgreen;";
        dropWindowP.innerHTML = "Drag and drop file here<br>(or press ESC)";
        dropWindowDiv.appendChild(dropWindowP);

        dropWindowDiv.ondragover = function (event) {
            event.preventDefault();
            dropWindowDiv.style["background-color"] = myApp.appVars.hoverbgcolor;
        }

        dropWindowDiv.ondragleave = function (event) {
            event.preventDefault();
            dropWindowDiv.style["background-color"] = myApp.appVars.normalbgcolor;
        }

        dropWindowDiv.ondrop = function (event) {
            event.preventDefault();
            myApp.appFuncs.HideUploadDiv();

            let fileRecord = event.dataTransfer.files[0];
            let fileReader = new FileReader();
            fileReader.onload = function (event) {
                myApp.appVars.uploadPendingPromise(event.target.result, false);
            };
            fileReader.readAsBinaryString(fileRecord);
            //document.getElementById("demo").style.color = "";
            //event.target.style.border = "";
            //var data = event.dataTransfer.getData("Text");
            //event.target.appendChild(document.getElementById(data));
        };

        dropWindowDiv.onkeyup = (e) => {
            let charCode = e.key.charCodeAt(0);
            let code2 = e.key.charCodeAt(1);
            let code3 = e.key.charCodeAt(2);
            //console.log(`${charCode}, ${code2}, ${code3}`);
            switch (e.key) {
                case "Escape":
                    // Escape
                    myApp.appFuncs.HideUploadDiv();
                    myApp.appVars.uploadPendingPromise(null, true);
                    break;
                default:
            }
        };

        myApp.appVars.dropWindowDiv = dropWindowDiv;
        myApp.appFuncs.HideUploadDiv();

        myApp.appVars.termDiv.appendChild(dropWindowDiv);
    }
});
//# sourceURL=vdm-app-DRPShell.js