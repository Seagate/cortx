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
 * Original creation date: 14-Oct-2015
 */
package com.seagates3.parameter.validator;

import java.util.Map;

/**
 * Validate the input for SAML provider APIs - Create, Delete and isValidUpdateParams.
 */
public class SAMLProviderParameterValidator extends AbstractParameterValidator {

    /**
     * Validate the input parameters for isValidCreateParams SAML Provider request. SAML
     * provider name is required. SAML metadata is required.
     *
     * @param requestBody TreeMap of input parameters.
     * @return true if input is valid.
     */
    @Override
    public Boolean isValidCreateParams(Map<String, String> requestBody) {
        if (!S3ParameterValidatorUtil.isValidSamlProviderName(requestBody.get("Name"))) {
            return false;
        }

        return S3ParameterValidatorUtil.isValidSAMLMetadata(
                requestBody.get("SAMLMetadataDocument"));
    }

    /**
     * Validate the input parameters for isValidDeleteParams user request. SAML Provider ARN
     * is required.
     *
     * @param requestBody TreeMap of input parameters.
     * @return true if input is valid.
     */
    @Override
    public Boolean isValidDeleteParams(Map<String, String> requestBody) {
        return S3ParameterValidatorUtil.isValidARN(requestBody.get("SAMLProviderArn"));
    }

    /**
     * Validate the input parameters for isValidListParams SAML providers request. This API
     * doesn't require an input.
     *
     * @param requestBody TreeMap of input parameters.
     * @return true.
     */
    @Override
    public Boolean isValidListParams(Map<String, String> requestBody) {
        return true;
    }

    /**
     * Validate the input parameters for isValidUpdateParams SAML provider request. SAML
     * provider ARN is required. SAML metadata is required.
     *
     * @param requestBody TreeMap of input parameters.
     * @return true.
     */
    @Override
    public Boolean isValidUpdateParams(Map<String, String> requestBody) {
        if (!S3ParameterValidatorUtil.isValidARN(requestBody.get("SAMLProviderArn"))) {
            return false;
        }

        return S3ParameterValidatorUtil.isValidSAMLMetadata(requestBody.get("SAMLMetadataDocument"));
    }
}
