/*
 * COPYRIGHT 2017 SEAGATE LLC
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
 * Original author: Sushant Mane <sushant.mane@seagate.com>
 * Original creation date: 01-Feb-2017
 */

package com.seagates3.authserver;

import org.junit.Before;
import org.junit.Test;

import static org.junit.Assert.assertEquals;

public class ResourceMapTest {

    private ResourceMap resourceMap;

    @Before
    public void setUp() throws Exception {
        resourceMap = new ResourceMap("Account", "create");
    }

    @Test
    public void getControllerClassTest() {
        assertEquals("com.seagates3.controller.AccountController",
                resourceMap.getControllerClass());
    }

    @Test
    public void getParamValidatorClassTest() {
        assertEquals("com.seagates3.parameter.validator.AccountParameterValidator",
                resourceMap.getParamValidatorClass());
    }

    @Test
    public void getControllerActionTest() {
        assertEquals("create", resourceMap.getControllerAction());
    }

    @Test
    public void getParamValidatorMethodTest() {
        assertEquals("isValidCreateParams", resourceMap.getParamValidatorMethod());
    }
}