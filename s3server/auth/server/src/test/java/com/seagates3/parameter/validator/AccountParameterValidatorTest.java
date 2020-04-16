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

import java.util.Map;
import java.util.TreeMap;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import org.junit.Before;
import org.junit.Test;

public class AccountParameterValidatorTest {

    AccountParameterValidator accountValidator;
    Map requestBody;

    public AccountParameterValidatorTest() {
        accountValidator = new AccountParameterValidator();
    }

    @Before
    public void setUp() {
        requestBody = new TreeMap();
    }

    /**
     * Test Account#isValidCreateParams. Case - Account name is not provided.
     */
    @Test
    public void Create_AccountNameNull_False() {
        assertFalse(accountValidator.isValidCreateParams(requestBody));
    }

    /**
     * Test Account#isValidCreateParams. Case - Email is valid.
     */
    @Test
    public void Create_InvalidEmail_False() {
        requestBody.put("AccountName", "seagate");
        requestBody.put("Email", "testuser");
        assertFalse(accountValidator.isValidCreateParams(requestBody));
    }

    /**
     * Test Account#isValidCreateParams. Case - Email is valid.
     */
    @Test
    public void Create_InvalidAccountName_False() {
        requestBody.put("AccountName", "arj-123");
        requestBody.put("Email", "testuser");
        assertFalse(accountValidator.isValidCreateParams(requestBody));
    }

    /**
     * Test Account#isValidCreateParams. Case - Account name is valid.
     */
    @Test
    public void Create_ValidInputParams_True() {
        requestBody.put("AccountName", "seagate");
        requestBody.put("Email", "testuser@seagate.com");
        assertTrue(accountValidator.isValidCreateParams(requestBody));
    }
}
