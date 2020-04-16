/*
 * COPYRIGHT 2016 SEAGATE LLC
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
 * Original creation date: 17-Mar-2016
 */
package com.seagates3.aws;

import com.seagates3.model.AccessKey;
import com.seagates3.authentication.ClientRequestToken;
import com.seagates3.model.Requestor;
import io.netty.handler.codec.http.DefaultHttpHeaders;
import io.netty.handler.codec.http.HttpHeaders;
import java.util.Map;
import java.util.TreeMap;

public class AWSV4RequestHelper {

    public final static String ACCESS_KEY_ID = "AKIAIOSFODNN7EXAMPLE";
    public final static String SECRET_KEY
            = "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY";

    /**
     * Create the request headers for the seed request of a chunked upload.
     *
     * @return
     */
    public static Map<String, String> getChunkedSeedRequestHeaders() {
        Map<String, String> requestBody
                = new TreeMap<>(String.CASE_INSENSITIVE_ORDER);

        String authHeader = "AWS4-HMAC-SHA256 "
                + "Credential=AKIAIOSFODNN7EXAMPLE/20130524/us-east-1/s3/"
                + "aws4_request,SignedHeaders=content-encoding;content-length;"
                + "host;x-amz-content-sha256;x-amz-date;"
                + "x-amz-decoded-content-length;x-amz-storage-class,"
                + "Signature=4f232c4386841ef735655705268965c44a0e4690baa4adea15"
                + "3f7db9fa80a0a9";

        requestBody.put("Host", "s3.amazonaws.com");
        requestBody.put("x-amz-date", "20130524T000000Z");
        requestBody.put("x-amz-storage-class", "REDUCED_REDUNDANCY");
        requestBody.put("Authorization", authHeader);
        requestBody.put("x-amz-content-sha256", "STREAMING-AWS4-HMAC-SHA256-"
                + "PAYLOAD");
        requestBody.put("Content-Encoding", "aws-chunked");
        requestBody.put("x-amz-decoded-content-length", "66560");
        requestBody.put("Content-Length", "66824");
        requestBody.put("Action", "AuthenticateUser");
        requestBody.put("ClientAbsoluteUri", "/examplebucket/chunkObject.txt");
        requestBody.put("ClientQueryParams", "");
        requestBody.put("Method", "PUT");

        return requestBody;
    }

    /**
     * Create the request headers for a chunk in a chunked upload.
     *
     * @return
     */
    public static Map<String, String> getChunkedRequestHeaders() {
        Map<String, String> requestBody
                = new TreeMap<>(String.CASE_INSENSITIVE_ORDER);

        String authHeader = "AWS4-HMAC-SHA256 "
                + "Credential=AKIAIOSFODNN7EXAMPLE/20130524/us-east-1/s3/"
                + "aws4_request,SignedHeaders=content-encoding;content-length;"
                + "host;x-amz-content-sha256;x-amz-date;"
                + "x-amz-decoded-content-length;x-amz-storage-class,"
                + "Signature=4f232c4386841ef735655705268965c44a0e4690baa4adea15"
                + "3f7db9fa80a0a9";

        requestBody.put("Host", "s3.amazonaws.com");
        requestBody.put("x-amz-date", "20130524T000000Z");
        requestBody.put("x-amz-storage-class", "REDUCED_REDUNDANCY");
        requestBody.put("Authorization", authHeader);
        requestBody.put("x-amz-content-sha256", "bf718b6f653bebc184e1479f1935b8"
                + "da974d701b893afcf49e701f3e2f9f9c5a");
        requestBody.put("Content-Encoding", "aws-chunked");
        requestBody.put("x-amz-decoded-content-length", "66560");
        requestBody.put("Content-Length", "66824");
        requestBody.put("Action", "AuthenticateUser");
        requestBody.put("ClientAbsoluteUri", "/examplebucket/chunkObject.txt");
        requestBody.put("ClientQueryParams", "");
        requestBody.put("Method", "PUT");
        requestBody.put("previous-signature-sha256",
                "4f232c4386841ef735655705268965c44a0e4690baa4adea153f7db9fa80a0a9");
        requestBody.put("current-signature-sha256",
                "ad80c730a21e5b8d04586a2213dd63b9a0e99e0e2307b0ade35a65485a288648");

        return requestBody;
    }

    /**
     * Create request headers for a regular request.
     *
     * @return
     */
    public static Map<String, String> getRequestHeadersPathStyle() {
        Map<String, String> requestBody
                = new TreeMap<>(String.CASE_INSENSITIVE_ORDER);

        String authHeader = "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/"
                + "20160321/seagate/s3/aws4_request, SignedHeaders="
                + "content-length;content-md5;content-type;host;user-agent;"
                + "x-amz-content-sha256;x-amz-date, Signature=3ff2116bde7cbb3c9"
                + "6a3d858e9c7ee1ef9482593fc9dd49bf008010bc9cadd4d";

        requestBody.put("Action", "AuthenticateUser");
        requestBody.put("Authorization", authHeader);
        requestBody.put("ClientAbsoluteUri", "/seagate_bucket/");
        requestBody.put("ClientQueryParams", "delete");
        requestBody.put("Content-Length", "134");
        requestBody.put("Content-MD5", "lfCB9/TUZuH72txvI16+Og==");
        requestBody.put("Content-Type", "application/xml");
        requestBody.put("Host", "s3.seagate.com");
        requestBody.put("Method", "POST");
        requestBody.put("User-Agent", "aws-sdk-java/1.10.20 Linux/"
                + "3.10.0-229.14.1.el7.x86_64 Java_HotSpot(TM)_"
                + "64-Bit_Server_VM/25.60-b23/1.8.0_60");
        requestBody.put("Version", "2010-05-08");
        requestBody.put("x-amz-date", "20160321T071431Z");
        requestBody.put("x-amz-content-sha256", "aea0a6f73370d69fac23f09a196024"
                + "497b268994e031242c5af7d57f8e6755c6");

        return requestBody;
    }

    /**
     * Create request headers for a regular request.
     *
     * Use Request header to test virtual host style.
     *
     * @return
     */
    public static Map<String, String> getRequestHeadersVirtualHostStyle() {
        Map<String, String> requestBody
                = new TreeMap<>(String.CASE_INSENSITIVE_ORDER);

        String authHeader = "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/"
                + "20160321/US/s3/aws4_request,SignedHeaders=host;"
                + "x-amz-content-sha256;x-amz-date,Signature=b0024ea6f4b7ea025"
                + "c890ef0c18fb576ee889feaf54b00e7b06ca11401737a2c";

        requestBody.put("Action", "AuthenticateUser");
        requestBody.put("Authorization", authHeader);
        requestBody.put("ClientAbsoluteUri", "/");
        requestBody.put("ClientQueryParams", "delimiter=/");
        requestBody.put("Content-Length", "0");
        requestBody.put("Host", "seagatebucket123.s3.seagate.com");
        requestBody.put("Method", "GET");
        requestBody.put("Version", "2010-05-08");
        requestBody.put("x-amz-date", "20160321T104409Z");
        requestBody.put("x-amz-content-sha256", "e3b0c44298fc1c149afbf4c8996fb9"
                + "2427ae41e4649b934ca495991b7852b855");

        return requestBody;
    }

    /**
     * Create clientRequestToken for the seed request of a chunked upload.
     *
     * @return
     */
    public static ClientRequestToken getChunkedSeedRequestClientToken() {
        String signedHeaders = "content-encoding;content-length;"
                + "host;x-amz-content-sha256;x-amz-date;"
                + "x-amz-decoded-content-length;x-amz-storage-class";

        ClientRequestToken requestToken = new ClientRequestToken();
        requestToken.setAccessKeyId(ACCESS_KEY_ID);
        requestToken.setHttpMethod("PUT");
        requestToken.setSignedVersion(ClientRequestToken.AWSSigningVersion.V4);
        requestToken.setRequestPayload("");
        requestToken.setVirtualHost(Boolean.FALSE);
        requestToken.setUri("/examplebucket/chunkObject.txt");
        requestToken.setQuery("");
        requestToken.setRequestHeaders(getChunkedSeedRequestHeaders());
        requestToken.setSignedHeaders(signedHeaders);
        requestToken.setSigningAlgorithm("AWS4-HMAC-SHA256");
        requestToken.setCredentialScope("20130524/us-east-1/s3/aws4_request");
        requestToken.setDate("20130524");
        requestToken.setRegion("us-east-1");
        requestToken.setService("s3");
        requestToken.setSignature("4f232c4386841ef735655705268965c44a0e4690baa4adea153f7db9fa80a0a9");

        return requestToken;
    }

    /**
     * Create clientRequestToken for the chunk request of a chunked upload.
     *
     * @return
     */
    public static ClientRequestToken getChunkedRequestClientToken() {
        String signedHeaders = "content-encoding;content-length;"
                + "host;x-amz-content-sha256;x-amz-date;"
                + "x-amz-decoded-content-length;x-amz-storage-class";

        ClientRequestToken requestToken = new ClientRequestToken();
        requestToken.setAccessKeyId(ACCESS_KEY_ID);
        requestToken.setHttpMethod("PUT");
        requestToken.setSignedVersion(ClientRequestToken.AWSSigningVersion.V4);
        requestToken.setRequestPayload("");
        requestToken.setVirtualHost(Boolean.FALSE);
        requestToken.setUri("/examplebucket/chunkObject.txt");
        requestToken.setQuery("");
        requestToken.setRequestHeaders(getChunkedRequestHeaders());
        requestToken.setSignedHeaders(signedHeaders);
        requestToken.setSigningAlgorithm("AWS4-HMAC-SHA256");
        requestToken.setCredentialScope("20130524/us-east-1/s3/aws4_request");
        requestToken.setDate("20130524");
        requestToken.setRegion("us-east-1");
        requestToken.setService("s3");
        requestToken.setSignature(
                "ad80c730a21e5b8d04586a2213dd63b9a0e99e0e2307b0ade35a65485a288648");

        return requestToken;
    }

    /**
     * Create clientRequestToken for the chunk request of a chunked upload.
     *
     * @return
     */
    public static ClientRequestToken getRequestClientTokenPathStyle() {
        String signedHeaders = "content-length;content-md5;content-type;host;"
                + "user-agent;x-amz-content-sha256;x-amz-date";

        ClientRequestToken requestToken = new ClientRequestToken();
        requestToken.setSignedVersion(ClientRequestToken.AWSSigningVersion.V4);
        requestToken.setAccessKeyId(ACCESS_KEY_ID);
        requestToken.setCredentialScope("20160321/seagate/s3/aws4_request");
        requestToken.setDate("20160321");
        requestToken.setHttpMethod("POST");
        requestToken.setUri("/seagate_bucket/");
        requestToken.setQuery("delete");
        requestToken.setRegion("seagate");
        requestToken.setService("s3");
        requestToken.setSignature(
                "3ff2116bde7cbb3c96a3d858e9c7ee1ef9482593fc9dd49bf008010bc9cadd4d");
        requestToken.setSignedHeaders(signedHeaders);
        requestToken.setSigningAlgorithm("AWS4-HMAC-SHA256");
        requestToken.setRequestPayload("");
        requestToken.setVirtualHost(Boolean.FALSE);
        requestToken.setRequestHeaders(getRequestHeadersPathStyle());

        return requestToken;
    }

    /**
     * Create clientRequestToken for the chunk request of a chunked upload.
     *
     * @return
     */
    public static ClientRequestToken getRequestClientTokenVirtualHostStyle() {
        String signedHeaders = "host;x-amz-content-sha256;x-amz-date";

        ClientRequestToken requestToken = new ClientRequestToken();
        requestToken.setSignedVersion(ClientRequestToken.AWSSigningVersion.V4);
        requestToken.setAccessKeyId(ACCESS_KEY_ID);
        requestToken.setCredentialScope("20160321/US/s3/aws4_request");
        requestToken.setDate("20160321");
        requestToken.setHttpMethod("GET");
        requestToken.setUri("/");
        requestToken.setQuery("delimiter=/");
        requestToken.setRegion("US");
        requestToken.setService("s3");
        requestToken.setSignature(
                "b0024ea6f4b7ea025c890ef0c18fb576ee889feaf54b00e7b06ca11401737a2c");
        requestToken.setSignedHeaders(signedHeaders);
        requestToken.setSigningAlgorithm("AWS4-HMAC-SHA256");
        requestToken.setRequestPayload("");
        requestToken.setVirtualHost(Boolean.TRUE);
        requestToken.setRequestHeaders(getRequestHeadersVirtualHostStyle());
        requestToken.setBucketName("seagatebucket123");

        return requestToken;
    }

    /**
     * Create clientRequestToken for special query parameters.
     *
     * @return
     */
    public static ClientRequestToken getRequestClientTokenSpecialQuery() {
        String signedHeaders = "host;x-amz-content-sha256;x-amz-date";

        ClientRequestToken requestToken = new ClientRequestToken();
        requestToken.setSignedVersion(ClientRequestToken.AWSSigningVersion.V4);
        requestToken.setAccessKeyId(ACCESS_KEY_ID);
        requestToken.setCredentialScope("20160321/US/s3/aws4_request");
        requestToken.setDate("20160321");
        requestToken.setHttpMethod("GET");
        requestToken.setUri("/seagate&bucket/");
        requestToken.setQuery("delimiter=/");
        requestToken.setRegion("US");
        requestToken.setService("s3");
        requestToken.setSignature(
                "a64de7ff4d9f03dd71bcb2957e919258dfbe4fabd9860856636bfbe3fa63ddbc");
        requestToken.setSignedHeaders(signedHeaders);
        requestToken.setSigningAlgorithm("AWS4-HMAC-SHA256");
        requestToken.setRequestPayload("");
        requestToken.setVirtualHost(Boolean.TRUE);
        requestToken.setRequestHeaders(getRequestHeadersVirtualHostStyle());
        requestToken.setBucketName("seag&ate bu&cket");

        return requestToken;
    }

    /**
     * Return HTTP headers to test parser for FullHttpRequest.
     *
     * @return
     */
    public static HttpHeaders getHttpHeaders() {
        HttpHeaders httpHeaders = new DefaultHttpHeaders();

        String authHeader = "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/"
                + "20160321/US/s3/aws4_request,SignedHeaders=host;"
                + "x-amz-content-sha256;x-amz-date,Signature=b9d04cd83010685a1"
                + "085ece38657125c02a6f29093f5bd21dcd6e717f1996852";

        httpHeaders.add("Authorization", authHeader);
        httpHeaders.add("Content-Length", "0");
        httpHeaders.add("Host", "seagatebucket123.s3.seagate.com");
        httpHeaders.add("Version", "2010-05-08");
        httpHeaders.add("x-amz-date", "20160321T104409Z");
        httpHeaders.add("x-amz-content-sha256", "testsha");

        return httpHeaders;
    }

    /**
     * Return ClientRequestToken for the httpHeaders.
     *
     * @return
     */
    public static ClientRequestToken getFullHttpRequestClientTokenEmptyQuery() {
        String authHeader = "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/"
                + "20160321/US/s3/aws4_request,SignedHeaders=host;"
                + "x-amz-content-sha256;x-amz-date,Signature=b9d04cd83010685a1"
                + "085ece38657125c02a6f29093f5bd21dcd6e717f1996852";

        Map<String, String> requestBody
                = new TreeMap<>(String.CASE_INSENSITIVE_ORDER);
        requestBody.put("Authorization", authHeader);
        requestBody.put("Content-Length", "0");
        requestBody.put("Host", "seagatebucket123.s3.seagate.com");
        requestBody.put("Version", "2010-05-08");
        requestBody.put("x-amz-date", "20160321T104409Z");
        requestBody.put("x-amz-content-sha256", "testsha");

        String signedHeaders = "host;x-amz-content-sha256;x-amz-date";

        ClientRequestToken requestToken = new ClientRequestToken();
        requestToken.setSignedVersion(ClientRequestToken.AWSSigningVersion.V4);
        requestToken.setAccessKeyId(ACCESS_KEY_ID);
        requestToken.setCredentialScope("20160321/US/s3/aws4_request");
        requestToken.setDate("20160321");
        requestToken.setHttpMethod("GET");
        requestToken.setUri("/");
        requestToken.setQuery("");
        requestToken.setRegion("US");
        requestToken.setService("s3");
        requestToken.setSignature(
                "b9d04cd83010685a1085ece38657125c02a6f29093f5bd21dcd6e717f1996852");
        requestToken.setSignedHeaders(signedHeaders);
        requestToken.setSigningAlgorithm("AWS4-HMAC-SHA256");
        requestToken.setRequestPayload("");
        requestToken.setVirtualHost(Boolean.TRUE);
        requestToken.setRequestHeaders(requestBody);
        requestToken.setBucketName("seagatebucket123");

        return requestToken;
    }

    /**
     * Return ClientRequestToken for the httpHeaders.
     *
     * @return
     */
    public static ClientRequestToken getFullHttpRequestClientTokenWithQuery() {
        String authHeader = "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/"
                + "20160321/US/s3/aws4_request,SignedHeaders=host;"
                + "x-amz-content-sha256;x-amz-date,Signature=b9d04cd83010685a1"
                + "085ece38657125c02a6f29093f5bd21dcd6e717f1996852";

        Map<String, String> requestBody
                = new TreeMap<>(String.CASE_INSENSITIVE_ORDER);
        requestBody.put("Authorization", authHeader);
        requestBody.put("Content-Length", "0");
        requestBody.put("Host", "seagatebucket123.s3.seagate.com");
        requestBody.put("Version", "2010-05-08");
        requestBody.put("x-amz-date", "20160321T104409Z");
        requestBody.put("x-amz-content-sha256", "testsha");

        String signedHeaders = "host;x-amz-content-sha256;x-amz-date";

        ClientRequestToken requestToken = new ClientRequestToken();
        requestToken.setSignedVersion(ClientRequestToken.AWSSigningVersion.V4);
        requestToken.setAccessKeyId(ACCESS_KEY_ID);
        requestToken.setCredentialScope("20160321/US/s3/aws4_request");
        requestToken.setDate("20160321");
        requestToken.setHttpMethod("GET");
        requestToken.setUri("/");
        requestToken.setQuery("delimiter=");
        requestToken.setRegion("US");
        requestToken.setService("s3");
        requestToken.setSignature(
                "b9d04cd83010685a1085ece38657125c02a6f29093f5bd21dcd6e717f1996852");
        requestToken.setSignedHeaders(signedHeaders);
        requestToken.setSigningAlgorithm("AWS4-HMAC-SHA256");
        requestToken.setRequestPayload("");
        requestToken.setVirtualHost(Boolean.TRUE);
        requestToken.setRequestHeaders(requestBody);
        requestToken.setBucketName("seagatebucket123");

        return requestToken;
    }

    /**
     * Return ClientRequestToken for the httpHeaders.
     *
     * @return
     */
    public static ClientRequestToken getFullHttpRequestClientTokenHEAD() {
        String authHeader = "AWS4-HMAC-SHA256 Credential=AKIAJTYX36YCKQSAJT7Q/"
                          + "20180719/us-east-1/s3/aws4_request,"
                          + "SignedHeaders=connection;date;host;"
                          + "x-amz-content-sha256;x-amz-date,"
                          + "Signature="
          + "aad057b69f74b68957f7d32c3c7c19b5a64d78749de4f5b328253629d9a55059";

        Map<String, String> requestBody
                = new TreeMap<>(String.CASE_INSENSITIVE_ORDER);
        requestBody.put("Authorization", authHeader);
        requestBody.put("connection", "Keep-Alive");
        requestBody.put("date", "Thu, 19 Jul 2018 07:28:25 GMT");
        requestBody.put("host", "seagatebucket.s3.seagate.com");
        requestBody.put("Version", "2010-05-08");
        requestBody.put("x-amz-date", "20180719T072825Z");
        requestBody.put("x-amz-content-sha256",
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

        String signedHeaders = "connection;date;host;x-amz-content-sha256;"
                             + "x-amz-date";

        ClientRequestToken requestToken = new ClientRequestToken();
        requestToken.setSignedVersion(ClientRequestToken.AWSSigningVersion.V4);
        requestToken.setAccessKeyId("AKIAJTYX36YCKQSAJT7Q");
        requestToken.setCredentialScope("20180719/us-east-1/s3/aws4_request");
        requestToken.setDate("20180719");
        requestToken.setHttpMethod("HEAD");
        requestToken.setUri("/Full-0026/0000");

        requestToken.setRegion("us-east-1");
        requestToken.setService("s3");
        requestToken.setSignature(
         "aad057b69f74b68957f7d32c3c7c19b5a64d78749de4f5b328253629d9a55059");
        requestToken.setSignedHeaders(signedHeaders);
        requestToken.setSigningAlgorithm("AWS4-HMAC-SHA256");
        requestToken.setRequestPayload("");
        requestToken.setVirtualHost(Boolean.TRUE);
        requestToken.setRequestHeaders(requestBody);
        requestToken.setBucketName("seagatebucket");

        return requestToken;
    }

    /**
     * Return ClientRequestToken for the httpHeaders.
     *
     * @return
     */
    public static ClientRequestToken getInvalidHttpRequestClientToken() {
        String authHeader = "AWS4-HMAC-SHA256 Credential="
                 + "AKIAJTYX36YCKQSAJT7Q/20180719/us-east-1/s3/aws4_request,"
         + "SignedHeaders=connection;date;"
         + "host;x-amz-content-sha256;x-amz-date,"
         + "Signature="
         + "aad057b69f74b68957f7d32c3c7c19b5a64d78749de4f5b328253629d9a55059";

        Map<String, String> requestBody
                = new TreeMap<>(String.CASE_INSENSITIVE_ORDER);
        requestBody.put("Authorization", authHeader);
  //    Not adding connection header
  //    requestBody.put("connection", "Keep-Alive");
        requestBody.put("date", "Thu, 19 Jul 2018 07:28:25 GMT");
        requestBody.put("host", "seagatebucket.s3.seagate.com");
        requestBody.put("Version", "2010-05-08");
        requestBody.put("x-amz-date", "20180719T072825Z");
        requestBody.put("x-amz-content-sha256",
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

        String signedHeaders = "connection;date;host;"
                             + "x-amz-content-sha256;x-amz-date";

        ClientRequestToken requestToken = new ClientRequestToken();
        requestToken.setSignedVersion(ClientRequestToken.AWSSigningVersion.V4);
        requestToken.setAccessKeyId("AKIAJTYX36YCKQSAJT7Q");
        requestToken.setCredentialScope("20180719/us-east-1/s3/aws4_request");
        requestToken.setDate("20180719");
        requestToken.setHttpMethod("HEAD");
        requestToken.setUri("/Full-0026/0000");

        requestToken.setRegion("us-east-1");
        requestToken.setService("s3");
        requestToken.setSignature(
          "aad057b69f74b68957f7d32c3c7c19b5a64d78749de4f5b328253629d9a55059");
        requestToken.setSignedHeaders(signedHeaders);
        requestToken.setSigningAlgorithm("AWS4-HMAC-SHA256");
        requestToken.setRequestPayload("");
        requestToken.setVirtualHost(Boolean.TRUE);
        requestToken.setRequestHeaders(requestBody);
        requestToken.setBucketName("seagatebucket");

        return requestToken;
    }

    /**
     * Return the requestor object.
     *
     * @return
     */
    public static Requestor getRequestor() {
        Requestor requestor = new Requestor();
        AccessKey accessKey = new AccessKey();
        accessKey.setId(ACCESS_KEY_ID);
        accessKey.setSecretKey(SECRET_KEY);

        requestor.setAccessKey(accessKey);
        return requestor;
    }
    /**
     * Return the requestor object.
     *
     * @return
     */
    public static Requestor getRequestorMock(String aKey, String sKey) {
        Requestor requestor = new Requestor();
        AccessKey accessKey = new AccessKey();
        accessKey.setId(aKey);
        accessKey.setSecretKey(sKey);

        requestor.setAccessKey(accessKey);
        return requestor;
    }

}
