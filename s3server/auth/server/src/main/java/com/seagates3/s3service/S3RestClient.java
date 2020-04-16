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
import java.io.UnsupportedEncodingException;

import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;


import org.apache.http.client.methods.HttpRequestBase;

import org.apache.http.HttpResponse;
import org.apache.http.client.ClientProtocolException;
import org.apache.http.client.HttpClient;
import org.apache.http.client.methods.HttpDelete;

import org.apache.http.client.methods.HttpPost;

import org.apache.http.impl.client.HttpClientBuilder;

import com.seagates3.exception.S3RequestInitializationException;
import com.seagates3.util.AWSSignUtil;
import com.seagates3.util.BinaryUtil;
import com.seagates3.util.DateUtil;
import com.seagates3.util.IEMUtil;

import io.netty.handler.codec.http.HttpMethod;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class S3RestClient {

    private final Logger LOGGER =
            LoggerFactory.getLogger(S3AccountNotifier.class.getName());

    public String method = null;
    public Map<String, String> headers = new HashMap<String, String>();
    public String payload = null;
    public String accessKey = null;
    public String secretKey = null;
    public String URL = null;
    public String resource = null;

    public void setHeader(String header, String headerValue) {
        headers.put(header, headerValue);
    }

    public void setMethod(String reqType) {
        method = reqType;
    }

    public void setCredentials(String aKey, String sKey) {
        accessKey = aKey;
        secretKey = sKey;
    }

    public void setURL(String url) {
        URL = url;
    }

    public void setResource(String res) {
        resource = res;
    }

    /**
     * Validate values of all mandatory S3 Args
     * @return boolean
     */
    private boolean validateRequiredParams() {
        if (method == null) {
            LOGGER.error("Rest request method type missing");
            return false;
        }
        if (resource == null) {
            LOGGER.error("Rest request resource value missing");
            return false;
        }
        if (URL == null) {
            LOGGER.error("Rest request URL value missing");
            return false;
        }
        if (accessKey == null || accessKey.isEmpty() || secretKey == null ||
                                                        secretKey.isEmpty()) {
            LOGGER.error("Missing/invalid accessKey or secretKey values");
            return false;
        }
        return true;
    }

    /**
     * Initializes REST request
     * @param HttpRequestBase
     * @return boolean
     */
    private boolean prepareRequest(HttpRequestBase req) {
        final String d = DateUtil.getCurrentTimeGMT();
        final String payload;
        LOGGER.debug("headers-- " + headers);
        if (headers.containsKey("x-amz-security-token")) {
          payload = method + "\n\n\n" + d + "\n" + "x-amz-security-token:" +
                    headers.get("x-amz-security-token") + "\n" + resource;
        } else {
          payload = method + "\n\n\n" + d + "\n" + resource;
        }
        String signature = AWSSignUtil.calculateSignatureAWSV2(payload, secretKey);

        if (signature == null) {
            LOGGER.error("Failed to calculate signature for payload:"
                                                           + payload);
            return false;
        }

        LOGGER.debug("Payload:" + payload + " Signature:" + signature);

        req.setHeader("Authorization", "AWS " + accessKey + ":" + signature);
        req.setHeader("Date", d);
        Iterator<Map.Entry<String, String>> itr = headers.entrySet().iterator();

        while (itr.hasNext())
        {
             Map.Entry<String, String> entry = itr.next();
             req.setHeader(entry.getKey(), entry.getValue());
        }
        return true;
    }

    /**
     * Method to prepare and execute S3 Rest Request
     * @throws S3RequestInitializationException
     * @throws IOException
     * @throws ClientProtocolException
     */
    private S3HttpResponse execute(HttpRequestBase req) throws
     S3RequestInitializationException, ClientProtocolException, IOException {

        LOGGER.debug("Executing " + method + "request for resource:"
                                                        + resource);

        if (!prepareRequest(req)) {
           LOGGER.error("Failed to prepare S3 REST Post request");
           throw new S3RequestInitializationException("Failed to initialize "
                                                        + "S3 Post Request");
        }

        HttpClient httpClient =  HttpClientBuilder.create().build();
        S3HttpResponse s3Resp = new S3HttpResponse();
        try {
            HttpResponse resp = httpClient.execute(req);
            s3Resp.setHttpCode(resp.getStatusLine().getStatusCode());
            s3Resp.setHttpStatusMessage(
                    resp.getStatusLine().getReasonPhrase());
        } finally {
            req.releaseConnection();
        }

        return s3Resp;

    }

    /**
     * Method sends a POST request to S3 Server
     * @return HttpRespose
     * @throws S3RequestInitializationException
     * @throws ClientProtocolException
     * @throws IOException
     */
    public S3HttpResponse postRequest() throws S3RequestInitializationException,
                                        ClientProtocolException, IOException {
        method = HttpMethod.POST.toString();

        if (!validateRequiredParams()) {
            LOGGER.error("Invalid REST request parameter values found");
            throw new S3RequestInitializationException("Failed to initialize "
                    + "S3 Post Request");
        }
        HttpPost req = new HttpPost(URL);

        return execute(req);
    }

    /**
     * Method sends DELETE request to S3 Server
     * @return HttpResponse
     * @throws S3RequestInitializationException
     * @throws ClientProtocolException
     * @throws IOException
     */
    public S3HttpResponse deleteRequest()
            throws S3RequestInitializationException,
              ClientProtocolException, IOException {
        method = HttpMethod.DELETE.toString();

        if (!validateRequiredParams()) {
            LOGGER.error("Invalid REST request parameter values found");
            throw new S3RequestInitializationException("Failed to initialize "
                    + "S3 Delete Request");
        }
        HttpDelete req = new HttpDelete(URL);

        return execute(req);
    }

}

