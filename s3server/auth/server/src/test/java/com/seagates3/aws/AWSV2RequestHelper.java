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
 * Original creation date: 22-Mar-2016
 */
package com.seagates3.aws;

import com.seagates3.model.AccessKey;
import com.seagates3.authentication.ClientRequestToken;
import com.seagates3.model.Requestor;
import io.netty.handler.codec.http.DefaultHttpHeaders;
import io.netty.handler.codec.http.HttpHeaders;
import java.util.Map;
import java.util.TreeMap;

public class AWSV2RequestHelper {

    public final static String ACCESS_KEY_ID = "AKIAJTYX36YCKQSAJT7Q";
    public final static String SECRET_KEY
            = "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";

    /**
     * Create request headers for a regular request.
     *
     * @return
     */
    public static Map<String, String> getRequestHeadersPathStyle() {
        Map<String, String> requestBody
                = new TreeMap<>(String.CASE_INSENSITIVE_ORDER);

        requestBody.put("Accept-Encoding", "identity");
        requestBody.put("Action", "AuthenticateUser");
        requestBody.put("Authorization", "AWS AKIAJTYX36YCKQSAJT7Q:uDWiVvxwCUR"
                + "9YJ8EGJgbtW9tjFM=");
        requestBody.put("ClientAbsoluteUri", "/seagatebucket/test.txt");
        requestBody.put("ClientQueryParams", "");
        requestBody.put("Content-Length", "8");
        requestBody.put("content-type", "text/plain");
        requestBody.put("Host", "s3.seagate.com");
        requestBody.put("Method", "PUT");
        requestBody.put("Version", "2010-05-08");
        requestBody.put("x-amz-meta-s3cmd-attrs", "uid:0/gname:root/uname:root/"
                + "gid:0/mode:33188/mtime:1458639989/atime:1458640002/md5:eb1a"
                + "3227cdc3fedbaec2fe38bf6c044a/ctime:1458639989");
        requestBody.put("x-amz-date", "Tue, 22 Mar 2016 09:46:54 +0000");
        requestBody.put("x-amz-storage-class", "STANDARD");

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

        requestBody.put("Accept-Encoding", "identity");
        requestBody.put("Action", "AuthenticateUser");
        requestBody.put("Authorization", "AWS AKIAJTYX36YCKQSAJT7Q:4dtRFT7O4a7nVZ"
                + "ieelIicVLuGoE=");
        requestBody.put("ClientAbsoluteUri", "/test.txt");
        requestBody.put("ClientQueryParams", "");
        requestBody.put("Content-Length", "8");
        requestBody.put("content-type", "text/plain");
        requestBody.put("Host", "seagatebucket.s3.seagate.com");
        requestBody.put("Method", "PUT");
        requestBody.put("Version", "2010-05-08");
        requestBody.put("x-amz-meta-s3cmd-attrs", "uid:0/gname:root/uname:root/"
                + "gid:0/mode:33188/mtime:1458639989/atime:1458640002/md5:eb1a"
                + "3227cdc3fedbaec2fe38bf6c044a/ctime:1458639989");
        requestBody.put("x-amz-date", "Tue, 22 Mar 2016 10:01:02 +0000");
        requestBody.put("x-amz-storage-class", "STANDARD");

        return requestBody;
    }

    /**
     * Create clientRequestToken for the chunk request of a chunked upload.
     *
     * @return
     */
    public static ClientRequestToken getRequestClientTokenPathStyle() {
        ClientRequestToken requestToken = new ClientRequestToken();
        requestToken.setSignedVersion(ClientRequestToken.AWSSigningVersion.V2);
        requestToken.setAccessKeyId(ACCESS_KEY_ID);
        requestToken.setHttpMethod("PUT");
        requestToken.setUri("/seagatebucket/test.txt");
        requestToken.setQuery("");
        requestToken.setSignature("uDWiVvxwCUR9YJ8EGJgbtW9tjFM=");
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
        ClientRequestToken requestToken = new ClientRequestToken();
        requestToken.setSignedVersion(ClientRequestToken.AWSSigningVersion.V2);
        requestToken.setAccessKeyId(ACCESS_KEY_ID);
        requestToken.setHttpMethod("PUT");
        requestToken.setUri("/test.txt");
        requestToken.setQuery("");
        requestToken.setSignature("4dtRFT7O4a7nVZieelIicVLuGoE=");
        requestToken.setVirtualHost(Boolean.TRUE);
        requestToken.setRequestHeaders(getRequestHeadersVirtualHostStyle());
        requestToken.setBucketName("seagatebucket");

        return requestToken;
    }

    /**
     * Return HTTP headers to test parser for FullHttpRequest.
     *
     * @return
     */
    public static HttpHeaders getHttpHeaders() {
        HttpHeaders httpHeaders = new DefaultHttpHeaders();

        httpHeaders.add("Authorization", "AWS AKIAJTYX36YCKQSAJT7Q:4dtRFT7O4a7n"
                + "VZieelIicVLuGoE=");
        httpHeaders.add("Content-Length", "8");
        httpHeaders.add("Host", "seagatebucket123.s3.seagate.com");
        httpHeaders.add("Version", "2010-05-08");
        httpHeaders.add("x-amz-date", "Tue, 22 Mar 2016 10:01:02 +0000");
        httpHeaders.add("x-amz-meta-s3cmd-attrs", "uid:0/gname:root/uname:root/"
                + "gid:0/mode:33188/mtime:1458639989/atime:1458640002/md5:eb1a"
                + "3227cdc3fedbaec2fe38bf6c044a/ctime:1458639989");
        httpHeaders.add("x-amz-storage-class", "STANDARD");

        return httpHeaders;
    }

    /**
     * Return ClientRequestToken for the httpHeaders.
     *
     * @return
     */
    public static ClientRequestToken getFullHttpRequestClientToken() {
        Map<String, String> requestBody
                = new TreeMap<>(String.CASE_INSENSITIVE_ORDER);
        requestBody.put("Authorization", "AWS AKIAJTYX36YCKQSAJT7Q:4dtRFT7O4a7n"
                + "VZieelIicVLuGoE=");
        requestBody.put("Content-Length", "8");
        requestBody.put("Host", "seagatebucket123.s3.seagate.com");
        requestBody.put("Version", "2010-05-08");
        requestBody.put("x-amz-meta-s3cmd-attrs", "uid:0/gname:root/uname:root/"
                + "gid:0/mode:33188/mtime:1458639989/atime:1458640002/md5:eb1a"
                + "3227cdc3fedbaec2fe38bf6c044a/ctime:1458639989");
        requestBody.put("x-amz-date", "Tue, 22 Mar 2016 10:01:02 +0000");
        requestBody.put("x-amz-storage-class", "STANDARD");

        ClientRequestToken requestToken = new ClientRequestToken();
        requestToken.setSignedVersion(ClientRequestToken.AWSSigningVersion.V2);
        requestToken.setAccessKeyId(ACCESS_KEY_ID);
        requestToken.setHttpMethod("PUT");
        requestToken.setUri("/");
        requestToken.setQuery("");
        requestToken.setSignature("4dtRFT7O4a7nVZieelIicVLuGoE=");
        requestToken.setRequestPayload("");
        requestToken.setVirtualHost(Boolean.TRUE);
        requestToken.setRequestHeaders(requestBody);
        requestToken.setBucketName("seagatebucket123");

        return requestToken;
    }

    public static ClientRequestToken getClientRequestTokenSubResourceStringTest() {
        Map<String, String> requestBody
                = new TreeMap<>(String.CASE_INSENSITIVE_ORDER);
        requestBody.put("Accept", "text/html, image/gif, image/jpeg, *; q=.2, */*; q=.2");
        requestBody.put("Action", "AuthenticateUser");
        requestBody.put("Authorization", "AWS AKIAJTYX36YCKQSAJT7Q:g9ZXcuNXpaDDlP+j7B3vLfGE6Ow=");
        requestBody.put("ClientAbsoluteUri", "/seagatebucket/1mbfile");
        requestBody.put("ClientQueryParams", "partNumber=1&uploadId=5e9bac46-8994-4bc9-87fd-3486af85314c");
        requestBody.put("Content-Length", "1048576");
        requestBody.put("Content-Type", "application/unknown");
        requestBody.put("Date", "Mon, 04 Apr 2016 06:25:22 GMT");
        requestBody.put("Host", "s3.seagate.com");
        requestBody.put("Mehod", "PUT");
        requestBody.put("User-Agent", "jclouds/1.9.2 java/1.8.0_60");
        requestBody.put("Version", "2010-05-08");

        ClientRequestToken requestToken = new ClientRequestToken();
        requestToken.setAccessKeyId("AKIAJTYX36YCKQSAJT7Q");
        requestToken.setHttpMethod("PUT");
        requestToken.setQuery("partNumber=1&uploadId=5e9bac46-8994-4bc9-87fd-3486af85314c");
        requestToken.setSignature("g9ZXcuNXpaDDlP+j7B3vLfGE6Ow=");
        requestToken.setSignedVersion(ClientRequestToken.AWSSigningVersion.V2);
        requestToken.setAccessKeyId(ACCESS_KEY_ID);
        requestToken.setUri("/seagatebucket/1mbfile");
        requestToken.setVirtualHost(Boolean.FALSE);
        requestToken.setRequestHeaders(requestBody);

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
}
