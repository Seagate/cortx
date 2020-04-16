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

import com.seagates3.exception.InvalidTokenException;
import com.seagates3.model.Requestor;

import com.seagates3.util.BinaryUtil;
import java.io.UnsupportedEncodingException;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.URLEncoder;
import java.util.Iterator;
import java.util.Map;
import java.util.TreeMap;

import com.seagates3.util.IEMUtil;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class AWSV4Sign implements AWSSign {

    private final Logger LOGGER = LoggerFactory.getLogger(
            AWSV4Sign.class.getName());

    private final String STREAMING_AWS4_HMAC_SHA256_PAYLOAD
            = "STREAMING-AWS4-HMAC-SHA256-PAYLOAD";

    /**
     * Return true if the signature is valid.
     *
     * @param clientRequestToken
     * @param requestor
     * @return True if authenticated.
     * @throws InvalidTokenException
     */
    @Override
    public Boolean authenticate(ClientRequestToken clientRequestToken,
                   Requestor requestor) throws InvalidTokenException {
        Map<String, String> requestHeaders
                = clientRequestToken.getRequestHeaders();

        if (requestHeaders.containsKey("x-amz-content-sha256")
                && requestHeaders.get("x-amz-content-sha256")
                .equals(STREAMING_AWS4_HMAC_SHA256_PAYLOAD)) {

            LOGGER.debug("Chunked upload. Verify seed signature");
            return verifyChunkedSeedSignature(clientRequestToken, requestor);
        } else if (requestHeaders.containsKey("previous-signature-sha256")) {

            LOGGER.debug("Chunked upload. Verify chunk signature.");
            return verifyChunkedSignature(clientRequestToken, requestor);
        } else {

            LOGGER.debug("Verify signature of unchunked request.");
            return verifySignature(clientRequestToken, requestor);
        }
    }

    /**
     * Verify the signature of a regular S3/IAM API.
     *
     * @param clientRequestToken
     * @param requestor
     * @return
     * @throws BadRequestException
     * @throws InvalidTokenException
     */
    private Boolean verifySignature(ClientRequestToken clientRequestToken,
                       Requestor requestor) throws InvalidTokenException {
        String canonicalRequest, stringToSign, signature;
        byte[] signingKey;

        canonicalRequest = createCanonicalRequest(clientRequestToken);
        LOGGER.debug("Canonical request- " + canonicalRequest);

        stringToSign = createStringToSign(canonicalRequest, clientRequestToken);
        LOGGER.debug("String to sign- " + stringToSign);

        String secretKey = requestor.getAccesskey().getSecretKey();
        signingKey = deriveSigningKey(clientRequestToken, secretKey);
        LOGGER.debug("Signing key- " + signingKey);

        signature = calculateSignature(signingKey, stringToSign);
        LOGGER.debug("Request signature- " + clientRequestToken.getSignature());
        LOGGER.debug("Calculated signature- " + signature);

        return (signature != null)
                   ? signature.equals(clientRequestToken.getSignature())
                   : false;
    }

    /**
     * Authenticate seed signature of a chunked upload request.
     *
     * @param clientRequestToken
     * @param requestor
     * @return
     * @throws InvalidTokenException
     */
    private Boolean verifyChunkedSeedSignature(
            ClientRequestToken clientRequestToken, Requestor requestor)
                                         throws InvalidTokenException {
        String canonicalRequest, stringToSign, signature;
        byte[] signingKey;

        canonicalRequest = createCanonicalRequestChunkedSeed(clientRequestToken);
        LOGGER.debug("Canonical request- " + canonicalRequest);

        stringToSign = createStringToSign(canonicalRequest,
                clientRequestToken);
        LOGGER.debug("String to sign- " + stringToSign);

        String secretKey = requestor.getAccesskey().getSecretKey();
        signingKey = deriveSigningKey(clientRequestToken, secretKey);
        LOGGER.debug("Signing key- " + signingKey);

        signature = calculateSignature(signingKey, stringToSign);
        LOGGER.debug("Request signature- " + clientRequestToken.getSignature());
        LOGGER.debug("Calculated signature- " + signature);

        return (signature != null)
                   ? signature.equals(clientRequestToken.getSignature())
                   : false;
    }

    /**
     * Verify signature the chunk in a chunked upload request.
     *
     * @param clientRequestToken
     * @param requestor
     * @return
     */
    private Boolean verifyChunkedSignature(
            ClientRequestToken clientRequestToken, Requestor requestor) {
        String stringToSign, signature, currentSign;
        byte[] signingKey;

        stringToSign = createStringToSignChunked(clientRequestToken);
        LOGGER.debug("String to sign- " + stringToSign);

        String secretKey = requestor.getAccesskey().getSecretKey();
        signingKey = deriveSigningKey(clientRequestToken, secretKey);
        LOGGER.debug("Signing key- " + signingKey);

        signature = calculateSignature(signingKey, stringToSign);
        LOGGER.debug("Request signature- " + clientRequestToken.getSignature());
        LOGGER.debug("Calculated signature- " + signature);

        currentSign = clientRequestToken.getRequestHeaders()
                .get("current-signature-sha256");

        clientRequestToken.setSignature(currentSign);
        return (signature != null) ? signature.equals(currentSign) : false;
    }

    /**
     * Canonical Request is in the following format.
     *
     * HTTPRequestMethod + '\n' + CanonicalURI + '\n' + CanonicalQueryString +
     * '\n' + CanonicalHeaders + '\n' + SignedHeaders + '\n' +
     * HexEncode(Hash(RequestPayload))
      * @throws InvalidTokenException
     */
    private String createCanonicalRequest(
            ClientRequestToken clientRequestToken)
                    throws InvalidTokenException {
        String httpMethod, canonicalURI, canonicalQuery, canonicalHeader,
                hashedPayload, canonicalRequest;

        httpMethod = clientRequestToken.getHttpMethod();
        canonicalURI = clientRequestToken.getUri();
        /**
         * getCanonicalQuery is not required for s3cmd as query field in
         * clientRequestToken is already encoded from s3server.
         * If required in other CLI's query field needs to be URL-encoded
         * in getCanonicalQuery.
         */
        canonicalQuery = getCanonicalQuery(clientRequestToken.getQuery());
        canonicalHeader = createCanonicalHeader(clientRequestToken);
        hashedPayload = createHashedPayload(clientRequestToken);

        canonicalRequest = String.format("%s\n%s\n%s\n%s\n%s\n%s",
                httpMethod,
                canonicalURI,
                canonicalQuery,
                canonicalHeader,
                clientRequestToken.getSignedHeaders(),
                hashedPayload);

        return canonicalRequest;
    }

    /**
     * Create a canonical request for seed request of a chunked request.
     *
     * HTTPRequestMethod + '\n' + CanonicalURI + '\n' + CanonicalQueryString +
     * '\n' + CanonicalHeaders + '\n' + SignedHeaders + '\n' +
     * STREAMING-AWS4-HMAC-SHA256-PAYLOAD
     *
     * @param clientRequestToken
     * @return
     * @throws InvalidTokenException
     */
    private String createCanonicalRequestChunkedSeed(
            ClientRequestToken clientRequestToken)
                        throws InvalidTokenException {
        String httpMethod, canonicalURI, canonicalQuery, canonicalHeader,
                canonicalRequest;

        httpMethod = clientRequestToken.getHttpMethod();
        canonicalURI = clientRequestToken.getUri();
        canonicalQuery = getCanonicalQuery(clientRequestToken.getQuery());
        canonicalHeader = createCanonicalHeader(clientRequestToken);

        canonicalRequest = String.format("%s\n%s\n%s\n%s\n%s\n%s",
                httpMethod,
                canonicalURI,
                canonicalQuery,
                canonicalHeader,
                clientRequestToken.getSignedHeaders(),
                STREAMING_AWS4_HMAC_SHA256_PAYLOAD);

        return canonicalRequest;
    }

    /*
     * <IEM_INLINE_DOCUMENTATION>
     *     <event_code>048004002</event_code>
     *     <application>S3 Authserver</application>
     *     <submodule>Reflection</submodule>
     *     <description>Failed to invoke method</description>
     *     <audience>Development</audience>
     *     <details>
     *         Failed to invoke method.
     *         The data section of the event has following keys:
     *           time - timestamp
     *           node - node name
     *           pid - process id of Authserver
     *           file - source code filename
     *           line - line number within file where error occurred
     *           cause - cause of exception
     *     </details>
     *     <service_actions>
     *         Save authserver log files and contact development team for
     *         further investigation.
     *     </service_actions>
     * </IEM_INLINE_DOCUMENTATION>
     *
     */
    /**
     * Calculate the hash of Payload using the following formula. HashedPayload
     * = Lowercase(HexEncode(Hash(requestPayload)))
     *
     * @param clientRequestToken
     * @return Hashed Payload
     */
    private String createHashedPayload(ClientRequestToken clientRequestToken) {
        if (clientRequestToken.getRequestHeaders().get("x-amz-content-sha256") != null) {
            return clientRequestToken.getRequestHeaders().get("x-amz-content-sha256");
        } else {
            String hashMethodName = AWSSign.AWSHashFunction.get(
                    clientRequestToken.getSigningAlgorithm());
            Method method;
            String hashedPayload = "";
            try {
                method = BinaryUtil.class.getMethod(hashMethodName, byte[].class);
                byte[] hashedText = (byte[]) method.invoke(null,
                        clientRequestToken.getRequestPayload().getBytes());
                hashedPayload = new String(BinaryUtil.encodeToHex(hashedText)).toLowerCase();
            } catch (NoSuchMethodException | IllegalAccessException ex) {
                IEMUtil.log(IEMUtil.Level.ERROR, IEMUtil.NO_SUCH_METHOD_EX,
                        "Failed to invoke method",
                        String.format("\"cause\": \"%s\"", ex.getCause()));
                LOGGER.error("Exception description: ", ex);
            } catch (SecurityException | IllegalArgumentException |
                    InvocationTargetException ex) {
                LOGGER.error("Exception description: ", ex);
            }

            return hashedPayload;
        }
    }

    /**
     * The string to sign includes meta information about your request and about
     * the canonical request.
     *
     * Structure of String to sign- Algorithm + '\n' + RequestDate + '\n' +
     * CredentialScope + '\n' + HashedCanonicalRequest
     */
    private String createStringToSign(String canonicalRequest,
            ClientRequestToken clientRequestToken) {
        String stringToSign, requestDate, hexEncodedCRHash;

        requestDate = clientRequestToken.getRequestHeaders().get("x-amz-date");
        hexEncodedCRHash = BinaryUtil.hexEncodedHash(canonicalRequest);

        stringToSign = String.format("%s\n%s\n%s\n%s",
                clientRequestToken.getSigningAlgorithm(), requestDate,
                clientRequestToken.getCredentialScope(), hexEncodedCRHash);

        return stringToSign;
    }

    /**
     * The string to sign includes meta information about your request and about
     * the canonical request.
     *
     * Structure of String to sign Algorithm + '\n' + RequestDate + '\n' +
     * CredentialScope + '\n' + HashedCanonicalRequest
     */
    private String createStringToSignChunked(
            ClientRequestToken clientRequestToken) {
        String stringToSign, requestDate, prevSign, hashCurrentChunk;
        String hashEmptyInput
                = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

        requestDate = clientRequestToken.getRequestHeaders().get("x-amz-date");
        prevSign = clientRequestToken.getRequestHeaders()
                .get("previous-signature-sha256");

        hashCurrentChunk = clientRequestToken.getRequestHeaders()
                .get("x-amz-content-sha256");

        if (hashCurrentChunk.startsWith("STREAMING-AWS4-HMAC-SHA256-PAYLOAD")) {
          String tokens[] = hashCurrentChunk.split(",");
          hashCurrentChunk = "";
          hashCurrentChunk = tokens[1];
        }

        stringToSign = String.format("AWS4-HMAC-SHA256-PAYLOAD\n%s\n%s\n%s\n%s\n%s",
                requestDate, clientRequestToken.getCredentialScope(), prevSign,
                hashEmptyInput, hashCurrentChunk
        );

        return stringToSign;
    }

    /**
     * Before calculating the signature, derive a signing key secret access key.
     *
     * Algorithm-
     *
     * kSecret = Your AWS Secret Access Key kDate = HMAC("AWS4" + kSecret, Date)
     * kRegion = HMAC(kDate, Region) kService = HMAC(kRegion, Service) kSigning
     * = HMAC(kService, "aws4_request")
     */
    private byte[] deriveSigningKey(ClientRequestToken clientRequestToken,
            String secretKey) {
        try {
            byte[] kSecret = ("AWS4" + secretKey).getBytes("UTF-8");
            byte[] kDate = BinaryUtil.hmacSHA256(kSecret,
                    clientRequestToken.getDate().getBytes("UTF-8"));

            byte[] kRegion = BinaryUtil.hmacSHA256(kDate,
                    clientRequestToken.getRegion().getBytes("UTF-8"));

            byte[] kService = BinaryUtil.hmacSHA256(kRegion,
                    clientRequestToken.getService().getBytes("UTF-8"));

            byte[] kSigning = BinaryUtil.hmacSHA256(kService,
                    "aws4_request".getBytes("UTF-8"));
            return kSigning;
        } catch (UnsupportedEncodingException ex) {
            IEMUtil.log(IEMUtil.Level.ERROR, IEMUtil.UTF8_UNAVAILABLE,
                    "UTF-8 encoding is not supported", null);
        }

        return null;
    }

    /**
     * Return the signature of the request.
     *
     * signature = HexEncode(HMAC(derived-signing-key, string-to-sign))
     */
    private String calculateSignature(byte[] derivedSigningKey, String stringToSign) {
        try {
            byte[] signature = BinaryUtil.hmacSHA256(derivedSigningKey,
                    stringToSign.getBytes("UTF-8"));
            return BinaryUtil.toHex(signature);
        } catch (UnsupportedEncodingException ex) {
            IEMUtil.log(IEMUtil.Level.ERROR, IEMUtil.UTF8_UNAVAILABLE,
                    "UTF-8 encoding is not supported", null);
        }

        return null;
    }

    /**
     * The canonical headers consist of a list of all the HTTP headers are
     * include as a part of the AWS request. Convert all header names to
     * lowercase and trim excess white space characters out of the header
     * values.
     *
     * Canonical Header = CanonicalHeadersEntry0 + CanonicalHeadersEntry1 + ...
     * + CanonicalHeadersEntryN
     *
     * CanonicalHeadersEntry = Lowercase(HeaderName) + ':' +
     * Trimall(HeaderValue) + '\n'
     * @throws InvalidTokenException
     */
    private String createCanonicalHeader(ClientRequestToken clientRequestToken)
                                                 throws InvalidTokenException {
        String headerValue;
        String canonicalHeader = "";
        Map<String, String> requestHeaders = clientRequestToken.getRequestHeaders();

        for (String s : clientRequestToken.getSignedHeaders().split(";")) {
            headerValue = requestHeaders.get(s);
            if (headerValue == null) {
                String errMsg = "Signed header :" + s +
                                " is not found in Request header list";
                LOGGER.error(errMsg);

                throw new InvalidTokenException(errMsg);
            }
            headerValue = headerValue.trim();
            if (s.equalsIgnoreCase("content-type")) {
                /*
                 * Strangely, the aws .net sdk doesn't send the content type.
                 * Hence the content type is hard coded.
                 */
                if (headerValue.isEmpty()) {
                  canonicalHeader += "content-type:\n";
                } else {
                    canonicalHeader += String.format("%s:%s\n", s, headerValue);
                }
            } else {
                canonicalHeader += String.format("%s:%s\n", s, headerValue);
            }
        }

        return canonicalHeader;
    }

    /**
     * To construct the canonical query string, complete the following steps:
     *
     * 1. URI-encode each parameter name and value according to the following
     * rules: a. Do not URL-encode any of the unreserved characters that RFC
     * 3986 defines: A-Z, a-z, 0-9, hyphen ( - ), underscore ( _ ), period ( .
     * ), and tilde ( ~ ). b. Percent-encode all other characters with %XY,
     * where X and Y are hexadecimal characters (0-9 and uppercase A-F). For
     * example, the space character must be encoded as %20 (not using '+', as
     * some encoding schemes do) and extended UTF-8 characters must be in the
     * form %XY%ZA%BC.
     *
     * 2. Sort the encoded parameter names by character code (that is, in strict
     * ASCII order). For example, a parameter name that begins with the
     * uppercase letter F (ASCII code 70) precedes a parameter name that begins
     * with a lowercase letter b (ASCII code 98).
     *
     * 3. Build the canonical query string by starting with the first parameter
     * name in the sorted list.
     *
     * 4. For each parameter, append the URI-encoded parameter name, followed by
     * the character '=' (ASCII code 61), followed by the URI-encoded parameter
     * value. Use an empty string for parameters that have no value.
     *
     * 5. Append the character '&' (ASCII code 38) after each parameter value
     * except for the last value in the list.
     */
    private String getCanonicalQuery(String query) {
        if (query == null || query.isEmpty()) {
            return "";
        }

        Map<String, String> queryParams = new TreeMap<>();
        String[] tokens = query.split("&");
        for (String token : tokens) {
            String[] subTokens = token.split("=");
            if (subTokens.length == 2) {
                queryParams.put(subTokens[0], subTokens[1]);
            } else {
                queryParams.put(subTokens[0], "");
            }
        }
        String canonicalString = "";
        Iterator<Map.Entry<String, String>> entries = queryParams.entrySet().iterator();
        while (entries.hasNext()) {
            Map.Entry<String, String> entry = entries.next();
            // Values already url encoded by s3server
            // so we can directly use it for verification.
            canonicalString += entry.getKey() + "=";
            canonicalString += entry.getValue();
            if (entries.hasNext()) {
                canonicalString += "&";
            }
        }

        return canonicalString;
    }
}
