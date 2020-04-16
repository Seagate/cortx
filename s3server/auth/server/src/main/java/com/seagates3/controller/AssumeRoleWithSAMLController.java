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
 * Original creation date: 28-Oct-2015
 */
package com.seagates3.controller;

import com.seagates3.dao.AccountDAO;
import com.seagates3.dao.DAODispatcher;
import com.seagates3.dao.DAOResource;
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
import com.seagates3.response.generator.AssumeRoleWithSAMLResponseGenerator;
import com.seagates3.saml.SAMLUtil;
import com.seagates3.saml.SAMLUtilFactory;
import com.seagates3.saml.SAMLValidator;
import com.seagates3.service.AccessKeyService;
import com.seagates3.service.UserService;
import com.seagates3.util.BinaryUtil;
import java.util.Map;

public class AssumeRoleWithSAMLController extends AbstractController {

    AssumeRoleWithSAMLResponseGenerator responseGenerator;

    /**
     * Constructor
     *
     * @param requestor Requestor
     * @param requestBody API Request body.
     */
    public AssumeRoleWithSAMLController(Requestor requestor,
            Map<String, String> requestBody) {
        super(requestor, requestBody);

        responseGenerator = new AssumeRoleWithSAMLResponseGenerator();
    }

    /**
     * Validate the SAML response and create temporary credentials for the role
     * user.
     *
     * @return ServerResponse
     */
    @Override
    public ServerResponse create() {
        String samlAssertion = BinaryUtil.base64DecodeString(
                requestBody.get("SAMLAssertion"));

        String[] tokens = requestBody.get("PrincipalArn")
                .split("arn:seagate:iam::");
        tokens = tokens[1].split(":");
        String providerAccountName = tokens[0];
        String providerName = tokens[1];

        tokens = requestBody.get("RoleArn").split("arn:seagate:iam::");
        tokens = tokens[1].split(":");
        String roleAccountName = tokens[0];
        String roleName = tokens[1];

        /**
         * Provider and Role should belong to the same Account.
         */
        if (!providerAccountName.equals(roleAccountName)) {
            return responseGenerator.invalidParametervalue();
        }

        Account account;
        try {
            account = getAccount(providerAccountName);
        } catch (DataAccessException ex) {
            return responseGenerator.internalServerError();
        }

        if (!account.exists()) {
            return responseGenerator.noSuchEntity();
        }

        SAMLProvider samlProvider;
        try {
            samlProvider = getSAMLProvider(account, providerName);
        } catch (DataAccessException ex) {
            return responseGenerator.internalServerError();
        }

        if (!samlProvider.exists()) {
            return responseGenerator.noSuchEntity();
        }

        Role role;
        try {
            role = getRole(account, roleName);
        } catch (DataAccessException ex) {
            return responseGenerator.internalServerError();
        }

        if (!role.exists()) {
            return responseGenerator.noSuchEntity();
        }

        SAMLUtil samlutil;
        try {
            samlutil = SAMLUtilFactory.getSAMLUtil(samlAssertion);
        } catch (SAMLInitializationException ex) {
            return responseGenerator.internalServerError();
        }

        SAMLResponseTokens samlResponseTokens;
        try {
            samlResponseTokens = samlutil.parseSamlResponse(samlAssertion);
        } catch (SAMLReponseParserException |
                SAMLInvalidCertificateException ex) {
            return responseGenerator.invalidParametervalue();
        }

        SAMLValidator samlValidator;
        try {
            samlValidator = new SAMLValidator(samlProvider, samlutil);
        } catch (SAMLInitializationException ex) {
            return responseGenerator.internalServerError();
        }

        try {
            samlValidator.validateSAMLResponse(samlResponseTokens);
        } catch (InvalidSAMLResponseException ex) {
            return ex.getServerResponse();
        }

        /**
         * If Response has "SessionNotOnOrAfter", calculate the duration from
         * current time.
         *
         * Else, set duration = 3600 seconds (default)
         */
        int duration;
        String durationSeconds = requestBody.get("DurationSeconds");
        if (durationSeconds != null) {
            duration = samlutil.getExpirationDuration(samlResponseTokens,
                    Integer.parseInt(durationSeconds));
        } else {
            duration = samlutil.getExpirationDuration(samlResponseTokens, -1);
        }

        String roleSessionName = samlutil.getRoleSessionName(
                samlResponseTokens.getResponseAttributes());
        User user;
        AccessKey accessKey;
        try {
            user = UserService.createRoleUser(account, roleName,
                    roleSessionName);
            accessKey = AccessKeyService.createFedAccessKey(user, duration);
        } catch (DataAccessException ex) {
            return responseGenerator.internalServerError();
        }

        return responseGenerator.generateCreateResponse(user, accessKey,
                samlResponseTokens);
    }

    /**
     * Get the account details from database.
     *
     * @param accountName Account Name.
     * @return Account
     * @throws DataAccessException
     */
    private Account getAccount(String accountName) throws DataAccessException {
        AccountDAO accountDAO
                = (AccountDAO) DAODispatcher.getResourceDAO(DAOResource.ACCOUNT);
        return accountDAO.find(accountName);
    }

    /**
     * Get the SAML Provider from the database.
     *
     * @param account Account.
     * @param providerName IDP Provider name.
     * @return SAMLProvider.
     * @throws DataAccessException
     */
    private SAMLProvider getSAMLProvider(Account account, String providerName)
            throws DataAccessException {
        SAMLProviderDAO samlProviderDao
                = (SAMLProviderDAO) DAODispatcher.getResourceDAO(
                        DAOResource.SAML_PROVIDER);
        return samlProviderDao.find(account, providerName);
    }

    /**
     * Get the Role from the database.
     *
     * @param account Account.
     * @param roleName Role name.
     * @return Role.
     * @throws DataAccessException
     */
    private Role getRole(Account account, String roleName)
            throws DataAccessException {
        RoleDAO roleDAO
                = (RoleDAO) DAODispatcher.getResourceDAO(DAOResource.ROLE);
        return roleDAO.find(account, roleName);
    }
}
