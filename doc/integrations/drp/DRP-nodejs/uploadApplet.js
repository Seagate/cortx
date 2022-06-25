'use strict';
const DRP_Consumer = require('drp-mesh').Consumer;
const fs = require('fs').promises;

//let brokerURL = process.env.BROKERURL || "ws://localhost:8080";
let user = process.env.USER || null;
let pass = process.env.PASS || null;
let appletPath = process.env.APPLETPATH || "webroot/vdmapplets";

let myArgs = process.argv.slice(2);
let appletName = myArgs[0];
let brokerURL = myArgs[1] || "ws://localhost:8080";

if (!user) die("ENV variable USER not specified");
if (!pass) die("ENV variable PASS not specified");
if (!appletName) die(`Usage: node ${process.argv[1]} vdm-app-SomeApp [ws://brokerurl]`);

console.log(`Uploading applet, connecting to Broker Node @ ${brokerURL}`);
let myConsumer = new DRP_Consumer(brokerURL, user, pass, null, async function () {
    let appletProfileRaw = null;
    let appletContentsRaw = null;
    try {
        appletProfileRaw = await fs.readFile(`${appletPath}/${appletName}.json`, "utf8");
    } catch (ex) {
        die(`Error reading profile for applet '${appletName}' -> ${ex.message}`);
    }
    let appletProfile = JSON.parse(appletProfileRaw);
    if (!appletProfile) die(`Applet Profiles JSON could not be parsed into an object`);
    if (!appletProfile.appletScript) die(`Applet Profile does not contain appletScript`);
    try {
        appletContentsRaw = await fs.readFile(`${appletPath}/${appletProfile.appletScript}`, "utf8");
    } catch (ex) {
        die(`Error reading script for applet '${appletName}' -> ${ex.message}`);
    }
    appletProfile.appletContents = appletContentsRaw;
    let response = await myConsumer.BrokerClient.SendCmd("VDM", "uploadVDMApplet", appletProfile, true, null, null, null);
    if (response.status === 1) {
        console.log("Upload successful");
    } else {
        console.log(`Upload failed:`);
        console.dir(response);
    }
    process.exit(0);
});

function die(exitMessage) {
    console.error(exitMessage);
    process.exit(1);
}