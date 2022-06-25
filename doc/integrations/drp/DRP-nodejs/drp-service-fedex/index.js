'use strict';
const DRP_Node = require('drp-mesh').Node;
const DRP_Service = require('drp-mesh').Service;

let axios = require('axios');

class FedExAPIMgr extends DRP_Service {

    /**
    * @param {string} serviceName Service Name
    * @param {drpNode} drpNode DRP Node
    * @param {number} priority Priority (lower better)
    * @param {number} weight Weight (higher better)
    * @param {string} scope Scope [local|zone|global(defaut)]
    * @param {string} apiKey API Key
    * @param {string} secretKey Secret Key
    * @param {string} shippingAccount Shipping Account Number
    * @param {string} baseURL Base Service URL
    */
    constructor(serviceName, drpNode, priority, weight, scope, apiKey, secretKey, shippingAccount, baseURL) {
        super(serviceName, drpNode, "FedExAPIMgr", null, false, priority, weight, drpNode.Zone, scope, null, ["TrackingUpdates","CreateShipment"], 1);
        let thisAPIMgr = this;

        this.apiKey = apiKey || null;
        this.secretKey = secretKey || null;
        this.shippingAccount = shippingAccount || null;
        this.baseURL = baseURL || "https://apis-sandbox.fedex.com";
        /** @type {axios.default} */
        this.restAgent = null;
        this.maxTokenAgeSec = 3600;
        this.authToken = null;
        this.tokenAcquiredTimestamp = null;

        // Declare Tracking Update topic
        this.DRPNode.TopicManager.CreateTopic("TrackingUpdates", 100);

        // Declare Create Shipment topic
        this.DRPNode.TopicManager.CreateTopic("CreateShipment", 100);

        // Dirty way to make this function available for traversal
        this.PostShipmentUpdate = this.PostShipmentUpdate;

        this.ClientCmds = {
            ValidateAddresses: async (params) => { return await thisAPIMgr.ValidateAddresses(params.apiPayload); },
            TrackByTrackingNumbers: async (params) => { return await thisAPIMgr.TrackByTrackingNumbers(params.apiPayload); },
            FindLocation: async (params) => { return await thisAPIMgr.FindLocation(params.apiPayload); },
            QuoteRates: async (params) => { return await thisAPIMgr.QuoteRates(params.apiPayload); },
            CreateShipment: async (params) => { return await thisAPIMgr.CreateShipment(params.apiPayload, params.AuthInfo); },

            testAddressValidation: async (params) => { return await thisAPIMgr.ValidateAddress("7372 PARKRIDGE BLVD", "APT 286", "IRVING", "TX", "75063-8659", "US"); },
            testTrackByTrackingNumbers: async (params) => {
                return await thisAPIMgr.TrackByTrackingNumbers({
                    "includeDetailedScans": true,
                    "trackingInfo": [{ "trackingNumberInfo": { "trackingNumber": "794843185271" } }]
                });
            },
            testFindLocation: async (params) => {
                return await thisAPIMgr.FindLocation({
                    "locationsSummaryRequestControlParameters": {
                        "distance": {
                            "units": "MI",
                            "value": 2
                        }
                    },
                    "locationSearchCriterion": "ADDRESS",
                    "location": {
                        "address": {
                            "city": "Beverly Hills",
                            "stateOrProvinceCode": "CA",
                            "postalCode": "90210",
                            "countryCode": "US"
                        }
                    }
                });
            },
            testRateQuote: async (params) => {
                return await thisAPIMgr.QuoteRates({
                    "rateRequestControlParameters": {
                        "returnTransitTimes": true
                    },
                    "requestedShipment": {
                        "shipper": {
                            "address": {
                                "postalCode": 65247,
                                "countryCode": "US"
                            }
                        },
                        "recipient": {
                            "address": {
                                "postalCode": 75063,
                                "countryCode": "US"
                            }
                        },
                        "pickupType": "DROPOFF_AT_FEDEX_LOCATION",
                        "shippingChargesPayment": {
                            "paymentType": "SENDER",
                            "payor": {
                                "responsibleParty": {
                                    "accountNumber": {
                                        "value": `${thisAPIMgr.shippingAccount}`
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
                                    "value": 10
                                }
                            }
                        ]
                    }
                });
            },
            testCreateShipment: async (params) => {
                return await thisAPIMgr.CreateShipment({
                    "labelResponseOptions": "LABEL",
                    "requestedShipment": {
                        "shipper": {
                            "contact": {
                                "personName": "SHIPPER NAME",
                                "phoneNumber": 1234567890,
                                "companyName": "Shipper Company Name"
                            },
                            "address": {
                                "streetLines": [
                                    "SHIPPER STREET LINE 1"
                                ],
                                "city": "HARRISON",
                                "stateOrProvinceCode": "AR",
                                "postalCode": 72601,
                                "countryCode": "US"
                            }
                        },
                        "recipients": [
                            {
                                "contact": {
                                    "personName": "RECIPIENT NAME",
                                    "phoneNumber": 1234567890,
                                    "companyName": "Recipient Company Name"
                                },
                                "address": {
                                    "streetLines": [
                                        "RECIPIENT STREET LINE 1",
                                        "RECIPIENT STREET LINE 2"
                                    ],
                                    "city": "Collierville",
                                    "stateOrProvinceCode": "TN",
                                    "postalCode": 38017,
                                    "countryCode": "US"
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
                                    "value": 10,
                                    "units": "LB"
                                }
                            }
                        ]
                    },
                    "accountNumber": {
                        "value": `${thisAPIMgr.shippingAccount}`
                    }
                }, params.AuthInfo);
            }
        };
    }

    // Verify that the user is authorized to perform the action
    async AuthorizeAction(userGroups, functionName, targetAddress, actionCost) {

        let approved = false;

        // For demo purposes - statically define company locations and rules
        let companyLocationList = [
            { street1: "123 Pine Ln", street2: null, city: "Someplace", stateOrProvinceCode: "TN", postalCode: "12345", countryCode: "US" },
            { street1: "1897 Oak St", street2: "Suite 12", city: "Somewhere", stateOrProvinceCode: "CA", postalCode: "23456", countryCode: "US" },
            { street1: "8901 Cedar Ave", street2: null, city: "Somehow", stateOrProvinceCode: "MS", postalCode: "34567", countryCode: "US" }
        ];

        let rulesObj = {
            "CreateShipment": [
                { groupName: "Users", approvedLimit: 25.00 },
                { groupName: "Users", approvedLimit: 100.00, locations: companyLocationList },
                { groupName: "Admins", approvedLimit: 1000.00 }
            ]
        };

        // Check organization shipping rules
        // Loop over groups, check against rulesObj
        for (let i = 0; i < rulesObj[functionName].length; i++) {
            let thisRule = rulesObj[functionName][i];

            // If the group name doesn't match, skip
            if (!userGroups.includes(thisRule.groupName)) continue;

            // If the rule has an approvedLimit and the cost is too high, skip
            if (thisRule.approvedLimit && actionCost > thisRule.approvedLimit) continue;

            // If the rule has a list of locations and this one doesn't match, skip
            if (thisRule.locations) {
                let locationMatched = false;
                for (let j = 0; j < thisRule.locations; j++) {
                    let checkLocation = thisRule.location[j];
                    // TODO - find a cleaner way to do this comparison
                    if (checkLocation.streetLines[0] === targetAddress.streetLines[0] &&
                        checkLocation.streetLines[1] === targetAddress.streetLines[1] &&
                        checkLocation.city === targetAddress.city &&
                        checkLocation.stateOrProvinceCode === targetAddress.stateOrProvinceCode &&
                        checkLocation.postalCode === targetAddress.postalCode &&
                        checkLocation.countryCode === targetAddress.countryCode) {
                        locationMatched = true;
                    }
                }
                // If the location wasn't matched, skip
                if (!locationMatched) continue;
            }

            // This rule matches; approve
            approved = true;
            break;
        }

        return approved;
    }

    // Verify current token is valid; if not, acquire one
    async CheckToken() {
        let thisAPIMgr = this;
        // If we don't have a token or the token is expired, get another one
        // Is the token null or over X seconds old?
        let remainingTokenTimeSecs = thisAPIMgr.maxTokenAgeSec - (Date.now() - thisAPIMgr.tokenAcquiredTimestamp) / 1000;
        if (!thisAPIMgr.authToken || remainingTokenTimeSecs < 0) {
            thisAPIMgr.DRPNode.log("Acquiring new token");
            thisAPIMgr.restAgent = axios.create({
                baseURL: thisAPIMgr.baseURL,
                 timeout: 10000,
                 headers: {},
                 proxy: false
             });
            let response = await thisAPIMgr.restAgent.post("/oauth/token", `grant_type=client_credentials&client_id=${thisAPIMgr.apiKey}&client_secret=${thisAPIMgr.secretKey}`, { headers: { "Content-Type": "application/x-www-form-urlencoded" } });
            let responseObj = response.data;

            if (responseObj && responseObj['access_token']) {
                thisAPIMgr.authToken = responseObj['access_token'];
                thisAPIMgr.tokenAcquiredTimestamp = Date.now();
                thisAPIMgr.maxTokenAgeSec = responseObj['expires_in'];
                thisAPIMgr.restAgent.defaults.headers.common['Authorization'] = `Bearer ${thisAPIMgr.authToken}`;
            } else {
                return "Could not authenticate";
            }
        } else {
            thisAPIMgr.DRPNode.log(`Using cached token, remaining time: ${remainingTokenTimeSecs} seconds`);
        }
    }

    async ValidateAddresses(addressCriteria) {
        let thisAPIMgr = this;

        let tokenCheckErr = await thisAPIMgr.CheckToken();
        if (tokenCheckErr) return tokenCheckErr;

        let response = null;
        try {
            response = await thisAPIMgr.restAgent.post("/address/v1/addresses/resolve", addressCriteria, { headers: { "Content-Type": "application/json" } });
        } catch (ex) {
            response = ex.response;
        }
        return response.data;
    }

    async ValidateAddress(street1, street2, city, state, postalCode, countryCode) {
        let thisAPIMgr = this;

        let tokenCheckErr = await thisAPIMgr.CheckToken();
        if (tokenCheckErr) return tokenCheckErr;

        let streetLines = [];
        streetLines.push(street1);
        if (street2) streetLines.push(street2);
        let addressToValidate = {
            "addressesToValidate": [
                {
                    "address": {
                        "streetLines": streetLines,
                        "city": city,
                        "stateOrProvinceCode": state,
                        "postalCode": postalCode,
                        "countryCode": countryCode
                    }
                }
            ]
        };

        let response = null;
        try {
            response = await thisAPIMgr.restAgent.post("/address/v1/addresses/resolve", addressToValidate, { headers: { "Content-Type": "application/json" } });
        } catch (ex) {
            response = ex.response;
        }
        return response.data;
    }

    async TrackByTrackingNumbers(trackingCriteria) {
        let thisAPIMgr = this;

        let tokenCheckErr = await thisAPIMgr.CheckToken();
        if (tokenCheckErr) return tokenCheckErr;

        let response = null;
        try {
            response = await thisAPIMgr.restAgent.post("/track/v1/trackingnumbers", trackingCriteria, { headers: { "Content-Type": "application/json" } });
        } catch (ex) {
            response = ex.response;
        }
        return response.data;
    }

    async FindLocation(locationCriteria) {
        let thisAPIMgr = this;

        let tokenCheckErr = await thisAPIMgr.CheckToken();
        if (tokenCheckErr) return tokenCheckErr;

        let response = null;
        try {
            response = await thisAPIMgr.restAgent.post("/location/v1/locations", locationCriteria, { headers: { "Content-Type": "application/json" } });
        } catch (ex) {
            response = ex.response;
        }
        return response.data;
    }

    async QuoteRates(quoteCriteria) {
        let thisAPIMgr = this;

        // Assign account number
        // TODO - make dynamic by group?
        quoteCriteria.requestedShipment.shippingChargesPayment.payor.responsibleParty.accountNumber.value = thisAPIMgr.shippingAccount;

        let tokenCheckErr = await thisAPIMgr.CheckToken();
        if (tokenCheckErr) return tokenCheckErr;

        let response = null;
        try {
            response = await thisAPIMgr.restAgent.post("/rate/v1/rates/quotes", quoteCriteria, { headers: { "Content-Type": "application/json" } });
        } catch (ex) {
            response = ex.response;
        }
        return response.data;
    }

    async CreateShipment(shipmentCriteria, authInfo) {
        let thisAPIMgr = this;

        // Assign account number
        // TODO - make dynamic by group?
        shipmentCriteria.accountNumber.value = thisAPIMgr.shippingAccount;

        // Make sure we're authorized to send to all the recipients
        for (let i = 0; i < shipmentCriteria.requestedShipment.recipients.length; i++) {
            // TODO - Get actual rate quote, dummy value for now
            let quotedCost = 1;

            let allowed = await thisAPIMgr.AuthorizeAction(authInfo.userInfo.Groups, "CreateShipment", shipmentCriteria.requestedShipment.recipients[i].address, quotedCost);
            if (!allowed) return "User unauthorized to send this shipment";
        }

        let tokenCheckErr = await thisAPIMgr.CheckToken();
        if (tokenCheckErr) return tokenCheckErr;

        let response = null;
        try {
            response = await thisAPIMgr.restAgent.post("/ship/v1/shipments", shipmentCriteria, { headers: { "Content-Type": "application/json" } });
            this.DRPNode.TopicManager.SendToTopic("CreateShipment", response.data);
        } catch (ex) {
            response = ex.response;
        }
        return response.data;
    }

    async PostShipmentUpdate(params) {
        this.DRPNode.TopicManager.SendToTopic("TrackingUpdates", params.body);
    }
}

module.exports = FedExAPIMgr;