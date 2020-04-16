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
 * Original creation date: 12-Oct-2015
 */
package com.seagates3.controller;

import com.seagates3.dao.DAODispatcher;
import com.seagates3.dao.DAOResource;
import com.seagates3.dao.SAMLProviderDAO;
import com.seagates3.exception.DataAccessException;
import com.seagates3.exception.SAMLInitializationException;
import com.seagates3.exception.SAMLInvalidCertificateException;
import com.seagates3.exception.SAMLReponseParserException;
import com.seagates3.model.Requestor;
import com.seagates3.model.SAMLProvider;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.generator.SAMLProviderResponseGenerator;
import com.seagates3.saml.SAMLUtil;
import com.seagates3.saml.SAMLUtilFactory;
import java.util.Map;
import javax.management.InvalidAttributeValueException;

public class SAMLProviderController extends AbstractController {

    SAMLProviderDAO samlProviderDao;
    SAMLProviderResponseGenerator samlProviderResponseGenerator;

    public SAMLProviderController(Requestor requestor,
            Map<String, String> requestBody) {
        super(requestor, requestBody);

        samlProviderDao = (SAMLProviderDAO) DAODispatcher.getResourceDAO(
                DAOResource.SAML_PROVIDER);
        samlProviderResponseGenerator = new SAMLProviderResponseGenerator();
    }

    /**
     * Create a SAML Provider.
     *
     * @return ServerResponse.
     */
    @Override
    public ServerResponse create() {
        String samlMetadata = requestBody.get("SAMLMetadataDocument");
        SAMLProvider samlProvider;
        try {
            samlProvider = samlProviderDao.find(
                    requestor.getAccount(), requestBody.get("Name"));
        } catch (DataAccessException ex) {
            return samlProviderResponseGenerator.internalServerError();
        }

        if (samlProvider.exists()) {
            return samlProviderResponseGenerator.entityAlreadyExists();
        }

        samlProvider.setSamlMetadata(samlMetadata);
        SAMLUtil samlutil;
        try {
            samlutil = SAMLUtilFactory.getSAMLUtil(samlMetadata);
        } catch (SAMLInitializationException ex) {
            return samlProviderResponseGenerator.internalServerError();
        }

        try {
            samlutil.getSAMLProvider(samlProvider, samlMetadata);
        } catch (SAMLReponseParserException ex) {
            return samlProviderResponseGenerator.invalidParametervalue();
        } catch (InvalidAttributeValueException |
                SAMLInvalidCertificateException ex) {
            return samlProviderResponseGenerator.invalidParametervalue();
        }

        try {
            samlProviderDao.save(samlProvider);
        } catch (DataAccessException ex) {
            return samlProviderResponseGenerator.internalServerError();
        }

        return samlProviderResponseGenerator.generateCreateResponse(
                samlProvider);
    }

    /**
     * Delete SAML provider.
     *
     * @return ServerResponse
     */
    @Override
    public ServerResponse delete() {
        String samlProviderName = getSAMLProviderName(
                requestBody.get("SAMLProviderArn"));

        SAMLProvider samlProvider;
        try {
            samlProvider = samlProviderDao.find(
                    requestor.getAccount(), samlProviderName);
        } catch (DataAccessException ex) {
            return samlProviderResponseGenerator.internalServerError();
        }

        if (!samlProvider.exists()) {
            return samlProviderResponseGenerator.noSuchEntity();
        }

        try {
            samlProviderDao.delete(samlProvider);
        } catch (DataAccessException ex) {
            return samlProviderResponseGenerator.internalServerError();
        }

        return samlProviderResponseGenerator.generateDeleteResponse();
    }

    /**
     * List all the SAML providers of an account.
     *
     * @return ServerResponse
     */
    @Override
    public ServerResponse list() {
        SAMLProvider[] samlProviderList;
        try {
            samlProviderList = samlProviderDao.findAll(
                    requestor.getAccount());
        } catch (DataAccessException ex) {
            return samlProviderResponseGenerator.internalServerError();
        }

        return samlProviderResponseGenerator.generateListResponse(
                samlProviderList);
    }

    /**
     * Update the SAML Provider metadata.
     *
     * @return ServerResponse
     */
    @Override
    public ServerResponse update() {
        String samlProviderName = getSAMLProviderName(
                requestBody.get("SAMLProviderArn"));

        SAMLProvider samlProvider;

        try {
            samlProvider = samlProviderDao.find(
                    requestor.getAccount(), samlProviderName);
        } catch (DataAccessException ex) {
            return samlProviderResponseGenerator.internalServerError();
        }

        if (!samlProvider.exists()) {
            return samlProviderResponseGenerator.noSuchEntity();
        }

        try {
            samlProviderDao.update(samlProvider,
                    requestBody.get("SAMLMetadataDocument"));
        } catch (DataAccessException ex) {
            return samlProviderResponseGenerator.internalServerError();
        }

        return samlProviderResponseGenerator.generateUpdateResponse(
                samlProviderName);
    }

    /**
     * TODO Write a generic implementation to parse ARN.
     *
     * Get the SAML provider name from ARN. ARN should be in this format
     * arn:seagate:iam::<account name>:<idp provider name>
     *
     * @param arn SAML provider ARN
     * @return SAML provider name
     */
    private String getSAMLProviderName(String arn) {
        String[] tokens = arn.split("arn:seagate:iam::");
        tokens = tokens[1].split(":");
        return tokens[1];
    }
}
