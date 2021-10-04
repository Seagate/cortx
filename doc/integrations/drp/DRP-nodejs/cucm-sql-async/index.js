const util = require("util");
const https = require("https");
const parseString = require('xml2js').parseString;

var XML_ENVELOPE = '<soapenv:Envelope xmlns:soapenv="http://schemas.xmlsoap.org/soap/envelope/" xmlns:ns="http://www.cisco.com/AXL/API/10.5"><soapenv:Header/><soapenv:Body><ns:executeSQL%s><sql>%s</sql></ns:executeSQL%s></soapenv:Body></soapenv:Envelope>';

function CucmSQLSession(cucmServerUrl, cucmUser, cucmPassword) {
    this._OPTIONS = {
        host: cucmServerUrl, // The IP Address of the Communications Manager Server
        port: 443, // Clearly port 443 for SSL -- I think it's the default so could be removed
        path: '/axl/', // This is the URL for accessing axl on the server
        method: 'POST', // AXL Requires POST messages
        headers: {
            'SOAPAction': 'CUCM:DB ver=10.5',
            'Authorization': 'Basic ' + new Buffer(cucmUser + ":" + cucmPassword).toString('base64'),
            'Content-Type': 'text/xml;'
        },
        rejectUnauthorized: false // required to accept self-signed certificate
    };
    this.chunkSize = 0;
    this.chunkCount = 0;
}

CucmSQLSession.prototype.queryPromise = function (SQL) {
    var thisSession = this;
    return new Promise(function (resolve, reject) {
        thisSession.query(SQL, function (err, returnData) {
            if (err) {
                reject(err);
            }
            else {
                //callback(err, returnData);
                resolve(returnData);
            }
            //resolve(err, returnData);
        })
    });
}

CucmSQLSession.prototype.query = async function (SQL, callback) {
    var thisSession = this;
    //thisSession.queryPromise = util.promisify(thisSession.query);

    // The user needs to make sure they are sending safe SQL to the communications manager.
    if (thisSession.chunkSize) {
        var batchText = "";
        if (thisSession.chunkCount) {
            batchText = "skip " + thisSession.chunkSize * thisSession.chunkCount + " ";
        }
        batchText = batchText + "first " + thisSession.chunkSize;
        SQL = SQL.replace("select ", "select " + batchText + " ");
        //console.log("SQL -> " + SQL);
        thisSession.chunkCount++;
    } else {
        thisSession.SQL = SQL;
    }
    var XML = util.format(XML_ENVELOPE, 'Query', SQL, 'Query');
    var soapBody = new Buffer(XML);
    var output = "";
    var options = this._OPTIONS;
    options.agent = new https.Agent({
        keepAlive: false
    });
    var outputHeader = false;

    var returnArray = [];

    var req = https.request(options, function (res) {
        //console.log("Sending request...");
        res.setEncoding('utf8');
        res.on('data', function (d) {
            if (!outputHeader) {
                //console.dir(res);
                outputHeader = true;
            }
            //console.log("Received response...");
            output = output + d;
        });
        res.on('end', function (d) {
            parseString(output, {
                explicitArray: false,
                explicitRoot: false
            }, async function (err, result) {
                if (err) {
                    // SOAP Error
                    callback(err, null);
                } else if (!result['soapenv:Body']['soapenv:Fault']) {
                    // No SOAP error and no Fault.  Good to go.
                    var returnArray = [];
                    if (result['soapenv:Body']['ns:executeSQLQueryResponse']['return']) {
                        if (result['soapenv:Body']['ns:executeSQLQueryResponse']['return']['row'].constructor === Array) {
                            // Multi-row return
                            returnArray = result['soapenv:Body']['ns:executeSQLQueryResponse']['return']['row'];
                        } else {
                            // Single row return
                            returnArray.push(result['soapenv:Body']['ns:executeSQLQueryResponse']['return']['row']);
                        }
                    } else {
                        // No rows
                    }

                    callback(null, returnArray);
                } else if (result['soapenv:Body']['soapenv:Fault']['faultstring'].match(/^Query request too large/)) {
                    // Need to break query up into chunks
                    var matches = result['soapenv:Body']['soapenv:Fault']['faultstring'].match(/^Query request too large. Total rows matched: (\d+) rows. Suggestive Row Fetch: less than (\d+) rows/);
                    //console.dir(thisSession);
                    thisSession.totalRows = matches[1];
                    thisSession.chunkSize = Math.floor(parseInt(matches[2]) / 2);
                    thisSession.remainder = thisSession.totalRows % thisSession.chunkSize;
                    thisSession.chunkCount = 0;
                    thisSession.loopCount = Math.ceil(thisSession.totalRows / thisSession.chunkSize);
                    //console.log("Fetching results in [", thisSession.chunkSize, "] row chunks...");
                    var aggregateResults = [];
                    for (var i = 0; i < thisSession.loopCount; i++) {
                        //console.log("Getting batch [" + i + "]...");
                        var axlResults = await thisSession.queryPromise(thisSession.SQL, callback);
                        aggregateResults = aggregateResults.concat(axlResults);
                        //callback(null, axlResult);
                        //console.dir(axlResult);
                    }

                    // NEED TO ADD RESET BEFORE CALLBACK!!!
                    thisSession.chunkSize = 0;

                    callback(null, aggregateResults);
                } else {
                    // Unknown AXL error; return fault string
                    callback(result['soapenv:Body']['soapenv:Fault']['faultstring'], null)
                }
            });
        });
        req.on('error', function (e) {
            callback(e);
        });
    });

    // Only reached if there are 0 results?
    req.end(soapBody);
};

CucmSQLSession.prototype.update = function (SQL, callback) {
    // The user needs to make sure they are sending safe SQL to the communications manager.
    var XML = util.format(XML_ENVELOPE, 'Update', SQL, 'Update');
    var soapBody = new Buffer(XML);
    var output = "";
    var options = this._OPTIONS;
    options.agent = new https.Agent({
        keepAlive: false
    });
    var outputHeader = false;

    var req = https.request(options, function (res) {
        //console.log("Sending request...");
        res.setEncoding('utf8');
        res.on('data', function (d) {
            if (!outputHeader) {
                //console.dir(res);
                outputHeader = true;
            }
            //console.log("Received response...");
            output = output + d;
        });
        res.on('end', function (d) {
            parseString(output, {
                explicitArray: false,
                explicitRoot: false
            }, function (err, result) {
                try {
                    callback(null, result['soapenv:Body']['ns:executeSQLUpdateResponse']['return']['rowsUpdated']);
                } catch (ex) {
                    callback(ex, result)
                }
            });
        });
        req.on('error', function (e) {
            callback(e);
        });
    });
    req.end(soapBody);

};

module.exports = function (cucmServerUrl, cucmUser, cucmPassword) {
    return new CucmSQLSession(cucmServerUrl, cucmUser, cucmPassword);
}
