(class extends rSageApplet {
    constructor(appletProfile) {
        super(appletProfile);
        let myApp = this;

        this.menu = {
            "Tracking": {
                "Track by Number(s)": async function () {
                    myApp.appFuncs.showTrackByNumber();
                }
            },
            "Addresses": {
                "Validate Address": async function () {
                    myApp.appFuncs.showValidateAddress();
                }
            },
            "Locations": {
                "Find Location": async function () {
                    myApp.appFuncs.showFindLocation();
                }
            },
            "Quotes": {
                "Quote Rates": async function () {
                    myApp.appFuncs.showRateQuote();
                }
            },
            "Ship": {
                "Create Shipment": async function () {
                    myApp.appFuncs.showCreateShipment();
                }
            }
        };

        this.appFuncs = {
            clearPanes: () => {
                myApp.appVars.topPane.innerHTML = "";
                myApp.appVars.bottomPane.innerHTML = "";
            },
            showTrackByNumber: async () => {
                // Clear panes
                this.appFuncs.clearPanes();

                // Add form to top
                myApp.appVars.topPane.innerHTML = `
Tracking Number(s):<br>
<textarea class="inputText"></textarea><br>
<button class="cmdSend">Send</button>
`;

                // Assign appVars
                let cmdParamsInput = $(myApp.appVars.topPane).find('.inputText')[0];
                let cmdSend = $(myApp.appVars.topPane).find('.cmdSend')[0];

                $(cmdSend).on('click', async function () {

                    // Get tracking numbers
                    let trackingNumbersRaw = cmdParamsInput.value;

                    // Need to sanitize!  For now assume one raw number
                    let appDataObj = {
                        apiPayload: {}
                    };
                    appDataObj.apiPayload = {
                        "includeDetailedScans": true,
                        "trackingInfo": [{ "trackingNumberInfo": { "trackingNumber": trackingNumbersRaw } }]
                    };

                    // Send DRP command
                    let response = await myApp.sendCmd("FedEx", "TrackByTrackingNumbers", appDataObj, true);

                    if (response) {
                        // Response to immediate command
                        myApp.appFuncs.displayResponse(response);
                    }
                });
            },
            showValidateAddress: async () => {
                // Clear panes
                this.appFuncs.clearPanes();

                // Add form to top
                myApp.appVars.topPane.innerHTML = `
Street 1: <input class="street1" type="text"><br>
Street 2: <input class="street2" type="text"><br>
City: <input class="city" type="text"><br>
State: <input class="state" type="text"><br>
PostalCode: <input class="postalCode" type="text"><br>
CountryCode: <input class="countryCode" type="text" value="US"><br>
<button class="cmdSend">Send</button>
`;

                // Assign appVars
                let street1 = $(myApp.appVars.topPane).find('.street1')[0];
                let street2 = $(myApp.appVars.topPane).find('.street2')[0];
                let city = $(myApp.appVars.topPane).find('.city')[0];
                let state = $(myApp.appVars.topPane).find('.state')[0];
                let postalCode = $(myApp.appVars.topPane).find('.postalCode')[0];
                let countryCode = $(myApp.appVars.topPane).find('.countryCode')[0];
                let cmdSend = $(myApp.appVars.topPane).find('.cmdSend')[0];

                $(cmdSend).on('click', async function () {

                    let streetLines = [];
                    if (street1 && street1.value) streetLines.push(street1.value);
                    if (street2 && street2.value) streetLines.push(street2.value);

                    // Need to sanitize!  For now assume one raw number
                    let appDataObj = {
                        apiPayload: {}
                    };
                    appDataObj.apiPayload = {
                        "addressesToValidate": [
                            {
                                "address": {
                                    "streetLines": streetLines,
                                    "city": city.value,
                                    "stateOrProvinceCode": state.value,
                                    "postalCode": postalCode.value,
                                    "countryCode": countryCode.value
                                }
                            }
                        ]
                    };

                    // Send DRP command
                    let response = await myApp.sendCmd("FedEx", "ValidateAddresses", appDataObj, true);

                    if (response) {
                        // Response to immediate command
                        myApp.appFuncs.displayResponse(response);
                    }
                });
            },
            showFindLocation: async () => {
                // Clear panes
                this.appFuncs.clearPanes();

                // Add form to top
                myApp.appVars.topPane.innerHTML = `
City: <input class="city" type="text"><br>
State: <input class="state" type="text"><br>
PostalCode: <input class="postalCode" type="text"><br>
CountryCode: <input class="countryCode" type="text" value="US"><br>
<button class="cmdSend">Send</button>
`;

                // Assign appVars
                let city = $(myApp.appVars.topPane).find('.city')[0];
                let state = $(myApp.appVars.topPane).find('.state')[0];
                let postalCode = $(myApp.appVars.topPane).find('.postalCode')[0];
                let countryCode = $(myApp.appVars.topPane).find('.countryCode')[0];
                let cmdSend = $(myApp.appVars.topPane).find('.cmdSend')[0];

                $(cmdSend).on('click', async function () {

                    // Need to sanitize!  For now assume one raw number
                    let appDataObj = {
                        apiPayload: {}
                    };
                    appDataObj.apiPayload = {
                        "locationsSummaryRequestControlParameters": {
                            "distance": {
                                "units": "MI",
                                "value": 2
                            }
                        },
                        "locationSearchCriterion": "ADDRESS",
                        "location": {
                            "address": {
                                "city": city.value,
                                "stateOrProvinceCode": state.value,
                                "postalCode": postalCode.value,
                                "countryCode": countryCode.value
                            }
                        }
                    };

                    // Send DRP command
                    let response = await myApp.sendCmd("FedEx", "FindLocation", appDataObj, true);

                    if (response) {
                        // Response to immediate command
                        myApp.appFuncs.displayResponse(response);
                    }
                });
            },
            showRateQuote: async () => {
                // Clear panes
                this.appFuncs.clearPanes();

                // Add form to top
                myApp.appVars.topPane.innerHTML = `
Shipper PostalCode: <input class="shipperPostalCode" type="text"><br>
Shipper CountryCode: <input class="shipperCountryCode" type="text" value="US"><br>
Recipient PostalCode: <input class="recipientPostalCode" type="text"><br>
Recipient CountryCode: <input class="recipientCountryCode" type="text" value="US"><br>
Weight (lbs): <input class="weight" type="text"><br>
<button class="cmdSend">Send</button>
`;

                // Assign appVars
                let shipperPostalCode = $(myApp.appVars.topPane).find('.shipperPostalCode')[0];
                let shipperCountryCode = $(myApp.appVars.topPane).find('.shipperCountryCode')[0];
                let recipientPostalCode = $(myApp.appVars.topPane).find('.recipientPostalCode')[0];
                let recipientCountryCode = $(myApp.appVars.topPane).find('.recipientCountryCode')[0];
                let weight = $(myApp.appVars.topPane).find('.weight')[0];
                let cmdSend = $(myApp.appVars.topPane).find('.cmdSend')[0];

                $(cmdSend).on('click', async function () {

                    // Need to sanitize!  For now assume one raw number
                    let appDataObj = {
                        apiPayload: {}
                    };
                    appDataObj.apiPayload = {
                        "rateRequestControlParameters": {
                            "returnTransitTimes": true
                        },
                        "requestedShipment": {
                            "shipper": {
                                "address": {
                                    "postalCode": shipperPostalCode.value,
                                    "countryCode": `${shipperCountryCode.value}`
                                }
                            },
                            "recipient": {
                                "address": {
                                    "postalCode": recipientPostalCode.value,
                                    "countryCode": `${recipientCountryCode.value}`
                                }
                            },
                            "pickupType": "DROPOFF_AT_FEDEX_LOCATION",
                            "shippingChargesPayment": {
                                "paymentType": "SENDER",
                                "payor": {
                                    "responsibleParty": {
                                        "accountNumber": {
                                            "value": null // Gets populated by backend
                                        }
                                    }
                                }
                            },
                            "rateRequestType": [
                                "ACCOUNT",
                                "LIST"
                            ],
                            "requestedPackageLineItems": [
                                {
                                    "weight": {
                                        "units": "LB",
                                        "value": weight.value
                                    }
                                }
                            ]
                        }
                    };

                    // Send DRP command
                    let response = await myApp.sendCmd("FedEx", "QuoteRates", appDataObj, true);

                    if (response) {
                        // Response to immediate command
                        myApp.appFuncs.displayResponse(response);
                    }
                });
            },
            showCreateShipment: async () => {
                // Clear panes
                this.appFuncs.clearPanes();

                // Add form to top
                myApp.appVars.topPane.innerHTML = `
<div style="display: flex;">
    <div>
    Shipper:<br>
    Name: <input class="shipperName" type="text"><br>
    Company: <input class="shipperCompany" type="text"><br>
    Phone Number: <input class="shipperPhone" type="text"><br>
    Street 1: <input class="shipperStreet1" type="text"><br>
    Street 2: <input class="shipperStreet2" type="text"><br>
    City: <input class="shipperCity" type="text"><br>
    State: <input class="shipperState" type="text"><br>
    PostalCode: <input class="shipperPostalCode" type="text"><br>
    CountryCode: <input class="shipperCountryCode" type="text" value="US"></div>
    <div>
    Recipient:<br>
    Name: <input class="recipientName" type="text"><br>
    Company: <input class="recipientCompany" type="text"><br>
    Phone Number: <input class="recipientPhone" type="text"><br>
    Street 1: <input class="recipientStreet1" type="text"><br>
    Street 2: <input class="recipientStreet2" type="text"><br>
    City: <input class="recipientCity" type="text"><br>
    State: <input class="recipientState" type="text"><br>
    PostalCode: <input class="recipientPostalCode" type="text"><br>
    CountryCode: <input class="recipientCountryCode" type="text" value="US"></div>
    <div>
        Weight (lbs): <input class="weight" type="text"><br><br><br>
        <button class="cmdSend">Submit</button>
    </div>
</div>
`;

                // Assign appVars
                let shipperName = $(myApp.appVars.topPane).find('.shipperName')[0];
                let shipperCompany = $(myApp.appVars.topPane).find('.shipperCompany')[0];
                let shipperPhone = $(myApp.appVars.topPane).find('.shipperPhone')[0];
                let shipperStreet1 = $(myApp.appVars.topPane).find('.shipperStreet1')[0];
                let shipperStreet2 = $(myApp.appVars.topPane).find('.shipperStreet2')[0];
                let shipperCity = $(myApp.appVars.topPane).find('.shipperCity')[0];
                let shipperState = $(myApp.appVars.topPane).find('.shipperState')[0];
                let shipperPostalCode = $(myApp.appVars.topPane).find('.shipperPostalCode')[0];
                let shipperCountryCode = $(myApp.appVars.topPane).find('.shipperCountryCode')[0];

                let recipientName = $(myApp.appVars.topPane).find('.recipientName')[0];
                let recipientCompany = $(myApp.appVars.topPane).find('.recipientCompany')[0];
                let recipientPhone = $(myApp.appVars.topPane).find('.recipientPhone')[0];
                let recipientStreet1 = $(myApp.appVars.topPane).find('.recipientStreet1')[0];
                let recipientStreet2 = $(myApp.appVars.topPane).find('.recipientStreet2')[0];
                let recipientCity = $(myApp.appVars.topPane).find('.recipientCity')[0];
                let recipientState = $(myApp.appVars.topPane).find('.recipientState')[0];
                let recipientPostalCode = $(myApp.appVars.topPane).find('.recipientPostalCode')[0];
                let recipientCountryCode = $(myApp.appVars.topPane).find('.recipientCountryCode')[0];
                let weight = $(myApp.appVars.topPane).find('.weight')[0];
                let cmdSend = $(myApp.appVars.topPane).find('.cmdSend')[0];

                $(cmdSend).on('click', async function () {

                    // Need to sanitize!  For now assume one raw number
                    let appDataObj = {
                        apiPayload: {}
                    };

                    let shipperStreetLines = [shipperStreet1.value];
                    if (shipperStreet2.value) shipperStreetLines.push(shipperStreet2.value);

                    let recipientStreetLines = [recipientStreet1.value];
                    if (recipientStreet2.value) recipientStreetLines.push(recipientStreet2.value);

                    appDataObj.apiPayload = {
                        "labelResponseOptions": "LABEL",
                        "requestedShipment": {
                            "shipper": {
                                "contact": {
                                    "personName": shipperName.value,
                                    "phoneNumber": shipperPhone.value,
                                    "companyName": shipperCompany.value
                                },
                                "address": {
                                    "streetLines": shipperStreetLines,
                                    "city": shipperCity.value,
                                    "stateOrProvinceCode": shipperState.value,
                                    "postalCode": shipperPostalCode.value,
                                    "countryCode": shipperCountryCode.value
                                }
                            },
                            "recipients": [
                                {
                                    "contact": {
                                        "personName": recipientName.value,
                                        "phoneNumber": recipientPhone.value,
                                        "companyName": recipientCompany.value
                                    },
                                    "address": {
                                        "streetLines": recipientStreetLines,
                                        "city": recipientCity.value,
                                        "stateOrProvinceCode": recipientState.value,
                                        "postalCode": recipientPostalCode.value,
                                        "countryCode": recipientCountryCode.value
                                    }
                                }
                            ],
                            "shipDatestamp": "2020-07-03",
                            "serviceType": "STANDARD_OVERNIGHT",
                            "packagingType": "FEDEX_PAK",
                            "pickupType": "USE_SCHEDULED_PICKUP",
                            "blockInsightVisibility": false,
                            "shippingChargesPayment": {
                                "paymentType": "SENDER"
                            },
                            "labelSpecification": {
                                "imageType": "PDF",
                                "labelStockType": "PAPER_LETTER"
                            },
                            "requestedPackageLineItems": [
                                {
                                    "weight": {
                                        "value": weight.value,
                                        "units": "LB"
                                    }
                                }
                            ]
                        },
                        "accountNumber": {
                            "value": null // Gets populated by backend
                        }
                    };
                    
                    // Send DRP command
                    let response = await myApp.sendCmd("FedEx", "CreateShipment", appDataObj, true);

                    if (response) {
                        // Response to immediate command
                        myApp.appFuncs.displayResponse(response);

                        if (!response.output) return;

                        let shipments = response.output.transactionShipments;

                        // Loop over shipments
                        for (let i = 0; i < shipments.length; i++) {

                            let pieceResponses = shipments[i].pieceResponses;

                            // Loop over pieceResponses
                            for (let j = 0; j < pieceResponses.length; j++) {

                                // Loop over returned labels, display
                                let documents = pieceResponses[j].packageDocuments;
                                for (let k = 0; k < documents.length; k++) {
                                    let newWindowObj = new VDMWindow({ title: `FedEx Shipping Label - ${pieceResponses[j].trackingNumber}`, sizeX: 700, sizeY: 500 });
                                    myApp.vdmDesktop.newWindow(newWindowObj);
                                    let imageBase64 = documents[k].encodedLabel;
                                    newWindowObj.windowParts["data"].innerHTML = "<iframe width='100%' height='100%' src='data:application/pdf;base64, " + imageBase64 + "'></iframe>";
                                }
                            }
                        }
                    }
                });
            },
            displayResponse: (displayData) => {
                let appDataObj = null;
                try {
                    appDataObj = JSON.parse(displayData);
                }
                catch (ex) {
                    appDataObj = displayData;
                }
                let displayText = JSON.stringify(appDataObj, null, 2);
                myApp.appVars.bottomPane.innerHTML = "<pre style='font-size: 12px;line-height: 12px;color: #DDD;height: 100%;'>" + displayText + "</pre>";
            }
        };
    }

    async runStartup() {
        let myApp = this;

        // Split data pane vertically
        var newPanes = myApp.splitPaneVertical(myApp.windowParts["data"], 130, false, false);
        myApp.appVars.topPane = newPanes[0];
        myApp.appVars.hDiv = newPanes[1];
        myApp.appVars.bottomPane = newPanes[2];

        myApp.appVars.topPane.style['overflow-y'] = "auto";
        myApp.appVars.topPane.style['font-size'] = "12px";
        myApp.appVars.bottomPane.style['user-select'] = "text";
        myApp.appVars.bottomPane.style['background'] = "#444";

    }
});
//# sourceURL=vdm-app-FedEx.js
