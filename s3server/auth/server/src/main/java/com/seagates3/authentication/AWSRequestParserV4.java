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
 * Original creation date: 17-Sep-2014
 */
package com.seagates3.authentication;

import io.netty.buffer.ByteBuf;
import io.netty.handler.codec.http.FullHttpRequest;
import io.netty.handler.codec.http.HttpHeaders;
import io.netty.util.CharsetUtil;
import java.util.Map;
import java.util.Arrays;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.seagates3.exception.InvalidTokenException;

public class AWSRequestParserV4 extends AWSRequestParser {

    private final Logger LOGGER =
            LoggerFactory.getLogger(AWSRequestParserV4.class.getName());


    @Override
    public ClientRequestToken parse(FullHttpRequest httpRequest) throws InvalidTokenException {
        ClientRequestToken clientRequestToken = new ClientRequestToken();
        clientRequestToken.setSignedVersion(ClientRequestToken.AWSSigningVersion.V4);
        clientRequestToken.setRequestPayload(getRequestPayload(httpRequest));

        parseRequestHeaders(httpRequest, clientRequestToken);

        HttpHeaders httpHeaders = httpRequest.headers();
        authHeaderParser(httpHeaders.get("authorization"), clientRequestToken);

        return clientRequestToken;
    }

    @Override
    public ClientRequestToken parse(Map<String, String> requestBody) throws InvalidTokenException {
        ClientRequestToken clientRequestToken = new ClientRequestToken();
        clientRequestToken.setSignedVersion(ClientRequestToken.AWSSigningVersion.V4);

        if (requestBody.get("RequestPayload") != null) {
            clientRequestToken.setRequestPayload(requestBody.get("RequestPayload"));
        } else {
            clientRequestToken.setRequestPayload("");
        }

        parseRequestHeaders(requestBody, clientRequestToken);

        authHeaderParser(requestBody.get("authorization"), clientRequestToken);

        return clientRequestToken;
    }

    /**
     * Authorization header is in following format (Line break added for
     * readability) Authorization: algorithm Credential=access key ID/credential
     * scope, SignedHeaders=SignedHeaders, Signature=signature
     *
     * Sample Authorization Header
     *
     * Authorization: AWS4-HMAC-SHA256
     * Credential=AKIAJTYX36YCKQSAJT7Q/20150810/us-east-1/iam/aws4_request,
     * SignedHeaders=content-type;host;user-agent;x-amz-content-sha256;x-amz-date,
     * Signature=b751427a69f5bb76fb171fec45bdb1e8f664fac7f7c23c983f9c7361bb382d76
     *
     * Authorization header value is broken into 3 chunks. tokens[0] : algorithm
     * Credential=access key ID/credential scope tokens[1] :
     * SignedHeaders=SignedHeaders tokens[2] : Signature=signature
     *
     * Credential Scope = date/region/service/aws4_request
     *
     * @param authorizationHeaderValue Authorization Header
     * @param clientRequestToken
     * @throws InvalidTokenException
     */
    @Override
    protected void authHeaderParser(String authorizationHeaderValue,
            ClientRequestToken clientRequestToken) throws InvalidTokenException  {
        String errMsg = "Invalid client request token found while parsing request";
        String[] tokens = authorizationHeaderValue.split(",");
        if (tokens.length < 3) {
            LOGGER.error("Found "+ tokens.length + " tokens "+ Arrays.toString(tokens) +
                       " while parsing but required atleast 3 credtokens.");
            throw new InvalidTokenException(errMsg);
        }

        String[] subTokens, credTokens, credScopeTokens;

        subTokens = tokens[0].split(" ");
        if (subTokens.length < 2) {
            LOGGER.error("Found "+ subTokens.length + " subtokens "+ Arrays.toString(subTokens) +
                       " while parsing but required atleast 2 subtokens.");
            throw new InvalidTokenException(errMsg);
        }
        clientRequestToken.setSigningAlgorithm(subTokens[0]);

        credTokens = subTokens[1].split("/", 2);
        if (credTokens.length < 2) {
            LOGGER.error("Found "+ credTokens.length + " credtokens " + Arrays.toString(credTokens) +
                       " while parsing but required atleast 2 credtokens.");
            throw new InvalidTokenException(errMsg);
        }
        clientRequestToken.setAccessKeyId(credTokens[0].split("=")[1]);

        String credentialScope = credTokens[1];
        clientRequestToken.setCredentialScope(credentialScope);

        credScopeTokens = credentialScope.split("/");
        if (credScopeTokens.length < 3) {
            LOGGER.error("Found " + credScopeTokens.length + " credscopetokens "+ Arrays.toString(credScopeTokens) +
                       " while parsing but required atleast 3 credscopetokens.");
            throw new InvalidTokenException(errMsg);
        }
        clientRequestToken.setDate(credScopeTokens[0]);
        clientRequestToken.setRegion(credScopeTokens[1]);
        clientRequestToken.setService(credScopeTokens[2]);

        subTokens = tokens[1].split("=");
        if (subTokens.length < 2) {
            LOGGER.error("Found " + subTokens.length + " subtokens "+ Arrays.toString(subTokens) +
                       " while parsing but required atleast 2 subtokens.");
            throw new InvalidTokenException(errMsg);
        }
        clientRequestToken.setSignedHeaders(subTokens[1]);
        clientRequestToken.setSignature(tokens[2].split("=")[1]);
    }

    /**
     * Get the request pay load from HTTP Request object.
     *
     * @param httpRequest HTTP Request object.
     * @return request payload.
     */
    private String getRequestPayload(FullHttpRequest httpRequest) {
        ByteBuf content = httpRequest.content();
        StringBuilder payload = new StringBuilder();
        payload.append(content.toString(CharsetUtil.UTF_8));

        return payload.toString();
    }
}
