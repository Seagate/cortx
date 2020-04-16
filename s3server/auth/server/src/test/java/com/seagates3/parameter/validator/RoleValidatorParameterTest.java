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

import com.seagates3.parameter.validator.RoleParameterValidator;
import java.util.Map;
import java.util.TreeMap;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import org.junit.Before;
import org.junit.Test;

public class RoleValidatorParameterTest {
    RoleParameterValidator roleValidator;
    Map requestBody;

    final String assumeRolePolicyDoc = "{\n" +
"  \"Version\": \"2012-10-17\",\n" +
"  \"Statement\": {\n" +
"    \"Effect\": \"Allow\",\n" +
"    \"Principal\": {\"Service\": \"test\"},\n" +
"    \"Action\": \"sts:AssumeRole\"\n" +
"  }\n" +
"}";

    public RoleValidatorParameterTest() {
        roleValidator = new RoleParameterValidator();
    }

    @Before
    public void setUp() {
        requestBody = new TreeMap();
    }

    /**
     * Test Role#isValidCreateParams.
     * Case - Role name is not provided.
     */
    @Test
    public void Create_RoleNameNull_False() {
        assertFalse(roleValidator.isValidCreateParams(requestBody));
    }

    /**
     * Test Role#isValidCreateParams.
     * Case - Assume role policy document is not provided.
     */
    @Test
    public void Create_AssumeRolePolicyDocNull_False() {
        requestBody.put("RoleName", "admin");
        assertFalse(roleValidator.isValidCreateParams(requestBody));
    }

    /**
     * Test Role#isValidCreateParams.
     * Case - Role name and Assume role policy doc are valid, Path is not provided.
     */
    @Test
    public void Create_ValidRoleNamePolicyDocAndPathEmpty_True() {
        requestBody.put("RoleName", "admin");
        requestBody.put("AssumeRolePolicyDocument", assumeRolePolicyDoc);
        assertTrue(roleValidator.isValidCreateParams(requestBody));
    }

    /**
     * Test Role#isValidCreateParams.
     * Case - Role name and path are valid.
     */
    @Test
    public void Create_ValidInputs_True() {
        requestBody.put("RoleName", "admin");
        requestBody.put("AssumeRolePolicyDocument", assumeRolePolicyDoc);
        requestBody.put("Path", "/seagate/test/");
        assertTrue(roleValidator.isValidCreateParams(requestBody));
    }

    /**
     * Test Role#isValidCreateParams.
     * Case - Role name is invalid.
     */
    @Test
    public void Create_InvalidRoleName_False() {
        requestBody.put("RoleName", "admin$^");
        assertFalse(roleValidator.isValidCreateParams(requestBody));
    }

    /**
     * Test Role#isValidCreateParams.
     * Case - Role name is invalid.
     */
    @Test
    public void Create_InvalidPolidyDoc_False() {
        char c = "\u0100".toCharArray()[0];
        String policyDoc = String.valueOf(c);

        requestBody.put("RoleName", "admin");
        requestBody.put("AssumeRolePolicyDocument", policyDoc);
        assertFalse(roleValidator.isValidCreateParams(requestBody));
    }

    /**
     * Test Role#isValidCreateParams.
     * Case - Role name and role policy are valid, path is invalid.
     */
    @Test
    public void Create_InvalidPath_False() {
        requestBody.put("RoleName", "admin");
        requestBody.put("AssumeRolePolicyDocument", assumeRolePolicyDoc);
        requestBody.put("Path", "seagate/test/");
        assertFalse(roleValidator.isValidCreateParams(requestBody));
    }

    /**
     * Test Role#isValidDeleteParams.
     * Case - Role name is not provided.
     */
    @Test
    public void Delete_RoleNameNull_False() {
        assertFalse(roleValidator.isValidDeleteParams(requestBody));
    }

    /**
     * Test Role#isValidDeleteParams.
     * Case - Role name is valid.
     */
    @Test
    public void Delete_ValidRoleName_True() {
        requestBody.put("RoleName", "admin");
        assertTrue(roleValidator.isValidDeleteParams(requestBody));
    }

    /**
     * Test Role#isValidDeleteParams.
     * Case - Role name is invalid.
     */
    @Test
    public void Delete_InValidRoleName_False() {
        requestBody.put("RoleName", "admin$^");
        assertFalse(roleValidator.isValidDeleteParams(requestBody));
    }
}
