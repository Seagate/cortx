'use strict';
const {jsonToStructProto} =require('./structjson.js')
// const d2=require('@google-cloud/dialogflow')
const dialogflow=require('dialogflow');
const config=require('../config/keys')

const googleAuth = require('google-oauth-jwt');

const projectID=config.googleProjectID
const credentials={
    client_email:config.googleClientEmail,
    private_key:config.googlePrivateKey
}

const sessionClient= new dialogflow.SessionsClient({projectID:projectID,credentials:credentials});
const sessionPath=sessionClient.sessionPath(config.googleProjectID,config.dialogFlowSessionID);



module.exports={

    getToken: async function() {
      return new Promise((resolve) => {
          googleAuth.authenticate(
            {
              email: config.googleClientEmail,
              key: config.googlePrivateKey,
              scopes: ['https://www.googleapis.com/auth/cloud-platform'],
            },
            (err, token) => {
                resolve(token);
            },
        );
      });
    },
    
    textQuery:async function(text,parameters){
        let self=module.exports;
        const request = {
            session: sessionPath,
            queryInput: {
              text: {
                // The query to send to the dialogflow agent
                text:text,
                // The language used by the client (en-US)
                languageCode: config.dialogFlowSessionLanguageCode,
              },
            },
            queryParams:{
                payload:{
                    data:parameters
                }
            }
          };
          let responses=await sessionClient.detectIntent(request);
          responses = await self.handleAction(responses);
          return responses;
    },
    eventQuery:async function(event,parameters={}){
        let self=module.exports;
        const request = {
            session: sessionPath,
            queryInput: {
              event: {
                // The query to send to the dialogflow agent
                name:event,
                parameters:jsonToStructProto(parameters),
                // The language used by the client (en-US)
                languageCode: config.dialogFlowSessionLanguageCode,
              },
            },
            
          };
          let responses=await sessionClient.detectIntent(request);
          responses = await self.handleAction(responses);
          return responses;
    },
    handleAction: function(responses){
        let self = module.exports;
        let queryResult = responses[0].queryResult;

        switch (queryResult.action) {
            case 'recommendcourses-yes':
                if (queryResult.allRequiredParamsPresent) {
                    self.saveRegistration(queryResult.parameters.fields);
                }
                break;
        }

        return responses;
    },
}