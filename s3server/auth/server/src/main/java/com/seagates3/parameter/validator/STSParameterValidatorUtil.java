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
 * Original creation date: 16-Nov-2015
 */

package com.seagates3.parameter.validator;

import com.seagates3.util.BinaryUtil;

/**
 * Util class containing common validation rules for STS APIs.
 */
public class STSParameterValidatorUtil {

    final static int MAX_ASSUME_ROLE_DURATION = 3600;
    final static int MAX_DURATION = 129600;
    final static int MAX_NAME_LENGTH = 32;
    final static int MAX_POLICY_LENGTH = 2048;
    final static int MAX_SAML_ASSERTION_LENGTH = 50000;
    final static int MIN_DURATION = 900;
    final static int MIN_NAME_LENGTH = 2;
    final static int MIN_SAML_ASSERTION_LENGTH = 4;

    final static String NAME_PATTERN = "[\\w+=,.@-]*";
    final static String POLICY_PATTERN = "[\\u0009\\u000A\\u000D\\u0020-\\u00FF]+";

    /**
     * Validate the name.
     * Length of the name should be between 2 and 32 characters.
     * It should match the patten "[\\w+=,.@-]*".
     *
     * @param name user name to be validated.
     * @return true if name is valid.
     */
    public static Boolean isValidName(String name) {
        if(name == null) {
            return false;
        }

        if(!name.matches(NAME_PATTERN)) {
            return false;
        }

        return !(name.length() < MIN_NAME_LENGTH ||
                name.length() > MAX_NAME_LENGTH);
    }

    /**
     * Validate the duration seconds.
     * Range - 900 to 129600
     *
     * @param duration user name to be validated.
     * @return true if name is valid.
     */
    public static Boolean isValidDurationSeconds(String duration) {
        int durationSeconds;
        try {
            durationSeconds = Integer.parseInt(duration);
        } catch(NumberFormatException | AssertionError ex) {
            return false;
        }
        return !(durationSeconds > MAX_DURATION || durationSeconds < MIN_DURATION);
    }

    /**
     * Validate the user policy.
     * Length of the name should be between 1 and 2048 characters.
     * It should match the patten "[\\u0009\\u000A\\u000D\\u0020-\\u00FF]+".
     *
     * @param policy policy to be validated.
     * @return true if name is valid.
     */
    public static Boolean isValidPolicy(String policy) {
        if(policy == null) {
            return false;
        }

        if(!policy.matches(POLICY_PATTERN)) {
            return false;
        }

        return !(policy.length() < 1 || policy.length() > MAX_POLICY_LENGTH);
    }

    /**
     * Validate the duration for assume role with SAML.
     * Range - 900 to 3600
     *
     * @param duration duration to be validated.
     * @return true if name is valid.
     */
    public static Boolean isValidAssumeRoleDuration(String duration) {
        int durationSeconds;
        try {
            durationSeconds = Integer.parseInt(duration);
        } catch(NumberFormatException | AssertionError ex) {
            return false;
        }

        return !(durationSeconds < MIN_DURATION ||
                durationSeconds > MAX_ASSUME_ROLE_DURATION);
    }

    /**
     * Validate the SAML assertion (Assume role with SAML API).
     * Assertion should be base 64 encoded.
     * Length of the assertion should be between 4 and 50000 characters.
     *
     * @param assertion SAML assertion.
     * @return true if name is valid.
     */
    public static Boolean isValidSAMLAssertion(String assertion) {
        if(assertion == null) {
            return false;
        }

        if(!BinaryUtil.isBase64Encoded(assertion)) {
            return false;
        }

        return !(assertion.length() < MIN_SAML_ASSERTION_LENGTH ||
                assertion.length() > MAX_SAML_ASSERTION_LENGTH);
    }
}
