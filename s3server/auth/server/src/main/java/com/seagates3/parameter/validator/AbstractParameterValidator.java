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
 * Abstract class for Validator classes.
 */
public abstract class AbstractParameterValidator {

    /**
     * Abstract implementation to check create request parameters.
     *
     * @param requestBody TreeMap of input parameters.
     * @return true
     */
    public Boolean isValidCreateParams(Map<String, String> requestBody) {
        return true;
    }

    /**
     * Abstract implementation to check delete request parameters.
     *
     * @param requestBody TreeMap of input parameters.
     * @return true
     */
    public Boolean isValidDeleteParams(Map<String, String> requestBody) {
        return true;
    }

    /**
     * Abstract implementation to check list request parameters. Validate the
     * input parameters for isValidListParams requests.
     *
     * List operation is used only on IAM resources i.e roles, user etc. Hence
     * default implementation is specific to IAM APIs.
     *
     * Path prefix is optional. Max Items is optional. Marker is optional.
     *
     * @param requestBody TreeMap of input parameters.
     * @return true if input is valid.
     */
    public Boolean isValidListParams(Map<String, String> requestBody) {
        if (requestBody.containsKey("PathPrefix")) {
            if (!S3ParameterValidatorUtil.isValidPathPrefix(
                    requestBody.get("PathPrefix"))) {
                return false;
            }
        }

        if (requestBody.containsKey("MaxItems")) {
            if (!S3ParameterValidatorUtil.isValidMaxItems(
                    requestBody.get("MaxItems"))) {
                return false;
            }
        }

        if (requestBody.containsKey("Marker")) {
            return S3ParameterValidatorUtil.isValidMarker(
                    requestBody.get("Marker"));
        }

        return true;
    }

    /**
     * Abstract implementation to check update request parameters.
     *
     * @param requestBody TreeMap of input parameters.
     * @return true
     */
    public Boolean isValidUpdateParams(Map<String, String> requestBody) {
        return true;
    }

    /**
     * Abstract implementation to check changePassword request parameters.
     *
     * @param requestBody TreeMap of input parameters.
     * @return true
     */
   public
    Boolean isValidChangepasswordParams(Map<String, String> requestBody) {
      return true;
    }
}
