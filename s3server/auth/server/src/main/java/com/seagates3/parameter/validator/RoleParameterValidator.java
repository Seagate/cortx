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
 * Original creation date: 31-Oct-2015
 */
package com.seagates3.parameter.validator;

import java.util.Map;

/**
 * Validate the input for role APIs - Create, Delete and List.
 */
public class RoleParameterValidator extends AbstractParameterValidator {

    /**
     * Validate the input parameters for isValidCreateParams role request. Role name is
     * required. AssumeRolePolicyDocument is required. Path is optional.
     *
     * @param requestBody TreeMap of input parameters.
     * @return true if input is valid.
     */
    @Override
    public Boolean isValidCreateParams(Map<String, String> requestBody) {
        if (requestBody.containsKey("Path")) {
            if (!S3ParameterValidatorUtil.isValidPath(
                    requestBody.get("Path"))) {
                return false;
            }
        }

        if (!S3ParameterValidatorUtil.isValidName(requestBody.get("RoleName"))) {
            return false;
        }

        return S3ParameterValidatorUtil.isValidAssumeRolePolicyDocument(
                requestBody.get("AssumeRolePolicyDocument"));
    }

    /**
     * Validate the input parameters for isValidDeleteParams role request. Role name is
     * required.
     *
     * @param requestBody TreeMap of input parameters.
     * @return true if input is valid.
     */
    @Override
    public Boolean isValidDeleteParams(Map<String, String> requestBody) {
        return S3ParameterValidatorUtil.isValidName(requestBody.get("RoleName"));
    }
}
