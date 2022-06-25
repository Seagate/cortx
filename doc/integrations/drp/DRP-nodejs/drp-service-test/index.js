'use strict';
const DRP_Node = require('drp-mesh').Node;
const DRP_Service = require('drp-mesh').Service;
const DRP_UMLAttribute = require('drp-mesh').UML.Attribute;
const DRP_UMLFunction = require('drp-mesh').UML.Function;
const DRP_UMLClass = require('drp-mesh').UML.Class;

// Define OpenAPI Doc
let openAPIDoc = {
    "openapi": "3.0.1",
    "info": {
        "title": "TestService",
        "description": "This is a Test Service API",
        "version": "1.0.1"
    },
    "servers": [
        {
            "url": "/Mesh/Services/ServiceName"
        }
    ],
    "tags": [
        {
            "name": "ClientCmds",
            "description": "Service ClientCmds"
        }
    ],
    "paths": {
        "/ClientCmds/sayHi": {
            "get": {
                "tags": [
                    "ClientCmds"
                ],
                "summary": "Hello",
                "description": "Says Hello",
                "operationId": "sayHi",
                "responses": {
                    "200": {
                        "description": "Greeting",
                        "content": {
                            "application/json": {
                                "schema": {
                                    "type": "object"
                                }
                            }
                        }
                    }
                },
                "x-swagger-router-controller": "TestService"
            }
        },
        "/ClientCmds/sayBye": {
            "get": {
                "tags": [
                    "ClientCmds"
                ],
                "summary": "Goodbye",
                "description": "Says Bye",
                "operationId": "sayBye",
                "responses": {
                    "200": {
                        "description": "Farewell",
                        "content": {
                            "application/json": {
                                "schema": {
                                    "type": "object"
                                }
                            }
                        }
                    }
                },
                "x-swagger-router-controller": "TestService"
            }
        },
        "/ClientCmds/showParams": {
            "get": {
                "tags": [
                    "ClientCmds"
                ],
                "summary": "Show Params",
                "description": "Echoes back parameters",
                "operationId": "showParams",
                "responses": {
                    "200": {
                        "description": "Params",
                        "content": {
                            "application/json": {
                                "schema": {
                                    "type": "object"
                                }
                            }
                        }
                    }
                },
                "x-swagger-router-controller": "TestService"
            }
        }
    },
    "components": {
        "securitySchemes": {
            "x-api-key": {
                "type": "apiKey",
                "name": "x-api-key",
                "in": "header"
            },
            "x-api-token": {
                "type": "apiKey",
                "name": "x-api-token",
                "in": "header"
            }
        }
    },
    "security": [
        { "x-api-key": [] },
        { "x-api-token": [] }
    ]
};

// Create test service class
class TestService extends DRP_Service {
    constructor(serviceName, drpNode, priority, weight, scope) {
        super(serviceName, drpNode, "TestService", null, false, priority, weight, drpNode.Zone, scope, null, ["TestStream"], 1);
        let thisService = this;

        // Define global methods
        this.ClientCmds = {
            getOpenAPIDoc: async function (cmdObj) { return openAPIDoc; },
            sayHi: async function () {
                thisService.DRPNode.log("Remote node wants to say hi");
                return {
                    pathItem: `Hello from ${thisService.DRPNode.NodeID}`
                };
            },
            sayBye: async function () {
                thisService.DRPNode.log("Remote node wants to say bye");
                return {
                    pathItem: `Goodbye from ${thisService.DRPNode.NodeID}`
                };
            },
            showParams: async function (params) {
                return {
                    pathItem: params
                };
            },
            peerBroadcastTest: async function (params) {
                thisService.DRPNode.log(`Peer service sent a broadcast test - ${params.message}`);
            },
            sendPeerBroadcastTest: async function (params) {
                thisService.PeerBroadcast("peerBroadcastTest", { message: `From serviceID ${thisService.InstanceID}` });
                thisService.DRPNode.log(`Sent a peer broadcast`);
            }
        };


        // Define data classes for this Provider
        this.AddClass(new DRP_UMLClass("Person", [],
            [
                new DRP_UMLAttribute("personID", "personID", null, false, "int", null, "1", "PK,MK"),
                new DRP_UMLAttribute("firstName", null, null, false, "string(128)", null, "1", null),
                new DRP_UMLAttribute("lastName", null, null, false, "string(128)", null, "1", null),
                new DRP_UMLAttribute("departmentName", "departmentName", null, false, "string(128)", null, "1", "FK")
            ],
            [
                new DRP_UMLFunction("terminate", null, ["effectiveDate"], "results")
            ]
        ));

        this.AddClass(new DRP_UMLClass("Department", [],
            [
                new DRP_UMLAttribute("name", "departmentName", null, false, "string(128)", null, "1", "PK,MK"),
                new DRP_UMLAttribute("description", null, null, false, "string(128)", null, "1", null),
                new DRP_UMLAttribute("address", null, null, false, "string(128)", null, "1", null)
            ],
            []
        ));

        // Add sample data records
        this.Classes['Person'].AddRecord({
            "personID": 1001,
            "firstName": "John",
            "lastName": "Smith",
            "departmentName": "Accounting"
        }, this.serviceName, "2019-11-30T04:10:54.843Z");

        this.Classes['Person'].AddRecord({
            "personID": 1002,
            "firstName": "Bob",
            "lastName": "Jones",
            "departmentName": "Accounting"
        }, this.serviceName, "2019-11-30T04:10:54.843Z");

        this.Classes['Department'].AddRecord({
            "name": "Accounting",
            "description": "Number crunchers",
            "address": "123 Pine St, Nowhere, AR"
        }, this.serviceName, "2019-11-30T04:10:54.843Z");

        // Mark cache loading as complete
        this.Classes['Person'].loadedCache = true;
        this.Classes['Department'].loadedCache = true;

        // Start sending data to TestStream
        setInterval(function () {
            thisService.DRPNode.TopicManager.SendToTopic("TestStream", `Test message from [${thisService.InstanceID}]`);
        }, 3000);
    }
}

module.exports = TestService;