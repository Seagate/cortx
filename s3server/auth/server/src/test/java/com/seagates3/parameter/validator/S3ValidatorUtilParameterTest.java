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
 * Original creation date: 12-Nov-2015
 */
package com.seagates3.parameter.validator;

import com.seagates3.parameter.validator.S3ParameterValidatorUtil;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import org.junit.Test;

public class S3ValidatorUtilParameterTest {

    /**
     * Test S3ValidatorUtil#isValidName. case - Valid name.
     */
    @Test
    public void IsValidName_ValidName_True() {
        String name = "test.123@seagate.com+=,-";
        assertTrue(S3ParameterValidatorUtil.isValidName(name));
    }

    /**
     * Test S3ValidatorUtil#isValidName. case - Name equals null
     */
    @Test
    public void IsValidName_NameNull_False() {
        String name = null;
        assertFalse(S3ParameterValidatorUtil.isValidName(name));
    }

    /**
     * Test S3ValidatorUtil#isValidName. case - Invalid name pattern.
     */
    @Test
    public void IsValidName_InvalidNamePattern_False() {
        String name = "123*^";
        assertFalse(S3ParameterValidatorUtil.isValidName(name));
    }

    /**
     * Test S3ValidatorUtil#isValidName. case - Length of the name is out of
     * range.
     */
    @Test
    public void IsValidName_NameLengthOutOfRange_False() {
        String name = new String(new char[65]).replace('\0', 'a');
        assertFalse(S3ParameterValidatorUtil.isValidName(name));

        name = "";
        assertFalse(S3ParameterValidatorUtil.isValidName(name));
    }

    /**
     * Test S3ValidatorUtil#isValidPath. case - Valid path.
     */
    @Test
    public void IsValidPath_ValidPath_True() {
        String path = "/seagate123@/test/";
        assertTrue(S3ParameterValidatorUtil.isValidPath(path));

        path = "/";
        assertTrue(S3ParameterValidatorUtil.isValidPath(path));
    }

    /**
     * Test S3ValidatorUtil#isValidPath. case - Path Length is out of range.
     */
    @Test
    public void IsValidPath_PathLengthOutOfRange_False() {
        String str = new String(new char[600]).replace('\0', '/');
        String path = String.format("/%s/", str);
        assertFalse(S3ParameterValidatorUtil.isValidName(path));
    }

    /**
     * Test S3ValidatorUtil#isValidPath. case - Invalid Path pattern.
     */
    @Test
    public void IsValidPath_InvalidPathPattern_False() {
        String path = "/seagate/test";
        assertFalse(S3ParameterValidatorUtil.isValidPath(path));

        path = "seagate/test/";
        assertFalse(S3ParameterValidatorUtil.isValidPath(path));

        path = "";
        assertFalse(S3ParameterValidatorUtil.isValidPath(path));
    }

    /**
     * Test S3ValidatorUtil#isValidMaxItems. case - Max items is in the range.
     */
    @Test
    public void IsValidMaxItems_ValidMaxItems_True() {
        String maxitems = "100";
        assertTrue(S3ParameterValidatorUtil.isValidMaxItems(maxitems));
    }

    /**
     * Test S3ValidatorUtil#isValidMaxItems. case - Max items is outside the
     * range.
     */
    @Test
    public void IsValidMaxItems_MaxItemsOutOfRange_False() {
        String maxitems = "10000";
        assertFalse(S3ParameterValidatorUtil.isValidMaxItems(maxitems));

        maxitems = "0";
        assertFalse(S3ParameterValidatorUtil.isValidMaxItems(maxitems));
    }

    /**
     * Test S3ValidatorUtil#isValidMaxItems. case - Max items is not an integer.
     */
    @Test
    public void IsValidMaxItems_InvalidMaxItems_False() {
        String maxitems = "abc";
        assertFalse(S3ParameterValidatorUtil.isValidMaxItems(maxitems));

        maxitems = null;
        assertFalse(S3ParameterValidatorUtil.isValidMaxItems(maxitems));
    }

    /**
     * Test S3ValidatorUtil#isValidPathPrefix. case - Valid path prefix.
     */
    @Test
    public void IsValidPathPrefix_ValidPathPrefix_True() {
        String path = "/seagate123@/test";
        assertTrue(S3ParameterValidatorUtil.isValidPathPrefix(path));

        path = "/";
        assertTrue(S3ParameterValidatorUtil.isValidPathPrefix(path));
    }

    /**
     * Test S3ValidatorUtil#isValidPathPrefix. case - Path prefix Length is out
     * of range.
     */
    @Test
    public void IsValidPathPrefix_PathLengthOutOfRange_False() {
        String str = new String(new char[600]).replace('\0', '/');
        String path = String.format("/%s/", str);
        assertFalse(S3ParameterValidatorUtil.isValidPathPrefix(path));
    }

    /**
     * Test S3ValidatorUtil#isValidPath. case - Invalid path prefix pattern.
     */
    @Test
    public void IsValidPathPrefix_InvalidPathPrefixPattern_False() {
        String path = "seagate/";
        assertFalse(S3ParameterValidatorUtil.isValidPathPrefix(path));

        path = "";
        assertFalse(S3ParameterValidatorUtil.isValidPathPrefix(path));
    }

    /**
     * Test S3ValidatorUtil#isValidMarker. case - Valid marker.
     */
    @Test
    public void IsValidMarker_ValidMarker_True() {
        String marker = "test";
        assertTrue(S3ParameterValidatorUtil.isValidMarker(marker));
    }

    /**
     * Test S3ValidatorUtil#isValidMarker. case - Marker Length is out of range.
     */
    @Test
    public void IsValidMarker_MarkerLengthOutOfRange_False() {
        String marker = new String(new char[350]).replace('\0', 'a');
        assertFalse(S3ParameterValidatorUtil.isValidMarker(marker));
    }

    /**
     * Test S3ValidatorUtil#isValidMarker. case - Invalid Marker pattern.
     */
    @Test
    public void IsValidMarker_InvalidMarkerPattern_False() {
        char c = "\u0100".toCharArray()[0];
        String marker = String.valueOf(c);
        assertFalse(S3ParameterValidatorUtil.isValidMarker(marker));
    }

    /**
     * Test S3ValidatorUtil#isValidAccessKeyUserName. case - Valid name.
     */
    @Test
    public void IsValidAccessKeyUserName_ValidName_True() {
        String name = "test.123@seagate.com+=,-";
        assertTrue(S3ParameterValidatorUtil.isValidName(name));
    }

    /**
     * Test S3ValidatorUtil#isValidAccessKeyUserName. case - Name equals null
     */
    @Test
    public void IsValidAccessKeyUserName_NameNull_False() {
        String name = null;
        assertFalse(S3ParameterValidatorUtil.isValidName(name));
    }

    /**
     * Test S3ValidatorUtil#isValidAccessKeyUserName. case - Invalid name
     * pattern.
     */
    @Test
    public void IsValidAccessKeyUserName_InvalidNamePattern_False() {
        String name = "123*^";
        assertFalse(S3ParameterValidatorUtil.isValidName(name));
    }

    /**
     * Test S3ValidatorUtil#isValidAccessKeyUserName. case - Length of the name
     * is out of range.
     */
    @Test
    public void IsValidAccessKeyUserName_NameLengthOutOfRange_False() {
        String name = new String(new char[130]).replace('\0', 'a');
        assertFalse(S3ParameterValidatorUtil.isValidName(name));

        name = "";
        assertFalse(S3ParameterValidatorUtil.isValidName(name));
    }

    /**
     * Test S3ValidatorUtil#isValidAccessKeyId. case - Valid access key id.
     */
    @Test
    public void IsValidAccessKeyId_ValidAccessKeyId_True() {
        String accessKeyId = "ABCDEFGHIJKLMN123456";
        assertTrue(S3ParameterValidatorUtil.isValidAccessKeyId(accessKeyId));
    }

    /**
     * Test S3ValidatorUtil#isValidAccessKeyId. case - Access key id is null
     */
    @Test
    public void IsValidAccessKeyId_NameNull_False() {
        String accessKeyId = null;
        assertFalse(S3ParameterValidatorUtil.isValidAccessKeyId(accessKeyId));
    }

    /**
     * Test S3ValidatorUtil#isValidAccessKeyId. case - Invalid access key id
     * pattern.
     */
    @Test
    public void IsValidAccessKeyId_InvalidNamePattern_False() {
        String accessKeyId = "ABCDEFGHIJKLMN 123456";
        assertFalse(S3ParameterValidatorUtil.isValidAccessKeyId(accessKeyId));
    }

    /**
     * Test S3ValidatorUtil#isValidAccessKeyId. case - Length of the access key
     * id is out of range.
     */
    @Test
    public void IsValidAccessKeyId_NameLengthOutOfRange_False() {
        String accessKeyId = new String(new char[40]).replace('\0', 'A');
        assertFalse(S3ParameterValidatorUtil.isValidAccessKeyId(accessKeyId));

        accessKeyId = "ABCDE";
        assertFalse(S3ParameterValidatorUtil.isValidAccessKeyId(accessKeyId));
    }

    /**
     * Test S3ValidatorUtil#isValidAccessKeyStatus. case - Valid access key id.
     */
    @Test
    public void IsValidAccessKeyStatus_ValidAccessKeyStatus_True() {
        String status = "Active";
        assertTrue(S3ParameterValidatorUtil.isValidAccessKeyStatus(status));

        status = "Inactive";
        assertTrue(S3ParameterValidatorUtil.isValidAccessKeyStatus(status));
    }

    /**
     * Test S3ValidatorUtil#isValidAccessKeyStatus. case - Access key id is null
     */
    @Test
    public void IsValidAccessKeyStatus_AccessKeyStatusNull_False() {
        String status = null;
        assertFalse(S3ParameterValidatorUtil.isValidAccessKeyStatus(status));
    }

    /**
     * Test S3ValidatorUtil#isValidAccessKeyStatus. case - Invalid access key id
     * pattern.
     */
    @Test
    public void IsValidAccessKeyStatus_InvalidAccessKeyStatus_False() {
        String status = "active";
        assertFalse(S3ParameterValidatorUtil.isValidAccessKeyStatus(status));
    }

    /**
     * Test S3ValidatorUtil#isValidAssumeRolePolicyDocument. case - Valid policy
     * document.
     */
    @Test
    public void IsValidAssumeRolePolicyDoc_ValidName_True() {
        String policyDoc = "{\n"
                + "  \"Version\": \"2012-10-17\",\n"
                + "  \"Statement\": {\n"
                + "    \"Effect\": \"Allow\",\n"
                + "    \"Principal\": {\"Service\": \"test\"},\n"
                + "    \"Action\": \"sts:AssumeRole\"\n"
                + "  }\n"
                + "}";
        assertTrue(S3ParameterValidatorUtil.isValidAssumeRolePolicyDocument(policyDoc));
    }

    /**
     * Test S3ValidatorUtil#isValidAssumeRolePolicyDocument. case - policy is
     * null.
     */
    @Test
    public void IsValidAssumeRolePolicyDoc_NameNull_False() {
        String policyDoc = null;
        assertFalse(S3ParameterValidatorUtil.isValidAssumeRolePolicyDocument(policyDoc));
    }

    /**
     * Test S3ValidatorUtil#isValidAssumeRolePolicyDocument. case - Policy doc
     * has invalid characters.
     */
    @Test
    public void IsValidAssumeRolePolicyDoc_InvalidNamePattern_False() {
        char c = "\u0100".toCharArray()[0];
        String policyDoc = String.valueOf(c);
        assertFalse(S3ParameterValidatorUtil.isValidAssumeRolePolicyDocument(policyDoc));
    }

    /**
     * Test S3ValidatorUtil#isValidAssumeRolePolicyDocument. case - Length of
     * policy document is out of range.
     */
    @Test
    public void IsValidAssumeRolePolicyDoc_NameLengthOutOfRange_False() {
        String policyDoc = new String(new char[2050]).replace('\0', 'a');
        assertFalse(S3ParameterValidatorUtil.isValidAssumeRolePolicyDocument(policyDoc));

        policyDoc = "";
        assertFalse(S3ParameterValidatorUtil.isValidAssumeRolePolicyDocument(policyDoc));
    }

    /**
     * Test S3ValidatorUtil#isValidSamlProviderName. case - Valid name.
     */
    @Test
    public void IsValidSamlProviderName_ValidName_True() {
        String name = "test123._-";
        assertTrue(S3ParameterValidatorUtil.isValidSamlProviderName(name));
    }

    /**
     * Test S3ValidatorUtil#isValidSamlProviderName. case - Name equals null
     */
    @Test
    public void IsValidSamlProviderName_NameNull_False() {
        String name = null;
        assertFalse(S3ParameterValidatorUtil.isValidSamlProviderName(name));
    }

    /**
     * Test S3ValidatorUtil#isValidSamlProviderName. case - Invalid name
     * pattern.
     */
    @Test
    public void IsValidSamlProviderName_InvalidNamePattern_False() {
        String name = "root*^";
        assertFalse(S3ParameterValidatorUtil.isValidSamlProviderName(name));
    }

    /**
     * Test S3ValidatorUtil#isValidSamlProviderName. case - Length of the name
     * is out of range.
     */
    @Test
    public void IsValidSamlProviderName_NameLengthOutOfRange_False() {
        String name = new String(new char[130]).replace('\0', 'a');
        assertFalse(S3ParameterValidatorUtil.isValidSamlProviderName(name));

        name = "";
        assertFalse(S3ParameterValidatorUtil.isValidSamlProviderName(name));
    }

    /**
     * Test S3ValidatorUtil#isValidSAMLMetadata. case - Valid SAML metadata.
     */
    @Test
    public void IsValidSamlMetadata_ValidName_True() {
        String metadata = new String(new char[1000]).replace('\0', 'a');
        assertTrue(S3ParameterValidatorUtil.isValidSAMLMetadata(metadata));
    }

    /**
     * Test S3ValidatorUtil#isValidSAMLMetadata. case - Name equals null
     */
    @Test
    public void IsValidSamlMetadata_NameNull_False() {
        String metadata = null;
        assertFalse(S3ParameterValidatorUtil.isValidSAMLMetadata(metadata));
    }

    /**
     * Test S3ValidatorUtil#isValidSAMLMetadata. case - Length of the name is
     * out of range.
     */
    @Test
    public void IsValidSamlMetadata_NameLengthOutOfRange_False() {
        String metadata = new String(new char[10000001]).replace('\0', 'a');
        assertFalse(S3ParameterValidatorUtil.isValidSAMLMetadata(metadata));

        metadata = "";
        assertFalse(S3ParameterValidatorUtil.isValidSAMLMetadata(metadata));
    }

    /**
     * Test S3ValidatorUtil#isValidARN. case - Valid ARN.
     */
    @Test
    public void IsValidARN_ValidARN_True() {
        String arn = "arn:seagate:iam:842327222694:role:test";
        assertTrue(S3ParameterValidatorUtil.isValidARN(arn));
    }

    /**
     * Test S3ValidatorUtil#isValidARN. case - ARN is null.
     */
    @Test
    public void IsValidARN_ARNIsNULL_False() {
        String arn = null;
        assertFalse(S3ParameterValidatorUtil.isValidARN(arn));
    }

    /**
     * Test S3ValidatorUtil#isValidARN. case - ARN length out of range.
     */
    @Test
    public void IsValidARN_InvalidARN_False() {
        String duration = "abc";
        assertFalse(S3ParameterValidatorUtil.isValidARN(duration));

        duration = new String(new char[2050]).replace('\0', 'a');
        assertFalse(S3ParameterValidatorUtil.isValidARN(duration));
    }
}
