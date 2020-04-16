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
package com.seagates3.parameter.validator;

import java.util.Map;

/**
 * Validate the input for Assume Role with SAML API.
 */
public class AssumeRoleWithSAMLParameterValidator extends AbstractParameterValidator {

    /**
     * Validate the input parameters for assume role with SAML API. PrincipalArn
     * is required. RoleArn is required. SAMLAssertion is required.
     * DurationSeconds is optional. Policy is optional.
     *
     * @param requestBody TreeMap of input parameters.
     * @return true if input is valid.
     */
    @Override
    public Boolean isValidCreateParams(Map<String, String> requestBody) {
        if (requestBody.containsKey("DurationSeconds")) {
            if (!STSParameterValidatorUtil.isValidAssumeRoleDuration(
                    requestBody.get("DurationSeconds"))) {
                return false;
            }
        }

        if (requestBody.containsKey("Policy")) {
            if (!STSParameterValidatorUtil.isValidPolicy(requestBody.get("Policy"))) {
                return false;
            }
        }

        if (!S3ParameterValidatorUtil.isValidARN(requestBody.get("PrincipalArn"))) {
            return false;
        }

        if (!S3ParameterValidatorUtil.isValidARN(requestBody.get("RoleArn"))) {
            return false;
        }

        return STSParameterValidatorUtil.isValidSAMLAssertion(
                requestBody.get("SAMLAssertion"));
    }
}
