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
 * Original creation date: 17-Sep-2015
 */

package com.seagates3.parameter.validator;

import java.util.Map;

/**
 * Validate the input for User APIs - Create, Delete, List and isValidUpdateParams.
 */
public class UserParameterValidator extends AbstractParameterValidator {

    /**
     * Validate the input parameters for isValidCreateParams user request.
     * User name is required.
     * Path is optional.
     *
     * @param requestBody TreeMap of input parameters.
     * @return true if input is valid.
     */
    @Override
    public Boolean isValidCreateParams(Map<String, String> requestBody) {
        if (requestBody.containsKey("Path")) {
            return S3ParameterValidatorUtil.isValidPath(requestBody.get("Path"));
        }

        return S3ParameterValidatorUtil.isValidName(requestBody.get("UserName"));
    }

    /**
     * Validate the input parameters for isValidDeleteParams user request.
     * User name is required.
     *
     * @param requestBody TreeMap of input parameters.
     * @return true if input is valid.
     */
    @Override
    public Boolean isValidDeleteParams(Map<String, String> requestBody) {
        return S3ParameterValidatorUtil.isValidName(requestBody.get("UserName"));
    }

    /**
     * Validate the input parameters for isValidUpdateParams user request.
     * User name is required.
     * New User name is optional.
     * New Path is optional.
     *
     * @param requestBody TreeMap of input parameters.
     * @return true if input is valid.
     */
    @Override
    public Boolean isValidUpdateParams(Map<String, String> requestBody) {
        if (requestBody.containsKey("NewUserName")) {
            if (!S3ParameterValidatorUtil.isValidName(requestBody.get("NewUserName"))) {
                return false;
            }
        }

        if (requestBody.containsKey("NewPath")) {
            if (!S3ParameterValidatorUtil.isValidPath(requestBody.get("NewPath"))) {
                return false;
            }
        }

        return S3ParameterValidatorUtil.isValidName(requestBody.get("UserName"));
    }
}
