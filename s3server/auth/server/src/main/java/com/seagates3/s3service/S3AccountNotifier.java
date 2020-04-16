/*
 * COPYRIGHT 2018 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.seagate.com/contact
 *
 * Original author: Basavaraj Kirunge
 * Original creation date: 05-07-2018
 */

package com.seagates3.s3service;

import java.io.IOException;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.Arrays;

import org.apache.commons.httpclient.HttpStatus;
import org.apache.http.HttpResponse;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.exception.S3RequestInitializationException;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.generator.AuthenticationResponseGenerator;


public class S3AccountNotifier {
    private final Logger LOGGER = LoggerFactory.getLogger(S3AccountNotifier.class.getName());
    public AuthenticationResponseGenerator responseGenerator =
                                                 new AuthenticationResponseGenerator();

    private String getEndpointURL() {
        String s3EndPoint = AuthServerConfig.getDefaultEndpoint();
        String s3ConnectMode = "http";
        if (AuthServerConfig.isEnableHttpsToS3()) {
        	s3ConnectMode = "https";
        };
        //s3ConnectMode will have value http or https
        String endPoint = s3ConnectMode + "://" + s3EndPoint;
        return endPoint;
    }

    /**
     * Sends New Account notification to S3 Server
     * @param accountId
     * @param accessKey
     * @param secretKey
     * @return
     */
    public ServerResponse notifyNewAccount(String accountId, String accessKey,
                                                           String secretKey) {

        S3RestClient s3Client = new S3RestClient();

        String resourceUrl = "/account/" + accountId;
        s3Client.setResource(resourceUrl);
        s3Client.setCredentials(accessKey, secretKey);

        s3Client.setURL(getEndpointURL() + resourceUrl);

        //Set S3 Management API header
        s3Client.setHeader("x-seagate-mgmt-api", "true");

        S3HttpResponse resp = null;
        try {
            resp = s3Client.postRequest();
        } catch (S3RequestInitializationException e) {
            LOGGER.error("S3 REST Request initialization failed with"
                    + "error message:" + e.getMessage());
            return responseGenerator.internalServerError();
        } catch (IOException e) {
            StringWriter stack = new StringWriter();
            e.printStackTrace(new PrintWriter(stack));
            LOGGER.error("S3 REST Request failed error stack:"
                    + stack.toString());
            return responseGenerator.internalServerError();
        }

        int httpCode = resp.getHttpCode();
        if(httpCode == HttpStatus.SC_CREATED) {
            LOGGER.info("New Account [" + accountId +"] create notification "
                    + "to S3 sent successfully");
            return responseGenerator.ok();
        } else {
            LOGGER.error("New Account [" + accountId +"] create notification"
                    + " to S3 failed with httpCode:" + httpCode);
            return responseGenerator.internalServerError();
        }

    }

    /**
     * Sends Delete notification to S3 Server
     * @param accountId
     * @param accessKey
     * @param secretKey
     * @return ServerResponse
     */
   public
    ServerResponse notifyDeleteAccount(String accountId, String accessKey,
                                       String secretKey, String securityToken) {
        S3RestClient s3Client = new S3RestClient();

        String resourceUrl = "/account/" + accountId;

        s3Client.setResource(resourceUrl);

        s3Client.setCredentials(accessKey, secretKey);
        s3Client.setURL(getEndpointURL() + resourceUrl);
        //Set S3 Management API header
        s3Client.setHeader("x-seagate-mgmt-api", "true");
        if (securityToken != null) {
          s3Client.setHeader("x-amz-security-token", securityToken);
        }
        S3HttpResponse resp = null;
        try {
            resp = s3Client.deleteRequest();
        } catch (S3RequestInitializationException e) {
            LOGGER.error("S3 REST Request initialization failed with"
                                  + "error message:" + e.getMessage());
            return responseGenerator.internalServerError();
        } catch (IOException e) {
            StringWriter stack = new StringWriter();
            e.printStackTrace(new PrintWriter(stack));
            LOGGER.error("S3 REST Request failed error stack:"
                    + stack.toString() );
            return responseGenerator.internalServerError();
        }

        int httpCode = resp.getHttpCode();
        if( httpCode == HttpStatus.SC_CONFLICT) {
            LOGGER.error("Account [" + accountId +"] is not empty, cannot"
                    + " delete account. httpCode:" + httpCode);
            return responseGenerator.accountNotEmpty();
        }

        LOGGER.info("Account [" + accountId +"] delete notification "
                + "to S3 sent successfully");
        if (httpCode == HttpStatus.SC_NO_CONTENT) {
            LOGGER.info("Account [" + accountId + "] does not own any "
                    + "resources, safe to delete");
            return responseGenerator.ok();
        }
        LOGGER.error("Account [" + accountId +"] delete notification to S3"
                + "failed with httpCode:" + httpCode);
        return responseGenerator.internalServerError();
    }

}

