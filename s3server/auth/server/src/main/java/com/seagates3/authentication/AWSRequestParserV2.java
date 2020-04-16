/*
 * COPYRIGHT 2015 SEAGATE LLC
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
 * Original author:  Arjun Hariharan <arjun.hariharan@seagate.com>
 * Original creation date: 22-Oct-2015
 */
package com.seagates3.authentication;

import io.netty.handler.codec.http.FullHttpRequest;
import io.netty.handler.codec.http.HttpHeaders;
import java.util.Map;
import java.util.Arrays;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.seagates3.exception.InvalidTokenException;

public class AWSRequestParserV2 extends AWSRequestParser {

    private final Logger LOGGER =
            LoggerFactory.getLogger(AWSRequestParserV2.class.getName());

    @Override
    public ClientRequestToken parse(FullHttpRequest httpRequest) throws InvalidTokenException {
        ClientRequestToken clientRequestToken = new ClientRequestToken();
        clientRequestToken.setSignedVersion(ClientRequestToken.AWSSigningVersion.V2);

        parseRequestHeaders(httpRequest, clientRequestToken);

        HttpHeaders httpHeaders = httpRequest.headers();
        authHeaderParser(httpHeaders.get("authorization"), clientRequestToken);

        return clientRequestToken;
    }

    @Override
    public ClientRequestToken parse(Map<String, String> requestBody) throws InvalidTokenException {
        ClientRequestToken clientRequestToken = new ClientRequestToken();
        clientRequestToken.setSignedVersion(ClientRequestToken.AWSSigningVersion.V2);

        parseRequestHeaders(requestBody, clientRequestToken);

        authHeaderParser(requestBody.get("Authorization"), clientRequestToken);

        return clientRequestToken;
    }

    /*
     * Authorization header is in following format
     * Authorization: "AWS" + " " + AWSAccessKeyId + ":" + Signature
     *
     * Sample Authorization Header
     * Authorization: AWS AKIAIOSFODNN7EXAMPLE:MyyxeRY7whkBe+bq8fHCL/2kKUg=
     */
    @Override
    public void authHeaderParser(String authorizationHeaderValue,
            ClientRequestToken clientRequestToken) throws InvalidTokenException {
        String[] tokens, subTokens;
        String errMsg = "Invalid client request token found while parsing request";

        tokens = authorizationHeaderValue.split(" ");
        if (tokens.length < 2) {
            LOGGER.error("Found "+ tokens.length + " tokens "+ Arrays.toString(tokens) +
                       " while parsing but required atleast 2 subtokens.");
            throw new InvalidTokenException(errMsg);
        }
        subTokens = tokens[1].split(":");
        if (subTokens.length < 2) {
            LOGGER.error("Found "+ subTokens.length + " subtokens "+ Arrays.toString(subTokens) +
                       " while parsing but required atleast 2 subtokens.");
            throw new InvalidTokenException(errMsg);
        }

        clientRequestToken.setAccessKeyId(subTokens[0]);
        clientRequestToken.setSignature(subTokens[1]);
    }
}
