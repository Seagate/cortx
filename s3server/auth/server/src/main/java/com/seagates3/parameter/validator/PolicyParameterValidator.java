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
 * Original creation date: 19-May-2016
 */
package com.seagates3.parameter.validator;

import java.util.Map;

/**
 * Validate the input for managed policy APIs - Create, Delete, List and Update.
 */
public class PolicyParameterValidator extends AbstractParameterValidator {

    /**
     * Validate the input parameters for isValidCreateParams policy request. Policy name is
     * required. Policy document is required. Path and description are optional.
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

        if (requestBody.containsKey("Description")) {
            if (!S3ParameterValidatorUtil.isValidDescription(
                    requestBody.get("Description"))) {
                return false;
            }
        }

        if (!S3ParameterValidatorUtil.isValidPolicyName(
                requestBody.get("PolicyName"))) {
            return false;
        }

        return S3ParameterValidatorUtil.isValidPolicyDocument(
                requestBody.get("PolicyDocument"));
    }
}
