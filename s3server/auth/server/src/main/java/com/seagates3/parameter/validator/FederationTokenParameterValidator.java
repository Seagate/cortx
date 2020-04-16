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
 * Validate the input for Get Federation Token API.
 */
public class FederationTokenParameterValidator extends AbstractParameterValidator {

    /**
     * Validate the input parameters for get federation token request.
     * Name is required.
     * Duration seconds is optional.
     * Policy is optional.
     *
     * @param requestBody TreeMap of input parameters.
     * @return true if input is valid.
     */
    @Override
    public Boolean isValidCreateParams(Map<String, String> requestBody) {
        if (requestBody.containsKey("DurationSeconds")) {
            if (!STSParameterValidatorUtil.isValidDurationSeconds(requestBody.get("DurationSeconds"))) {
                return false;
            }
        }

        if (requestBody.containsKey("Policy")) {
            if (!STSParameterValidatorUtil.isValidPolicy(requestBody.get("Policy"))) {
                return false;
            }
        }

        return STSParameterValidatorUtil.isValidName(requestBody.get("Name"));
    }
}
