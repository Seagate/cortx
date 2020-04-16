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

import java.util.regex.Matcher;
import java.util.regex.Pattern;
import org.apache.commons.validator.routines.EmailValidator;

/**
 * Util class containing common validation rules for S3 APIs.
 */
public class S3ParameterValidatorUtil {

    final static int MAX_ARN_LENGTH = 2048;
    final static int MAX_ACCESS_KEY_ID_LENGTH = 32;
    final static int MAX_ACCESS_KEY_USER_NAME_LENGTH = 128;
    final static int MAX_ASSUME_ROLE_POLICY_DOC_LENGTH = 2048;
    final static int MAX_DESCRIPTION_LENGTH = 1000;
    final static int MAX_GROUP_NAME_LENGTH = 128;
    final static int MAX_ITEMS = 1000;
    final static int MAX_POLICY_DOC_LENGTH = 5120;
    final static int MAX_POLICY_NAME_LENGTH = 128;
    final static int MAX_MARKER_LENGTH = 320;
    final static int MAX_NAME_LENGTH = 64;
    final static int MAX_PATH_LENGTH = 512;
    final static int MAX_SAML_METADATA_LENGTH = 10000000;
    final static int MAX_SAML_PROVIDER_NAME_LENGTH = 128;
    final static int MIN_ACCESS_KEY_ID_LENGTH = 16;
    final static int MIN_ARN_LENGTH = 20;
    final static int MIN_SAML_METADATA_LENGTH = 1000;
    final static int MIN_PASSWORD_LENGTH = 6;
    final static int MAX_PASSWORD_LENGTH = 128;

    /**
     * TODO Fix ACCESS_KEY_ID_PATTERN. It should be "[\\w]+"
     */
    final static String ACCESS_KEY_ID_PATTERN = "[\\w-]+";
    final static String ASSUME_ROLE_POLICY_DOC_PATTERN
            = "[\\u0009\\u000A\\u000D\\u0020-\\u00FF]+";
    final static String POLICY_DOC_PATTERN
            = "[\\u0009\\u000A\\u000D\\u0020-\\u00FF]+";
    final static String MARKER_PATTERN = "[\\u0020-\\u00FF]+";
    final static String NAME_PATTERN = "[\\w+=,.@-]+";
    final static String PATH_PATTERN
            = "(\\u002F)|(\\u002F[\\u0021-\\u007F]+\\u002F)";
    final static String PATH_PREFIX_PATTERN = "\\u002F[\\u0021-\\u007F]*";
    final static String SAML_PROVIDER_NAME_PATTERN = "[\\w._-]+";
    final static String PASSWORD_PATTERN =
        "[\\u0009\\u000A\\u000D\\u0020-\\u00FF]+";

    /**
     * Validate the name (user name, role name etc). Length of the name should
     * be between 1 and 64 characters. It should match the patten
     * "[\\w+=,.@-]+".
     *
     * @param name name to be validated.
     * @return true if name is valid.
     */
    public static Boolean isValidName(String name) {
        if (name == null) {
            return false;
        }

        if (!name.matches(NAME_PATTERN)) {
            return false;
        }

        return !(name.length() < 1 || name.length() > MAX_NAME_LENGTH);
    }

    /**
     * Validate the path. Length of the name should be between 1 and 512
     * characters. It should match the patten
     * "(\\u002F)|(\\u002F[\\u0021-\\u007F]+\\u002F)".
     *
     * @param path path to be validated.
     * @return true if path is valid.
     */
    public static Boolean isValidPath(String path) {
        if (!path.matches(PATH_PATTERN)) {
            return false;
        }

        return !(path.length() < 1 || path.length() > MAX_PATH_LENGTH);
    }

   public
    static Boolean isValidPassword(String password) {
      if (!password.matches(PASSWORD_PATTERN)) {
        return false;
      }

      return !(password.length() < 1 ||
               password.length() > MAX_PASSWORD_LENGTH);
    }

    /**
     * Validate the password as per password policy. Allowed range
     * is between 6 and 128.
     *
     * @param password user password.
     * @return true if password follows password policy.
     */
   public
    static Boolean validatePasswordPolicy(String password) {
      if (!password.matches(PASSWORD_PATTERN)) {
        return false;
      }

      return !(password.length() < MIN_PASSWORD_LENGTH ||
               password.length() > MAX_PASSWORD_LENGTH);
    }

    /**
     * Validate the max items requested while listing resources. Allowed range
     * is between 1 and 1000.
     *
     * @param maxItems max items requested in a page.
     * @return true if maxItems is valid.
     */
    public static Boolean isValidMaxItems(String maxItems) {
        int maxItemsInt;
        try {
            maxItemsInt = Integer.parseInt(maxItems);
        } catch (NumberFormatException | AssertionError ex) {
            return false;
        }

        return !(maxItemsInt < 1 || maxItemsInt > MAX_ITEMS);
    }

    /**
     * Validate the path prefix. Length of the name should be between 1 and 512
     * characters. It should match the patten "\\u002F[\\u0021-\\u007F]*".
     *
     * @param pathPrefix path to be validated.
     * @return true if pathPrefix is valid.
     */
    public static Boolean isValidPathPrefix(String pathPrefix) {
        if (!pathPrefix.matches(PATH_PREFIX_PATTERN)) {
            return false;
        }

        return !(pathPrefix.length() < 1
                || pathPrefix.length() > MAX_PATH_LENGTH);
    }

    /**
     * Validate the marker. Length of the marker should be between 1 and 320
     * characters. It should match the patten "[\\u0020-\\u00FF]+".
     *
     * @param marker marker to be validated.
     * @return true if the marker is valid.
     */
    public static Boolean isValidMarker(String marker) {
        if (!marker.matches(MARKER_PATTERN)) {
            return false;
        }

        return !(marker.length() < 1 || marker.length() > MAX_MARKER_LENGTH);
    }

    /**
     * Validate the access key user name (user name, role name etc). Length of
     * the name should be between 1 and 128 characters. It should match the
     * patten "[\\w+=,.@-]+".
     *
     * @param name name to be validated.
     * @return true if name is valid.
     */
    public static Boolean isValidAccessKeyUserName(String name) {
        if (name == null) {
            return false;
        }

        if (!name.matches(NAME_PATTERN)) {
            return false;
        }

        return !(name.length() < 1
                || name.length() > MAX_ACCESS_KEY_USER_NAME_LENGTH);
    }

    /**
     * Validate the access key id. Length of the name should be between 16 and
     * 32 characters. It should match the patten "[\\w]+".
     *
     * @param accessKeyId access key id to be validated.
     * @return true if access key id is valid.
     */
    public static Boolean isValidAccessKeyId(String accessKeyId) {
        if (accessKeyId == null) {
            return false;
        }

        if (!accessKeyId.matches(ACCESS_KEY_ID_PATTERN)) {
            return false;
        }

        return !(accessKeyId.length() < MIN_ACCESS_KEY_ID_LENGTH
                || accessKeyId.length() > MAX_ACCESS_KEY_ID_LENGTH);
    }

    /**
     * Validate the access key status. Accepted values for status are 'Active'
     * or 'Inactive'
     *
     * @param status access key status to be validated.
     * @return true if access key id is valid.
     */
    public static Boolean isValidAccessKeyStatus(String status) {
        if (status == null) {
            return false;
        }

        return (status.equals("Active") || status.equals("Inactive"));
    }

    /**
     * Validate assume role policy document. Length of the document should be
     * between 1 and 2048 characters. It should match the patten
     * "[\\u0009\\u000A\\u000D\\u0020-\\u00FF]+".
     *
     * @param policyDoc access key id to be validated.
     * @return true if access key id is valid.
     */
    public static Boolean isValidAssumeRolePolicyDocument(String policyDoc) {
        if (policyDoc == null) {
            return false;
        }

        if (!policyDoc.matches(ASSUME_ROLE_POLICY_DOC_PATTERN)) {
            return false;
        }

        return !(policyDoc.length() < 1
                || policyDoc.length() > MAX_ASSUME_ROLE_POLICY_DOC_LENGTH);
    }

    /**
     * Validate the SAMl provider name (user name, role name etc). Length of the
     * name should be between 1 and 128 characters. It should match the patten
     * "[\\w._-]+".
     *
     * @param name name to be validated.
     * @return true if name is valid.
     */
    public static Boolean isValidSamlProviderName(String name) {
        if (name == null) {
            return false;
        }

        if (!name.matches(SAML_PROVIDER_NAME_PATTERN)) {
            return false;
        }

        return !(name.length() < 1
                || name.length() > MAX_SAML_PROVIDER_NAME_LENGTH);
    }

    /**
     * Validate the SAMl metadata (user name, role name etc). Length of the
     * metadata should be between 1000 and 10000000 characters.
     *
     * @param samlMetadataDocument SAML metadata.
     * @return true if name is valid.
     */
    public static Boolean isValidSAMLMetadata(String samlMetadataDocument) {
        if (samlMetadataDocument == null) {
            return false;
        }

        return !(samlMetadataDocument.length() < MIN_SAML_METADATA_LENGTH
                || samlMetadataDocument.length() > MAX_SAML_METADATA_LENGTH);
    }

    /**
     * Validate the ARN (role ARN, principal ARN etc). Length of the name should
     * be between 20 and 2048 characters.
     *
     * @param arn ARN.
     * @return true if name is valid.
     */
    public static Boolean isValidARN(String arn) {
        if (arn == null) {
            return false;
        }

        String arn_regex_pattern = "arn:seagate:iam:[\\w-/]*:[\\w-/]*:[\\w-/]*";
        Pattern pattern = Pattern.compile(arn_regex_pattern);
        Matcher match = pattern.matcher(arn);
        if (!match.matches()) {
            return false;
        }

        return !(arn.length() < MIN_ARN_LENGTH
                || arn.length() > MAX_ARN_LENGTH);
    }

    /**
     * Validate the email address.
     *
     * @param email
     * @return True if valid email.
     */
    public static Boolean isValidEmail(String email) {
        EmailValidator ev = EmailValidator.getInstance();
        return ev.isValid(email);
    }

    /**
     * Validate the description of an entity. Description length should be
     * between 0 and 1000 characters.
     *
     * @param description
     * @return True if description is valid.
     */
    public static Boolean isValidDescription(String description) {
        if (description == null) {
            return false;
        }

        return !(description.length() < 1
                || description.length() > MAX_DESCRIPTION_LENGTH);
    }

    /**
     * Validate policy document. Length of the document should be between 1 and
     * 5120 characters. It should match the patten
     * "[\\u0009\\u000A\\u000D\\u0020-\\u00FF]+".
     *
     * @param policyDoc access key id to be validated.
     * @return true policy doc is valid.
     */
    public static Boolean isValidPolicyDocument(String policyDoc) {
        if (policyDoc == null) {
            return false;
        }

        if (!policyDoc.matches(POLICY_DOC_PATTERN)) {
            return false;
        }

        return !(policyDoc.length() < 1
                || policyDoc.length() > MAX_POLICY_DOC_LENGTH);
    }

    /**
     * Validate the policy name. Length of the name should be between 1 and 128
     * characters. It should match the patten "[\\w+=,.@-]+".
     *
     * @param policyName name to be validated.
     * @return true if name is valid.
     */
    public static Boolean isValidPolicyName(String policyName) {
        if (policyName == null) {
            return false;
        }

        if (!policyName.matches(NAME_PATTERN)) {
            return false;
        }

        return !(policyName.length() < 1
                || policyName.length() > MAX_POLICY_NAME_LENGTH);
    }

    /**
     * Validate the group name. Length of the name should be between 1 and 128
     * characters. It should match the patten "[\\w+=,.@-]+".
     *
     * @param groupName name to be validated.
     * @return true if name is valid.
     */
    public static Boolean isValidGroupName(String groupName) {
        if (groupName == null) {
            return false;
        }

        if (!groupName.matches(NAME_PATTERN)) {
            return false;
        }

        return !(groupName.length() < 1
                || groupName.length() > MAX_GROUP_NAME_LENGTH);
    }
}
