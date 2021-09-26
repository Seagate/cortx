'use strict';
const DRP_Consumer = require('drp-mesh').Consumer;

let brokerURL = process.env.BROKERURL || "ws://localhost:8080";
let user = process.env.USER || null;
let pass = process.env.PASS || null;

console.log(`Starting Test Consumer, connecting to Broker Node @ ${brokerURL}`);
let myConsumer = new DRP_Consumer(brokerURL, user, pass, null, async function () {
    // Connection established - let's do stuff
    let response = null;

    // See what commands are available;
    //response = await myClient.SendCmd("getCmds", null, true, null);
    //console.dir(response, { "depth": 10 });
    //myClient.SendCmd("getCmds", null, false, (response) => { console.dir(response) });

    // Execute a pathCmd
    //myClient.SendCmd("pathCmd", { "method": "cliGetPath", "pathList": ["Providers", "DocMgr1", "Services", "DocMgr", "ClientCmds", "listFiles"], "params": {}, "listOnly": true }, false, (payload) => { console.dir(payload, { "depth": 10 }) });

    // Get data for a class
    //myClient.GetClassRecords("SomeDataClass", (payload) => console.dir(payload) );

    // Subscribe to a stream
    let streamName = "TestStream";
    myConsumer.BrokerClient.WatchStream(streamName, "global", (payload) => {
        console.log(`[${streamName}] -> ${JSON.stringify(payload, null, 2)}`);
    });
    myConsumer.BrokerClient.WatchStream("TopologyTracker", "local", (payload) => {
        console.log(`[TopologyTracker] -> ${JSON.stringify(payload, null, 2)}`);
    });

    // Execute a service command
    //myClient.SendCmd("serviceCommand", { "serviceName": "Hive", "method": "listStereoTypes" }, false, (payload) => { console.dir(payload) });

    // List Files
    //myClient.SendCmd("serviceCommand", { "serviceName": "DocMgr", "method": "listFiles" }, false, (payload) => { console.dir(payload) });

    // Load a file
    //response = await myClient.SendCmd("serviceCommand", { "serviceName": "DocMgr", "method": "loadFile", "fileName": "newFile.json" }, true, null);

    // Save a file
    //response = await myClient.SendCmd("serviceCommand", { "serviceName": "DocMgr", "method": "saveFile", "fileName": "newFile.json", "fileData": JSON.stringify({"someKey":"someVal"}) }, true, null);
	
	//response = (await myClient.SendCmd("Greeter", "showParams", {"pathList":["asdf","ijkl"]}, true, null)).payload.pathItem;

    //console.dir(response, { "depth": 10 });
});