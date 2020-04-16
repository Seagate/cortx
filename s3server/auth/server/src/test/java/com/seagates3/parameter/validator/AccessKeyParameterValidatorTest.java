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
 * Original creation date: 13-Nov-2015
 */
package com.seagates3.parameter.validator;

import com.seagates3.parameter.validator.AccessKeyParameterValidator;
import java.util.Map;
import java.util.TreeMap;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import org.junit.Before;
import org.junit.Test;

public class AccessKeyParameterValidatorTest {
    AccessKeyParameterValidator accessKeyValidator;
    Map requestBody;

    public AccessKeyParameterValidatorTest() {
        accessKeyValidator = new AccessKeyParameterValidator();
    }

    @Before
    public void setUp() {
        requestBody = new TreeMap();
    }

    /**
     * Test AccessKey#isValidCreateParams.
     * Case - User name is not provided.
     */
    @Test
    public void Create_UserNameNull_True() {
        assertTrue(accessKeyValidator.isValidCreateParams(requestBody));
    }

    /**
     * Test AccessKey#isValidCreateParams.
     * Case - User name is valid.
     */
    @Test
    public void Create_ValidUserName_True() {
        requestBody.put("UserName", "root");
        assertTrue(accessKeyValidator.isValidCreateParams(requestBody));
    }

    /**
     * Test AccessKey#isValidDeleteParams.
     * Case - Access key id is not provided.
     */
    @Test
    public void Delete_AccessKeyIdNull_False() {
        assertFalse(accessKeyValidator.isValidDeleteParams(requestBody));
    }

    /**
     * Test AccessKey#isValidDeleteParams.
     * Case - Access key id is valid.
     *   User name is empty.
     */
    @Test
    public void Delete_ValidAccessKeyIdEmptyUserName_True() {
        requestBody.put("AccessKeyId", "ABCDEFGHIJKLMN123456");
        assertTrue(accessKeyValidator.isValidDeleteParams(requestBody));
    }

    /**
     * Test AccessKey#isValidDeleteParams.
     * Case - Access key id is valid.
     *   User name is valid.
     */
    @Test
    public void Delete_ValidAccessKeyIdAndUserName_True() {
        requestBody.put("AccessKeyId", "ABCDEFGHIJKLMN123456");
        requestBody.put("UserName", "root");
        assertTrue(accessKeyValidator.isValidDeleteParams(requestBody));
    }

    /**
     * Test AccessKey#isValidDeleteParams.
     * Case - Access key id is invalid.
     */
    @Test
    public void Delete_InValidAccessKeyId_False() {
        requestBody.put("AccessKeyId", "ABCDEFGHIJKLMN 123456");
        assertFalse(accessKeyValidator.isValidDeleteParams(requestBody));
    }

    /**
     * Test AccessKey#isValidDeleteParams.
     * Case - User name id is invalid.
     */
    @Test
    public void Delete_InValidAccessKeyIdAndUserName_False() {
        requestBody.put("AccessKeyId", "ABCDEFGHIJKLMN 123456");
        requestBody.put("UserName", "root*^");
        assertFalse(accessKeyValidator.isValidDeleteParams(requestBody));
    }

    /**
     * Test AccessKey#isValidListParams.
     * Case - Empty input.
     */
    @Test
    public void List_EmptyInput_True() {
        assertTrue(accessKeyValidator.isValidListParams(requestBody));
    }

    /**
     * Test AccessKey#isValidListParams.
     * Case - Invalid user name.
     */
    @Test
    public void List_InvalidPathPrefix_False() {
        requestBody.put("UserName", "root$^");
        assertFalse(accessKeyValidator.isValidListParams(requestBody));
    }

    /**
     * Test AccessKey#isValidListParams.
     * Case - Invalid Max Items.
     */
    @Test
    public void List_InvalidMaxItems_False() {
        requestBody.put("UserName", "root");
        requestBody.put("MaxItems", "0");
        assertFalse(accessKeyValidator.isValidListParams(requestBody));
    }

    /**
     * Test AccessKey#isValidListParams.
     * Case - Invalid Marker.
     */
    @Test
    public void List_InvalidMarker_False() {
        requestBody.put("UserName", "root");
        requestBody.put("MaxItems", "100");

        char c = "\u0100".toCharArray()[0];
        String marker = String.valueOf(c);

        requestBody.put("Marker", marker);
        assertFalse(accessKeyValidator.isValidListParams(requestBody));
    }

    /**
     * Test AccessKey#isValidListParams.
     * Case - Valid inputs.
     */
    @Test
    public void List_ValidInputs_True() {
        requestBody.put("UserName", "root");
        requestBody.put("MaxItems", "100");
        requestBody.put("Marker", "abc");
        assertTrue(accessKeyValidator.isValidListParams(requestBody));
    }

    /**
     * Test AccessKey#isValidUpdateParams.
     * Case - status is invalid.
     */
    @Test
    public void Update_InvalidAccessKeyStatus_False() {
        requestBody.put("Status", "active");
        assertFalse(accessKeyValidator.isValidUpdateParams(requestBody));
    }

    /**
     * Test AccessKey#isValidUpdateParams.
     * Case - User name is invalid.
     */
    @Test
    public void Update_InvalidUserName_False() {
        requestBody.put("Status", "Active");
        requestBody.put("UserName", "root$^");
        assertFalse(accessKeyValidator.isValidUpdateParams(requestBody));
    }

    /**
     * Test AccessKey#isValidUpdateParams.
     * Case - Access key id is invalid.
     */
    @Test
    public void Update_InvalidAccessKeyId_False() {
        requestBody.put("Status", "Active");
        requestBody.put("UserName", "root");
        requestBody.put("AccessKeyId", "ABCDE");
        assertFalse(accessKeyValidator.isValidUpdateParams(requestBody));
    }

    /**
     * Test AccessKey#isValidUpdateParams.
     * Case - Valid inputs.
     */
    @Test
    public void Update_ValidInputs_True() {
        requestBody.put("Status", "Active");
        requestBody.put("UserName", "root");
        requestBody.put("AccessKeyId", "ABCDEFGHIJKLMN123456");
        assertTrue(accessKeyValidator.isValidUpdateParams(requestBody));
    }
}
