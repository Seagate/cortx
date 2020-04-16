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
 * Original creation date: 22-Jan-2016
 */
package com.seagates3.controller;

import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.dao.AccessKeyDAO;
import com.seagates3.dao.DAODispatcher;
import com.seagates3.dao.DAOResource;
import com.seagates3.dao.RequestorDAO;
import com.seagates3.dao.RoleDAO;
import com.seagates3.dao.SAMLProviderDAO;
import com.seagates3.exception.DataAccessException;
import com.seagates3.exception.InvalidSAMLResponseException;
import com.seagates3.exception.SAMLInitializationException;
import com.seagates3.exception.SAMLInvalidCertificateException;
import com.seagates3.exception.SAMLReponseParserException;
import com.seagates3.model.AccessKey;
import com.seagates3.model.Account;
import com.seagates3.model.Requestor;
import com.seagates3.model.Role;
import com.seagates3.model.SAMLProvider;
import com.seagates3.model.SAMLResponseTokens;
import com.seagates3.model.User;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.generator.SAMLSessionResponseGenerator;
import com.seagates3.saml.SAMLUtil;
import com.seagates3.saml.SAMLUtilFactory;
import com.seagates3.saml.SAMLValidator;
import com.seagates3.service.AccessKeyService;
import com.seagates3.service.UserService;
import com.seagates3.util.BinaryUtil;
import com.seagates3.util.IEMUtil;
import io.netty.buffer.Unpooled;
import io.netty.handler.codec.http.DefaultFullHttpResponse;
import io.netty.handler.codec.http.FullHttpRequest;
import io.netty.handler.codec.http.FullHttpResponse;
import static io.netty.handler.codec.http.HttpHeaders.Names.CONTENT_LENGTH;
import static io.netty.handler.codec.http.HttpHeaders.Names.CONTENT_TYPE;
import static io.netty.handler.codec.http.HttpHeaders.Names.LOCATION;
import io.netty.handler.codec.http.HttpResponseStatus;
import io.netty.handler.codec.http.HttpVersion;
import io.netty.handler.codec.http.QueryStringDecoder;
import io.netty.handler.codec.http.QueryStringEncoder;
import java.io.UnsupportedEncodingException;
import java.util.List;
import java.util.Map;

/**
 * TODO - Refactor this class to inherit from Abstract controller.
 *
 */
public class SAMLWebSSOController {

    private final Map<String, String> requestBody;
    private final String SESSION_TOKEN = "session_token";
    private final String USER_NAME = "user_name";
    private final String ACCOUNT_NAME = "account_name";

    /**
     * Constructor
     *
     * @param requestBody API Request body.
     */
    public SAMLWebSSOController(Map<String, String> requestBody) {
        this.requestBody = requestBody;
    }

    public FullHttpResponse samlSignIn() {
        String samlAssertion = BinaryUtil.base64DecodeString(
                requestBody.get("SAMLResponse"));

        SAMLUtil samlutil;
        SAMLResponseTokens samlResponseTokens;
        try {
            samlutil = SAMLUtilFactory.getSAMLUtil(samlAssertion);
            samlResponseTokens = samlutil.parseSamlResponse(samlAssertion);
        } catch (SAMLInitializationException ex) {
            return createErrorResponse("Internal server error");
        } catch (SAMLReponseParserException |
                SAMLInvalidCertificateException ex) {
            return createErrorResponse("Invalid SAML assertion");
        }

        SAMLProvider samlProvider;
        try {
            samlProvider = getSAMLProvider(samlResponseTokens.getIssuer());
        } catch (DataAccessException ex) {
            return createErrorResponse("Internal server error");
        }

        if (!samlProvider.exists()) {
            return createErrorResponse("SAML Provider doesn't exist.");
        }

        SAMLValidator samlValidator;
        try {
            samlValidator = new SAMLValidator(samlProvider, samlutil);
            samlValidator.validateSAMLResponse(samlResponseTokens);
        } catch (SAMLInitializationException ex) {
            return createErrorResponse("Internal server error");
        } catch (InvalidSAMLResponseException ex) {
            return createErrorResponse("Invalid SAML assertion");
        }

        int duration = samlutil.getExpirationDuration(samlResponseTokens, -1);

        Role[] roles;
        try {
            roles = getRoles(samlProvider.getAccount());
        } catch (DataAccessException ex) {
            return createErrorResponse("Internal server error");
        }

        if (roles.length == 0) {
            return createErrorResponse("No role found.");
        }

        String roleName = roles[0].getName();

        /**
         * TODO - Return the list of roles to the user and ask the user to
         * select a role.
         *
         * Temporary fix - Select the first role always UNTIL policies are
         * implemented.
         */
        String roleSessionName = samlutil.getRoleSessionName(
                samlResponseTokens.getResponseAttributes());

        User user;
        AccessKey accessKey;
        try {
            user = UserService.createRoleUser(samlProvider.getAccount(),
                    roleName, roleSessionName);
            accessKey = AccessKeyService.createFedAccessKey(user, duration);
        } catch (DataAccessException ex) {
            return createErrorResponse("Internal server error");
        }

        return createSAMLSigninSuccessResponse(user, accessKey);
    }

    /**
     * TODO - Improve the security of this API since it returns the AWS creds of
     * the user.
     *
     * Create a new session by sending the temporary credentials of the user.
     *
     * @param httpRequest
     * @return Full Http Response
     */
    public ServerResponse createSession(FullHttpRequest httpRequest) {
        QueryStringDecoder queryParams = new QueryStringDecoder(
                httpRequest.getUri());
        Map<String, List<String>> cookieToken = queryParams.parameters();

        SAMLSessionResponseGenerator samlSessionResponseGenerator
                = new SAMLSessionResponseGenerator();

        String session_token = cookieToken.get(SESSION_TOKEN).get(0);
        if (session_token == null) {
            return samlSessionResponseGenerator.badRequest();
        }

        AccessKey accesskey;
        try {
            accesskey = getAccessKeyFromToken(session_token);
        } catch (DataAccessException ex) {
            return samlSessionResponseGenerator.internalServerError();
        }

        if (!accesskey.exists()) {
            return samlSessionResponseGenerator.noSuchEntity();
        }

        Requestor requestor;
        try {
            requestor = getRequestor(accesskey);
        } catch (DataAccessException ex) {
            return samlSessionResponseGenerator.internalServerError();
        }

        return samlSessionResponseGenerator.generateCreateResponse(requestor);
    }

    private FullHttpResponse createSAMLSigninSuccessResponse(User user,
            AccessKey accessKey) {
        FullHttpResponse response;
        QueryStringEncoder redirectURL = new QueryStringEncoder(
                AuthServerConfig.getConsoleURL());

        redirectURL.addParam(SESSION_TOKEN, accessKey.getToken());
        redirectURL.addParam(USER_NAME, user.getName());
        redirectURL.addParam(ACCOUNT_NAME, user.getAccountName());

        response = new DefaultFullHttpResponse(HttpVersion.HTTP_1_1,
                HttpResponseStatus.FOUND);

        response.headers().set(LOCATION, redirectURL.toString());
        response.headers().set(CONTENT_LENGTH, response.content().readableBytes());

        return response;
    }

    private FullHttpResponse createErrorResponse(String responseMessage) {
        FullHttpResponse response;

        try {
            response = new DefaultFullHttpResponse(HttpVersion.HTTP_1_1,
                    HttpResponseStatus.BAD_REQUEST,
                    Unpooled.wrappedBuffer(responseMessage.getBytes("UTF-8"))
            );
        } catch (UnsupportedEncodingException ex) {
            IEMUtil.log(IEMUtil.Level.ERROR, IEMUtil.UTF8_UNAVAILABLE,
                    "UTF-8 encoding is not supported", null);
            response = null;
        }
        if (response != null) {
        response.headers().set(CONTENT_TYPE, "text/xml");
        response.headers().set(CONTENT_LENGTH, response.content().readableBytes());
        }
        return response;
    }

    /**
     * Get the SAML Provider from the database.
     *
     * @param account Account.
     * @param providerName IDP Provider name.
     * @return SAMLProvider.
     * @throws DataAccessException
     */
    private SAMLProvider getSAMLProvider(String issuer)
            throws DataAccessException {
        SAMLProviderDAO samlProviderDao
                = (SAMLProviderDAO) DAODispatcher.getResourceDAO(
                        DAOResource.SAML_PROVIDER);
        return samlProviderDao.find(issuer);
    }

    /**
     * Get the Role from the database.
     *
     * @param account Account.
     * @param roleName Role name.
     * @return Role.
     * @throws DataAccessException
     */
    private Role[] getRoles(Account account)
            throws DataAccessException {
        RoleDAO roleDAO
                = (RoleDAO) DAODispatcher.getResourceDAO(DAOResource.ROLE);
        return roleDAO.findAll(account, "/");
    }

    private Requestor getRequestor(AccessKey accessKey)
            throws DataAccessException {
        RequestorDAO requestorDAO = (RequestorDAO) DAODispatcher
                .getResourceDAO(DAOResource.REQUESTOR);
        return requestorDAO.find(accessKey);
    }

    private AccessKey getAccessKeyFromToken(String secretToken)
            throws DataAccessException {
        AccessKeyDAO accessKeyDAO = (AccessKeyDAO) DAODispatcher.getResourceDAO(
                DAOResource.ACCESS_KEY);

        return accessKeyDAO.findFromToken(secretToken);
    }

}
