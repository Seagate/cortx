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

import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.exception.InvalidTokenException;

import io.netty.handler.codec.http.FullHttpRequest;
import io.netty.handler.codec.http.HttpHeaders;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.TreeMap;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public abstract class AWSRequestParser {

    /**
     * Parse the client request.
     *
     * @param httpRequest
     * @return
     * @throws InvalidTokenException
     */
    public abstract ClientRequestToken parse(FullHttpRequest httpRequest)
            throws InvalidTokenException;

    /**
     *
     * @param requestBody
     * @return
     * @throws InvalidTokenException
     */
    public abstract ClientRequestToken parse(Map<String, String> requestBody)
            throws InvalidTokenException;

    /**
     * Parse the authorization header.
     *
     * @param authorizationHeaderValue
     * @param clientRequestToken
     * @throws InvalidTokenException
     */
    protected abstract void authHeaderParser(String authorizationHeaderValue,
            ClientRequestToken clientRequestToken) throws InvalidTokenException;

    /**
     * Parse the request headers.
     *
     * @param httpRequest
     * @param clientRequestToken
     */
    protected void parseRequestHeaders(FullHttpRequest httpRequest,
            ClientRequestToken clientRequestToken) {
        HttpHeaders httpHeaders = httpRequest.headers();
        Map<String, String> requestHeaders
                = new TreeMap<>(String.CASE_INSENSITIVE_ORDER);

        for (Entry<String, String> e : httpHeaders) {
            requestHeaders.put(e.getKey(), e.getValue());
        }

        clientRequestToken.setHttpMethod(httpRequest.getMethod().toString());

        String host = httpHeaders.get("host");
        parseHostHeader(clientRequestToken, host);

        String[] tokens = httpRequest.getUri().split("\\?", -1);
        clientRequestToken.setUri(tokens[0]);

        if (tokens.length == 2) {
            clientRequestToken.setQuery(tokens[1]);
        } else {
            clientRequestToken.setQuery("");
        }

        clientRequestToken.setRequestHeaders(requestHeaders);
    }

    /**
     * Parse the client request.
     *
     * @param requestBody
     * @param clientRequestToken
     */
    protected void parseRequestHeaders(Map<String, String> requestBody,
            ClientRequestToken clientRequestToken) {

        clientRequestToken.setHttpMethod(requestBody.get("Method"));

        String host = requestBody.get("host");
        parseHostHeader(clientRequestToken, host);

        clientRequestToken.setUri(requestBody.get("ClientAbsoluteUri"));
        clientRequestToken.setQuery(requestBody.get("ClientQueryParams"));

        clientRequestToken.setRequestHeaders(requestBody);
    }

    /*
     * Parse host header, and identify if request is virtual hosted-style and
     * set bucket name if it is.
     */
    private void parseHostHeader(ClientRequestToken clientRequestToken, String host) {
        List<String> uriEndpoints = new ArrayList<>();
        List<String> s3Endpoints = Arrays.asList(AuthServerConfig.getEndpoints());
        String patternString;
        Pattern pattern;
        Matcher matcher;

        uriEndpoints.add(AuthServerConfig.getDefaultEndpoint());
        uriEndpoints.addAll(s3Endpoints);

        for (String endPoint : uriEndpoints) {
            patternString = String.format("(^.*)(\\.)(%s)([\\d:]*$)", endPoint);
            pattern = Pattern.compile(patternString);
            matcher = pattern.matcher(host);
            if (matcher.matches()) {
                clientRequestToken.setVirtualHost(Boolean.TRUE);
                clientRequestToken.setBucketName(matcher.group(1));
                return;
            }
        }
        clientRequestToken.setVirtualHost(Boolean.FALSE);
    }
}
