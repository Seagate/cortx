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

import com.seagates3.model.Requestor;
import com.seagates3.util.BinaryUtil;
import java.io.UnsupportedEncodingException;
import java.util.Arrays;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

import com.seagates3.util.IEMUtil;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class AWSV2Sign implements AWSSign {

    private final Logger LOGGER = LoggerFactory.getLogger(
            AWSSign.class.getName());

    /**
     * Return true if the signature is valid.
     *
     * Reference -
     * http://docs.aws.amazon.com/AmazonS3/latest/dev/RESTAuthentication.html
     *
     * @param clientRequestToken
     * @param requestor
     * @return True if user is authenticated
     */
    @Override
    public Boolean authenticate(ClientRequestToken clientRequestToken,
            Requestor requestor) {
        String stringToSign, signature;

        stringToSign = createStringToSign(clientRequestToken);
        LOGGER.debug("String to sign - \n" + stringToSign);

        byte[] kStringToSign;
        try {
            kStringToSign = BinaryUtil.hmacSHA1(
                    requestor.getAccesskey().getSecretKey().getBytes(),
                    stringToSign.getBytes("UTF-8"));
        } catch (UnsupportedEncodingException ex) {
            IEMUtil.log(IEMUtil.Level.ERROR, IEMUtil.UTF8_UNAVAILABLE,
                    "UTF-8 encoding is not supported", null);
            return false;
        }

        signature = BinaryUtil.encodeToBase64String(kStringToSign);
        LOGGER.debug("Request signature- " + clientRequestToken.getSignature());
        LOGGER.debug("Calculated signature- " + signature);

        return signature.equals(clientRequestToken.getSignature());
    }

    /*
     * String to sign is in the following format.
     *
     * HTTPRequestMethod + '\n' +
     * Content-MD5 + "\n" +
     * Content-Type + "\n" +
     * Date + "\n" +
     * CanonicalizedAmzHeaders +
     * CanonicalizedResource;
     */
    private String createStringToSign(ClientRequestToken clientRequestToken) {
        String httpMethod, contentMD5, contentType, date;
        String canonicalizedAmzHeaders, canonicalizedResource, stringToSign;

        Map<String, String> requestHeaders = clientRequestToken.getRequestHeaders();

        httpMethod = clientRequestToken.getHttpMethod();
        if (requestHeaders.containsKey("Content-MD5")) {
            contentMD5 = requestHeaders.get("Content-MD5");
        } else {
            contentMD5 = "";
        }

        if (requestHeaders.containsKey("Content-Type")) {
            contentType = requestHeaders.get("Content-Type");
        } else {
            contentType = "";
        }

        if (requestHeaders.containsKey("Date")) {
            date = requestHeaders.get("Date");
        } else {
            date = "";
        }

        canonicalizedAmzHeaders = createCanonicalziedAmzHeaders(clientRequestToken);
        canonicalizedResource = createCanonicalizedResource(clientRequestToken);

        stringToSign = String.format("%s\n%s\n%s\n%s\n%s%s",
                httpMethod,
                contentMD5,
                contentType,
                date,
                canonicalizedAmzHeaders,
                canonicalizedResource
        );

        return stringToSign;
    }

    /*
     * 1. To construct the CanonicalizedAmzHeaders part of StringToSign,
     * select all HTTP request headers that start with 'x-amz-'
     *
     * 2. Convert each HTTP header name to lowercase.
     * For example, 'X-Amz-Date' becomes 'x-amz-date'.
     *
     * 3. Sort the collection of headers lexicographically by header name.
     *
     * 4. Combine header fields with the same name into one
     * "header-name:comma-separated-value-list".
     * Ex - the two metadata headers 'x-amz-meta-username: fred' and
     * 'x-amz-meta-username: barney' would be combined into the single header
     * 'x-amz-meta-username: fred,barney'.
     *
     * 5. Trim any whitespace around the colon in the header
     *
     * 6. Finally, append a newline character (U+000A) to each canonicalized
     * header in the resulting list.
     */
    private String createCanonicalziedAmzHeaders(ClientRequestToken clientRequestToken) {
        Map<String, String> requestHeaders = clientRequestToken.getRequestHeaders();
        Map<String, String> xAmzHeaders = new TreeMap<>();

        for (Map.Entry<String, String> entry : requestHeaders.entrySet()) {
            String key = entry.getKey().toLowerCase();

            if (key.startsWith("x-amz-")) {
                xAmzHeaders.put(key, entry.getValue());
            }
        }

        String canonicalizedAmzHeaders = "";
        for (Map.Entry<String, String> entry : xAmzHeaders.entrySet()) {
            canonicalizedAmzHeaders += String.format("%s:%s\n", entry.getKey(),
                    entry.getValue().trim());
        }

        return canonicalizedAmzHeaders;
    }

    /*
     * 1. If the request specifies a bucket using the HTTP Host header
     * (virtual hosted-style), append the bucket name preceded by a "/"
     * Ex -"/bucketname"
     *
     * 2. Append the path part of the un-decoded HTTP Request-URI,
     * up-to but not including the query string.
     *
     * 2.a. For a virtual hosted-style request
     * "https://johnsmith.s3.amazonaws.com/photos/puppy.jpg",
     * the CanonicalizedResource is "/johnsmith/photos/puppy.jpg".
     *
     * 2.b. For a path-style request,
     * "https://s3.amazonaws.com/johnsmith/photos/puppy.jpg",
     * the CanonicalizedResource is "/johnsmith/photos/puppy.jpg"
     *
     * 3. Create sub resource string.
     */
    private String createCanonicalizedResource(ClientRequestToken clientRequestToken) {
        String canonicalResource = "";

        if (clientRequestToken.isVirtualHost()) {
                canonicalResource += "/" + clientRequestToken.getBucketName();
        }

        canonicalResource += clientRequestToken.getUri();
        canonicalResource += createSubResourceString(clientRequestToken);

        return canonicalResource;
    }

    /**
     * If the request addresses a subresource, such as ?versioning, ?location,
     * ?acl, ?torrent, ?lifecycle, or ?versionid, append the subresource, its
     * value if it has one, and the question mark. Note that in case of multiple
     * subresources, subresources must be lexicographically sorted by
     * subresource name and separated by '&'.
     *
     * Ex - ?acl&versionId=value.
     */
    private String createSubResourceString(ClientRequestToken clientRequestToken) {
        List<String> subResources = Arrays.asList(
                "acl", "delete", "lifecycle", "location", "logging", "notification", "partNumber",
                "policy", "requestPayment", "torrent", "uploadId", "uploads",
                "versionId", "versioning", "versions", "website");

        Map<String, String> queryResources = new TreeMap<>();
        String[] tokens = clientRequestToken.getQuery().split("&");

        for (String s : tokens) {
            String[] keyPair = s.split("=");

            if (subResources.contains(keyPair[0])) {
                if (keyPair.length == 2) {
                    queryResources.put(keyPair[0], keyPair[1]);
                } else {
                    queryResources.put(s, "");
                }
            }
        }

        String subResourceString = "";

        Iterator<Map.Entry<String, String>> entries = queryResources.entrySet().iterator();
        if (queryResources.size() > 0) {
            subResourceString += "?";
        }

        while (entries.hasNext()) {
            Map.Entry<String, String> entry = entries.next();
            subResourceString += entry.getKey();

            if (!entry.getValue().isEmpty()) {
                subResourceString += String.format("=%s", entry.getValue());
            }

            if (entries.hasNext()) {
                subResourceString += "&";
            }
        }

        return subResourceString;
    }
}
