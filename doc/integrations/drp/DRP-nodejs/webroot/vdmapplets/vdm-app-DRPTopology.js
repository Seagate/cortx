(class extends rSageApplet {
    constructor(appletProfile) {
        super(appletProfile);
        let myApp = this;

        class topologyNode {
            constructor() {
                /** @type {string} */
                this.NodeID = "";
                /** @type {string} */
                this.ProxyNodeID = "";
                /** @type {string} */
                this.Scope = "";
                /** @type {string} */
                this.Zone = "";
                /** @type {string} */
                this.LearnedFrom = "";
                /** @type {string[]} */
                this.Roles = [];
                /** @type {string} */
                this.NodeURL = "";
                /** @type {string} */
                this.HostID = "";
                /** @type {string[]} */
                this.ConsumerClients = [];
                /** @type {string[]} */
                this.NodeClients = [];
                /** @type {Object.<string,Object>} */
                this.Services = {};
            }
        }

        // Dropdown menu items
        myApp.menu = {
            "View": {
                "Toggle Refresh": function () {
                    myApp.appFuncs.toggleRefresh();
                },
                "Output JSON": async function () {
                    let fileData = JSON.stringify(myApp.appVars.cy.json());
                    console.log(fileData);
                    //myApp.appVars.msgBox.innerHTML = results;
                    //alert(results);
                }
            }
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
            "toggleRefresh": function () {
                if (!myApp.appVars.refreshActive) {
                    myApp.appVars.refreshInterval = setInterval(async () => {
                        myApp.appFuncs.loadNodeTopology();
                    }, 10000);
                    myApp.appVars.refreshActive = true;
                } else {
                    clearInterval(myApp.appVars.refreshInterval);
                    myApp.appVars.refreshActive = false;
                }
            },
            "placeNode": function (nodeClass) {
                let returnPosition = { x: 0, y: 0 };
                let colsPerRow = 6;
                switch (nodeClass) {
                    case "Registry":
                        returnPosition = Object.assign(returnPosition, myApp.appVars.nodeCursors["Registry"]);
                        myApp.appVars.nodeCursors["Registry"].y += 75;
                        break;
                    case "Broker":
                        returnPosition = Object.assign(returnPosition, myApp.appVars.nodeCursors["Broker"]);
                        returnPosition.y += myApp.appVars.nodeCursors["Broker"].index * 250;
                        myApp.appVars.nodeCursors["Broker"].index++;
                        break;
                    case "Provider":
                        returnPosition = Object.assign(returnPosition, myApp.appVars.nodeCursors["Provider"]);
                        myApp.appVars.nodeCursors["Provider"].y += 75;
                        break;
                    case "Logger":
                        returnPosition = Object.assign(returnPosition, myApp.appVars.nodeCursors["Logger"]);
                        myApp.appVars.nodeCursors["Logger"].y += 75;
                        break;
                    case "Consumer":
                        returnPosition = Object.assign(returnPosition, myApp.appVars.nodeCursors["Consumer"]);
                        let column = returnPosition.index % colsPerRow;
                        returnPosition.x += column * 50;
                        let row = Math.floor(returnPosition.index / colsPerRow);
                        returnPosition.y += row * 50;
                        myApp.appVars.nodeCursors["Consumer"].index++;
                        break;
                    case "Service":
                        returnPosition = Object.assign(returnPosition, myApp.appVars.nodeCursors["Service"]);
                        myApp.appVars.nodeCursors["Service"].y += 50;
                        myApp.appVars.nodeCursors["Service"].index++;
                        break;
                    default:
                }
                return returnPosition;
            },
            /**
             * 
             * @param {Object.<string, topologyNode>} topologyObj DRP Topology
             */
            "importMeshTopology": async function (topologyObj) {
                //let jsonText = `{"elements":{"nodes":[{"data":{"group":"nodes","id":"49d895b6-1953-4139-bbd4-7a9d40afb6f9","label":"node1"},"position":{"x":119,"y":133},"group":"nodes","removed":false,"selected":false,"selectable":true,"locked":false,"grabbable":true,"classes":""},{"data":{"group":"nodes","id":"d1ba0134-5ed1-46be-8a65-1ea0b0183642","label":"node2"},"position":{"x":270,"y":202},"group":"nodes","removed":false,"selected":false,"selectable":true,"locked":false,"grabbable":true,"classes":""}],"edges":[{"data":{"source":"49d895b6-1953-4139-bbd4-7a9d40afb6f9","target":"d1ba0134-5ed1-46be-8a65-1ea0b0183642","label":"dynamic","id":"ed551066-37f6-453e-a08c-ca2b571a7357"},"position":{},"group":"edges","removed":false,"selected":false,"selectable":true,"locked":false,"grabbable":true,"classes":""}]},"style":[{"selector":"node","style":{"label":"data(label)"}},{"selector":"edge","style":{"target-arrow-shape":"triangle"}},{"selector":":selected","style":{}}],"zoomingEnabled":true,"userZoomingEnabled":true,"zoom":1,"minZoom":1e-50,"maxZoom":1e+50,"panningEnabled":true,"userPanningEnabled":true,"pan":{"x":47,"y":-24},"boxSelectionEnabled":true,"renderer":{"name":"canvas"},"wheelSensitivity":0.25}`;
                //let jsonParsed = JSON.parse(jsonText);
                //myApp.appVars.cy.json(jsonParsed);

                let typeStyle = "grid-column: 1/ span 2;text-align: center; font-size: large;";
                let headerStyle = "text-align: right; padding-right: 10px;";
                let dataStyle = "color: lightseagreen; font-weight: bold;";

                // Clear existing nodes and edges

                // Loop over DRP Nodes in topology
                let nodeIDs = Object.keys(topologyObj);
                for (let i = 0; i < nodeIDs.length; i++) {
                    let drpNodeID = nodeIDs[i];
                    let drpNode = topologyObj[drpNodeID];

                    let labelData = `${drpNode.NodeID}\n[${drpNode.HostID}]`;
                    if (drpNode.NodeURL) labelData = `${labelData}\n${drpNode.NodeURL}`;

                    // Add DRP Node as Cytoscape node
                    myApp.appVars.cy.add({
                        group: 'nodes',
                        data: {
                            id: drpNodeID,
                            label: labelData,
                            drpNode: drpNode,
                            ShowDetails: async () => {
                                let returnVal = `<span style="${typeStyle}">Mesh Node</span>` +
                                    `<span style="${headerStyle}">Node ID:</span><span style="${dataStyle}">${drpNode.NodeID}</span>` +
                                    `<span style="${headerStyle}">Host ID:</span><span style="${dataStyle}">${drpNode.HostID}</span>`;
                                if (drpNode.NodeURL) {
                                    returnVal += `<span style="${headerStyle}">URL:</span><span style="${dataStyle}">${drpNode.NodeURL}</span>`;
                                } else {
                                    returnVal += `<span style="${headerStyle}">URL:</span><span style="${dataStyle}">(non-listening)</span>`;
                                }
                                returnVal += `<span style="${headerStyle}">Scope:</span><span style="${dataStyle}">${drpNode.Scope}</span>` +
                                    `<span style="${headerStyle}">Zone:</span><span style="${dataStyle}">${drpNode.Zone}</span>` +
                                    `<span style="${headerStyle}">Roles:</span><span style="${dataStyle}">${drpNode.Roles}</span>`;
                                return returnVal;
                            }
                        },
                        classes: drpNode.Roles.join(" "),
                        position: myApp.appFuncs.placeNode(drpNode.Roles[0])
                    });

                    // Loop over Node services
                    let serviceNameList = Object.keys(drpNode.Services);
                    for (let j = 0; j < serviceNameList.length; j++) {

                        let serviceName = serviceNameList[j];
                        if (serviceName === "DRP") {
                            continue;
                        }
                        let serviceObj = drpNode.Services[serviceName];
                        let serviceNodeID = serviceObj.InstanceID;

                        // See if service node exists
                        let svcNodeObj = myApp.appVars.cy.getElementById(serviceNodeID);
                        if (svcNodeObj.length === 0) {
                            // No - create it
                            myApp.appVars.cy.add({
                                group: 'nodes',
                                data: {
                                    id: serviceNodeID,
                                    label: serviceName,
                                    ShowDetails: async () => {
                                        let returnVal = `<span style="${typeStyle}">Mesh Service</span>` +
                                            `<span style="${headerStyle}">Name:</span><span style="${dataStyle}">${serviceName}</span>` +
                                            `<span style="${headerStyle}">Instance ID:</span><span style="${dataStyle}">${serviceObj.InstanceID}</span>` +
                                            `<span style="${headerStyle}">Node ID:</span><span style="${dataStyle}">${drpNode.NodeID}</span>` +
                                            `<span style="${headerStyle}">Scope:</span><span style="${dataStyle}">${serviceObj.Scope}</span>` +
                                            `<span style="${headerStyle}">Zone:</span><span style="${dataStyle}">${serviceObj.Zone}</span>`;
                                        return returnVal;
                                    }
                                },
                                classes: "Service",
                                position: myApp.appFuncs.placeNode("Service")
                            });
                        }

                        // Create edge
                        myApp.appVars.cy.add({
                            group: 'edges',
                            data: {
                                id: `${serviceNodeID}_${drpNodeID}`,
                                source: serviceNodeID,
                                target: drpNodeID
                            }
                        });
                    }
                }

                // Loop over DRP Nodes again; create Edges
                for (let i = 0; i < nodeIDs.length; i++) {
                    let drpNodeID = nodeIDs[i];
                    let drpNode = topologyObj[drpNodeID];
                    myApp.appVars.nodeCursors["Consumer"].index = 0;
                    let nodeObj = myApp.appVars.cy.getElementById(drpNodeID);
                    myApp.appVars.nodeCursors["Consumer"].y = nodeObj.position().y;

                    // Loop over nodeClients
                    let nodeClientIDs = Object.keys(drpNode.NodeClients);
                    for (let j = 0; j < nodeClientIDs.length; j++) {
                        let targetNodeID = nodeClientIDs[j];

                        myApp.appVars.cy.add({
                            group: 'edges',
                            data: {
                                id: `${drpNodeID}_${targetNodeID}`,
                                source: targetNodeID,
                                target: drpNodeID,
                                label: drpNode.NodeClients[nodeClientIDs[j]]['pingTimeMs'] + " ms"
                            }
                        });
                    }

                    // Loop over consumerClients
                    let consumerClientIDs = Object.keys(drpNode.ConsumerClients);
                    for (let j = 0; j < consumerClientIDs.length; j++) {
                        let consumerIndex = consumerClientIDs[j];
                        let consumerID = `${drpNodeID}-c:${consumerClientIDs[j]}`;

                        myApp.appVars.cy.add({
                            group: 'nodes',
                            data: {
                                id: consumerID,
                                label: `${consumerClientIDs[j]}`,
                                ShowDetails: async () => {
                                    // Get User Details
                                    let pathListArray = `Mesh/Nodes/${drpNodeID}/DRPNode/ConsumerEndpoints/${consumerIndex}/AuthInfo/userInfo`.split('/');
                                    let results = await myApp.sendCmd("DRP", "pathCmd", { pathList: pathListArray, listOnly: false }, true);
                                    if (results && results.pathItem) {
                                        return `<span style="${typeStyle}">Consumer</span>` +
                                            `<span style="${headerStyle}">User ID:</span><span style="${dataStyle}">${results.pathItem.UserName}</span>` +
                                            `<span style="${headerStyle}">Name:</span><span style="${dataStyle}">${results.pathItem.FullName}</span>`;
                                    }
                                }
                            },
                            classes: "Consumer",
                            position: myApp.appFuncs.placeNode("Consumer")
                        });

                        myApp.appVars.cy.add({
                            group: 'edges',
                            data: {
                                id: `${consumerID}_${drpNodeID}`,
                                source: consumerID,
                                target: drpNodeID,
                                //label: drpNode.consumerClients[consumerClientIDs[j]]['pingTimeMs'] + " ms\n" + drpNode.consumerClients[consumerClientIDs[j]]['uptimeSeconds'] + " s"
                                label: drpNode.ConsumerClients[consumerClientIDs[j]]['pingTimeMs'] + " ms"
                            }
                        });
                    }
                }
            },
            "loadNodeTopology": async function () {
                myApp.appVars.cy.elements().remove();

                myApp.appVars.nodeCursors = {
                    Registry: { x: 400, y: 50, index: 0 },
                    Broker: { x: 650, y: 100, index: 0 },
                    Provider: { x: 200, y: 100, index: 0 },
                    Logger: { x: 450, y: 250, index: 0 },
                    Consumer: { x: 825, y: 100, index: 0 },
                    Service: { x: 50, y: 100, index: 0 }
                };

                /** @type {Object.<string, topologyNode>}} */
                let topologyObj = await myApp.sendCmd("DRP", "getTopology", null, true);
                myApp.appFuncs.importMeshTopology(topologyObj);
            }
        };

        myApp.appVars = {
            dataStructs: {},
            refreshActive: false,
            refreshInterval: null,
            cy: null,
            linkFromObj: null,
            currentFile: "",
            nodeCursors: {
                Registry: { x: 400, y: 50, index: 0 },
                Broker: { x: 650, y: 100, index: 0 },
                Provider: { x: 200, y: 100, index: 0 },
                Logger: { x: 450, y: 250, index: 0 },
                Consumer: { x: 825, y: 100, index: 0 }
            },
            displayedNodeID: null
        };

        myApp.recvCmd = {
        };
    }

    async runStartup() {
        let myApp = this;

        myApp.appVars.cyBox = myApp.windowParts["data"];

        let cy = cytoscape({
            container: myApp.appVars.cyBox,
            wheelSensitivity: .25,
            zoom: 1,
            //pan: { "x": 300, "y": 160 },

            style: [{
                selector: 'node',
                style: {
                    //'font-family' : 'FontAwesome',
                    //'content' : '\uf099  twitter'
                    'font-size': '12px',
                    'text-wrap': 'wrap',
                    'content': 'data(label)',
                    'opacity': 1
                }
            }, {
                selector: 'node.Provider',
                style: {
                    'shape': "triangle",
                    'background-color': '#AADDAA'
                }
            }, {
                selector: 'node.Broker',
                style: {
                    'shape': "square",
                    'background-color': '#AAAADD'
                }
            }, {
                selector: 'node.Registry',
                style: {
                    'shape': "star",
                    'background-color': 'gold'
                }
            }, {
                selector: 'node.Logger',
                style: {
                    'shape': "diamond",
                    'background-color': '#654321'
                }
            }, {
                selector: 'node.Consumer',
                style: {
                    'shape': "circle",
                    'background-color': '#DDD',
                    'border-width': 3,
                    'border-color': '#333',
                    'text-valign': 'center',
                    'text-halign': 'center'
                }
            }, {
                selector: 'edge',
                style: {
                    'width': 2,
                    'line-color': '#fcc',
                    'target-arrow-color': '#fcc',
                    'target-arrow-shape': 'triangle',
                    'opacity': 0.5,
                    'curve-style': 'bezier',
                    'font-size': '10px',
                    'text-wrap': 'wrap',
                    'content': 'data(label)',
                    'text-rotation': 'autorotate'
                }
            }, {
                selector: 'edge.hover',
                style: {
                    'opacity': 1.0
                }
            }, {
                selector: ':selected',
                style: {}
            }
            ],
            layout: {
                name: 'preset'
            },

            elements: {
                nodes: [],
                edges: []
            }
        });

        myApp.appVars.cy = cy;

        cy.on('mouseover', 'node', async function (e) {
            // Add highlight to connected edges
            e.cyTarget.connectedEdges().addClass('hover');
            let targetNodeData = e.cyTarget.data();

            // If the node has a "ShowDetails" function, execute and display in details box
            if (targetNodeData.ShowDetails) {
                myApp.appVars.displayedNodeID = targetNodeData.id;
                myApp.appVars.detailsDiv.style['display'] = 'grid';
                myApp.appVars.detailsDiv.innerHTML = await targetNodeData.ShowDetails();
            }
        });
        cy.on('mouseout', 'node', async function (e) {
            // Remove highlight from connected edges
            e.cyTarget.connectedEdges().removeClass('hover');
            let targetNodeData = e.cyTarget.data();

            // If the node has a "ShowDetails" function AND its details are currently in the detail box, remove and hide
            if (myApp.appVars.displayedNodeID && myApp.appVars.displayedNodeID === targetNodeData.id) {
                myApp.appVars.detailsDiv.style['display'] = 'none';
            }
        });

        cy.on('mouseover', 'edge', function (e) {
            e.cyTarget.addClass('hover');
        });
        cy.on('mouseout', 'edge', function (e) {
            e.cyTarget.removeClass('hover');
        });

        let removed = null;

        var contextMenu = myApp.appVars.cy.contextMenus({
            menuItems: [
                {
                    id: 'remove',
                    content: 'remove',
                    selector: 'node, edge',
                    onClickFunction: function (event) {
                        var target = event.cyTarget;
                        removed = target.remove();

                        contextMenu.showMenuItem('undo-last-remove');
                    },
                    hasTrailingDivider: true
                }, {
                    id: 'undo-last-remove',
                    content: 'undo last remove',
                    selector: 'node, edge',
                    show: false,
                    coreAsWell: true,
                    onClickFunction: function (event) {
                        if (removed) {
                            removed.restore();
                        }
                        contextMenu.hideMenuItem('undo-last-remove');
                    },
                    hasTrailingDivider: true
                }, {
                    id: 'hide',
                    content: 'hide',
                    selector: '*',
                    onClickFunction: function (event) {
                        var target = event.cyTarget;
                        target.hide();
                    },
                    disabled: false
                }, {
                    id: 'add-node',
                    content: 'add node',
                    coreAsWell: true,
                    onClickFunction: function (event) {
                        var data = {
                            group: 'nodes'
                        };

                        var pos = event.position || event.cyPosition;

                        cy.add({
                            data: data,
                            position: {
                                x: pos.x,
                                y: pos.y
                            }
                        });
                    }
                }, {
                    id: 'remove-selected',
                    content: 'remove selected',
                    coreAsWell: true,
                    show: true,
                    onClickFunction: function (event) {
                        removedSelected = cy.$(':selected').remove();

                        contextMenu.hideMenuItem('remove-selected');
                        contextMenu.showMenuItem('restore-selected');
                    }
                }, {
                    id: 'restore-selected',
                    content: 'restore selected',
                    coreAsWell: true,
                    show: false,
                    onClickFunction: function (event) {
                        if (removedSelected) {
                            removedSelected.restore();
                        }
                        contextMenu.showMenuItem('remove-selected');
                        contextMenu.hideMenuItem('restore-selected');
                    }
                }, {
                    id: 'select-all-nodes',
                    content: 'select all nodes',
                    selector: 'node',
                    show: true,
                    onClickFunction: function (event) {
                        selectAllOfTheSameType(event.target || event.cyTarget);

                        contextMenu.hideMenuItem('select-all-nodes');
                        contextMenu.showMenuItem('unselect-all-nodes');
                    }
                }, {
                    id: 'unselect-all-nodes',
                    content: 'unselect all nodes',
                    selector: 'node',
                    show: false,
                    onClickFunction: function (event) {
                        unselectAllOfTheSameType(event.target || event.cyTarget);

                        contextMenu.showMenuItem('select-all-nodes');
                        contextMenu.hideMenuItem('unselect-all-nodes');
                    }
                }, {
                    id: 'select-all-edges',
                    content: 'select all edges',
                    selector: 'edge',
                    show: true,
                    onClickFunction: function (event) {
                        selectAllOfTheSameType(event.target || event.cyTarget);

                        contextMenu.hideMenuItem('select-all-edges');
                        contextMenu.showMenuItem('unselect-all-edges');
                    }
                }, {
                    id: 'unselect-all-edges',
                    content: 'unselect all edges',
                    selector: 'edge',
                    show: false,
                    onClickFunction: function (event) {
                        unselectAllOfTheSameType(event.target || event.cyTarget);

                        contextMenu.showMenuItem('select-all-edges');
                        contextMenu.hideMenuItem('unselect-all-edges');
                    }
                }, {
                    id: 'link-from',
                    content: 'link from this node',
                    selector: 'node',
                    show: true,
                    onClickFunction: function (event) {
                        //selectAllOfTheSameType(event.target || event.cyTarget);
                        myApp.appVars.linkFromObj = event.target || event.cyTarget;

                        contextMenu.showMenuItem('link-to');
                    }
                }, {
                    id: 'link-to',
                    content: 'link to',
                    selector: 'node',
                    show: false,
                    onClickFunction: function (event) {
                        //selectAllOfTheSameType(event.target || event.cyTarget);
                        //myApp.appVars.linkFromObj = event.target || event.cyTarget;
                        let linkToObj = event.target || event.cyTarget;
                        cy.add([{
                            group: 'edges',
                            data: {
                                source: myApp.appVars.linkFromObj._private.data.id,
                                target: linkToObj._private.data.id,
                                label: 'dynamic'
                            }
                        }
                        ]);

                        contextMenu.hideMenuItem('link-to');
                    }
                }
            ]
        });

        myApp.resizeMovingHook = function () {
            myApp.appVars.cy.resize();
            //cy.fit();
        };

        myApp.appFuncs.loadNodeTopology();

        // Add the drop window
        let detailsDiv = document.createElement('div');
        detailsDiv.className = "detailsDiv";
        detailsDiv.style = `width: 400px;height: 200px;z-index: 1;margin: 0;position: absolute;bottom: 0;right: 0;text-align: left;font-size: 14px;line-height: normal; color: khaki; background-color: black;opacity: .9; box-sizing: border-box; padding: 10px;
    display: none;
    grid-template-columns: 25% 75%;
    grid-template-rows: 25px 25px 25px 25px 25px 25px;
    border-radius: 20px;`;
        detailsDiv.innerHTML = "&nbsp;";

        myApp.appVars.detailsDiv = detailsDiv;

        myApp.appVars.cyBox.appendChild(detailsDiv);
    }
});
//# sourceURL=vdm-app-DRPTopology.js