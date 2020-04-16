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

import com.seagates3.exception.InvalidTokenException;
import com.seagates3.exception.InvalidArgumentException;
import com.seagates3.util.IEMUtil;
import io.netty.handler.codec.http.FullHttpRequest;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.generator.ResponseGenerator;
import com.seagates3.exception.InvalidAccessKeyException;

import java.util.Map;
import org.slf4j.Logger;
import java.util.regex.Pattern;
import java.util.regex.Matcher;
import org.slf4j.LoggerFactory;

public class ClientRequestParser {

    private static final String REQUEST_PARSER_PACKAGE
            = "com.seagates3.authentication";
    private static final String AWS_V2_AUTHRORIAZATION_PATTERN
            = "AWS [A-Za-z0-9-_]+:[a-zA-Z0-9+/=]+";
    private static final String AWS_V4_AUTHRORIAZATION_PATTERN
            = "AWS4-HMAC-SHA256[\\w\\W]+";
    private ResponseGenerator responseGenerator
            = new ResponseGenerator();
    private final static Logger LOGGER =
            LoggerFactory.getLogger(ClientRequestParser.class.getName());
    private static final Pattern ACCESS_KEY_PATTERN  = Pattern.compile("[\\w-]+");
    private static final Pattern V2_PATTERN = Pattern.compile("^AWS ");
    private static final Pattern V4_PATTERN = Pattern.compile("^AWS4-HMAC-SHA256");



    public static ClientRequestToken parse(FullHttpRequest httpRequest,
            Map<String, String> requestBody) throws InvalidAccessKeyException,
            InvalidArgumentException {
        String requestAction = requestBody.get("Action");
        String authorizationHeader;
        ClientRequestToken.AWSSigningVersion awsSigningVersion;

        if (requestAction.equals("AuthenticateUser")
                || requestAction.equals("AuthorizeUser")) {
            authorizationHeader = requestBody.get("authorization");

        } else if (requestAction.equals("ValidateACL")) {

          if (requestBody.get("ACL") != null) {

            ClientRequestToken clientRequestToken = new ClientRequestToken();

            return clientRequestToken;
          } else {

            return null;
          }

        } else if (requestAction.equals("ValidatePolicy")) {

          if (requestBody.get("Policy") != null) {

            ClientRequestToken clientRequestToken = new ClientRequestToken();

            return clientRequestToken;
          } else {

            return null;
          }

        } else {
            authorizationHeader = httpRequest.headers().get("authorization");
        }
        LOGGER.debug("authheader is" + authorizationHeader);

        if (authorizationHeader == null ||
            authorizationHeader.replaceAll("\\s+", "").isEmpty()) {
            return null;
        }
        ClientRequestParser clientRequestParser = new ClientRequestParser();

        clientRequestParser.validateAccessKey(authorizationHeader);
        clientRequestParser = null;
        if (authorizationHeader.matches(AWS_V2_AUTHRORIAZATION_PATTERN)) {
            awsSigningVersion = ClientRequestToken.AWSSigningVersion.V2;
        }
        else if(authorizationHeader.matches(AWS_V4_AUTHRORIAZATION_PATTERN))
        {
            awsSigningVersion = ClientRequestToken.AWSSigningVersion.V4;
        }
        else
        {
            return null;
        }

        AWSRequestParser awsRequestParser = getAWSRequestParser(awsSigningVersion);
        ClientRequestToken clientrequesttoken = null;
        if (requestAction.equals("AuthenticateUser")
                || requestAction.equals("AuthorizeUser")) {
            try {
              if (awsRequestParser != null) {
                clientrequesttoken = awsRequestParser.parse(requestBody);
              }
            }
            catch (InvalidTokenException ex) {
                LOGGER.error("Error while parsing request : "+ ex.getMessage());
            }
        } else {

            try {
              if (awsRequestParser != null) {
                clientrequesttoken = awsRequestParser.parse(httpRequest);
              }
            } catch (InvalidTokenException ex) {
                    LOGGER.error("Error while parsing request : "+ ex.getMessage());
            }
        }
        return clientrequesttoken;
    }

    private static AWSRequestParser getAWSRequestParser(
            ClientRequestToken.AWSSigningVersion awsSigningVersion) {

        String signVersion = awsSigningVersion.toString();
        String awsRequestParserClass = toRequestParserClass(signVersion);

        Class<?> awsRequestParser;
        Object obj;

        try {
            awsRequestParser = Class.forName(awsRequestParserClass);
            obj = awsRequestParser.newInstance();

            return (AWSRequestParser) obj;
        } catch (ClassNotFoundException | SecurityException ex) {
            IEMUtil.log(IEMUtil.Level.ERROR, IEMUtil.CLASS_NOT_FOUND_EX,
                    "Failed to get required class",
                    String.format("\"cause\": \"%s\"", ex.getCause()));
        } catch (IllegalAccessException | IllegalArgumentException | InstantiationException ex) {
            LOGGER.error("Error occured while creating aws request parser");
            LOGGER.error(ex.toString());
        }

        return null;
    }

    private static String toRequestParserClass(String signingVersion) {
        return String.format("%s.AWSRequestParser%s", REQUEST_PARSER_PACKAGE, signingVersion);
    }

    private void validateAccessKey (String authorizationHeader)
            throws InvalidAccessKeyException, InvalidArgumentException {

        ServerResponse serverResponse;
        String access_key="";
        String[] tokens, subTokens;

        //AuthorizationHeader of v2 is of type AWS AKIAJTYX36YCKQSAJT7Q:6IaKnMXRsQcsblXItQnSNB3EJEo=
        //V2 Pattern to match "AWS "
        //AuthorizationHeader of v4 is of type AWS4-HMAC-SHA256 Credential=AK IAJTYX36YCKQSAJT7Q/20190314/US/s3/          aws4_request,SignedHeaders=host;x-amz-content-sha256;x-amz-date,Signature=310b0122f12459dfea171cac82bd          4930626d5a8db695fef6bc7bfd2a30a39ea3

        if ((V2_PATTERN.matcher(authorizationHeader)).lookingAt()) {
            tokens = authorizationHeader.split(":");
            subTokens = tokens[0].split(" ");
            if (subTokens.length != 2) {
                serverResponse = responseGenerator.invalidArgument();
                throw new InvalidArgumentException(serverResponse);
            }
            access_key=subTokens[1];
        }
        else if ((V4_PATTERN.matcher(authorizationHeader)).lookingAt())
        {
            tokens = authorizationHeader.split(",");
            String[] credTokens;
            subTokens = tokens[0].split("=");
            credTokens = subTokens[1].split("/");
            access_key=credTokens[0];
        }
        else
        {
            serverResponse = responseGenerator.invalidArgument();
            throw new InvalidArgumentException(serverResponse);
        }

        if (access_key.contains(" ")) {
            serverResponse = responseGenerator.invalidArgument();
            throw new InvalidArgumentException(serverResponse);
        }

        Matcher matcher = ACCESS_KEY_PATTERN.matcher(access_key);

        if (!(matcher.matches())) {
            serverResponse = responseGenerator.invalidAccessKey();
            throw new InvalidAccessKeyException(serverResponse);
        }

}

}

