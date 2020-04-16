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

import java.util.Map;

public class ClientRequestToken {
    public enum AWSSigningVersion {
        V4, V2;
    }

    /*
     * Access Key Id of the requestor.
     */
    String accessKeyId;

    /*
     * Credential scope is used in AWS V4 signing algorithm.
     * It is a string that includes date (just the date and not the date and time),
     * region, target service and a termination string ("aws_v4")
     */
    String credentialScope;

    /*
     * This is used in AWS V4 signing algorithm.
     * Date obtained from credential scope.
     */
    String date;

    /*
     * Http Method i.e GET, PUT, DELETE etc.
     */
    String httpMethod;

    /*
     * Unparsed Query String
     */
    String query;

    /*
     * Target region obtained from credential scope.
     */
    String region;

    /*
     * Service name obtained from credential scope.
     */
    String service;

    /*
     * Request signature.
     */
    String signature;

    /*
     * Signed headers used in AWS V4 signing algorithm.
     */
    String signedHeaders;

    /*
     * Algorithm used to sign the request.
     * Ex - HMAC-SHA256
     */
    String signingAlgorithm;

    /*
     * AWS Signing algorithm version.
     */
    AWSSigningVersion signVersion;

    /*
     * Convert httpheaders into a hash map.
     * The header attributes are required by signing algorithms.
     */
    Map<String, String> requestHeaders;

    /*
     * Sub resource is required for AWS signing version 2.
     */
    String subResource;

    /*
     * Unparsed URI received by the server.
     */
    String uri;

    /*
     * If request uses virtual host style, then set this attribute to true.
     */
    Boolean virtualHost;

    /**
     * HTTP Request payload.
     */
    String requestPayload;

    /*
     * Bucket name, currently used only for virtual hosted-style requests
     */
    String bucketName;

    /*
     * Return the Access Key Id of the requestor.
     */
    public String getAccessKeyId() {
        return accessKeyId;
    }

    /*
     * Return the credential scope
     */
    public String getCredentialScope() {
        return credentialScope;
    }

    /*
     * Return the date.
     */
    public String getDate() {
        return date;
    }

    /*
     * Return the HTTP method.
     */
    public String getHttpMethod() {
        return httpMethod;
    }

    /*
     * Return the query string.
     */
    public String getQuery() {
        return query;
    }

    /*
     * Return the target region.
     */
    public String getRegion() {
        return region;
    }

    /*
     * Set the request headers.
     */
    public Map<String, String> getRequestHeaders() {
        return requestHeaders;
    }

    /*
     * Return the requested service name.
     */
    public String getService() {
        return service;
    }

    /*
     * Return the request signature.
     */
    public String getSignature() {
        return signature;
    }

    /*
     * Return the signed header array.
     */
    public String getSignedHeaders() {
        return signedHeaders;
    }

    /*
     * Return the signing algorithm.
     */
    public String getSigningAlgorithm() {
        return signingAlgorithm;
    }

    /*
     * Return the signature version
     */
    public AWSSigningVersion getSignVersion() {
        return signVersion;
    }

    /*
     * Return sub resource.
     */
    public String getSubResource() {
        return subResource;
    }

    /*
     * Return the uri.
     */
    public String getUri() {
        return uri;
    }

    /**
     *Getter for request payload.
     *
     * @return HTTP request payload
     */
    public String getRequestPayload() {
        return requestPayload;
    }

    /*
     * Return true if the request uses virtual host name.
     */
    public Boolean isVirtualHost() {
        return virtualHost;
    }

    public String getBucketName() {
        return bucketName;
    }

    /*
     * Set the access key id.
     */
    public void setAccessKeyId(String accessKeyId) {
        this.accessKeyId = accessKeyId;
    }

    /*
     * Set the credential scope.
     */
    public void setCredentialScope(String credentialScope) {
        this.credentialScope = credentialScope;
    }

    /*
     * Set the Date.
     */
    public void setDate(String date) {
        this.date = date;
    }

    /*
     * Set the http method.
     */
    public void setHttpMethod(String httpMethod) {
        this.httpMethod = httpMethod;
    }

    /*
     * Set the query string
     */
    public void setQuery(String query) {
        this.query = query;
    }

    /*
     * Set the region.
     */
    public void setRegion(String region) {
        this.region = region;
    }

    public void setRequestHeaders(Map<String, String> requestHeaders) {
        this.requestHeaders = requestHeaders;
    }

    /*
     * Set the service name.
     */
    public void setService(String service) {
        this.service = service;
    }

    /*
     * Set the signature
     */
    public void setSignature(String signature) {
        this.signature = signature;
    }

    /*
     * Set the signed headers.
     */
    public void setSignedHeaders(String signedHeaders) {
        this.signedHeaders = signedHeaders;
    }

    /*
     * Set the siginig algorithm.
     */
    public void setSigningAlgorithm(String algorithm) {
        this.signingAlgorithm = algorithm;
    }

    /*
     * Set the signed version.
     */
    public void setSignedVersion(AWSSigningVersion signVersion) {
        this.signVersion = signVersion;
    }

    /*
     * set the sub resource.
     */
    public void setSubResource(String subResource) {
        this.subResource = subResource;
    }

    /*
     * Set the uri.
     */
    public void setUri(String uri) {
        this.uri = uri;
    }

    /*
     * set true if the request uses virtual host name.
     */
    public void setVirtualHost(Boolean virtualHost) {
        this.virtualHost = virtualHost;
    }

    /**
     * Setter for request payload.
     *
     * @param requestPayload
     */
    public void setRequestPayload(String requestPayload) {
        this.requestPayload = requestPayload;
    }

    public void setBucketName(String bucketName) {
        this.bucketName = bucketName;
    }
}
