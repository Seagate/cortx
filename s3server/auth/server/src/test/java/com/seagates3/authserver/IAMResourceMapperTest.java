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

import com.seagates3.exception.AuthResourceNotFoundException;
import org.junit.Before;
import org.junit.Test;

import java.io.UnsupportedEncodingException;

import static org.junit.Assert.*;

public class IAMResourceMapperTest {

    @Before
    public void setUp() throws Exception {
        IAMResourceMapper.init();
    }

    @Test
    public void getResourceMapTest() throws UnsupportedEncodingException,
            AuthResourceNotFoundException {
        ResourceMap resourceMap = IAMResourceMapper.getResourceMap("CreateAccount");

        assertEquals("com.seagates3.controller.AccountController",
                resourceMap.getControllerClass());
        assertEquals("com.seagates3.parameter.validator.AccountParameterValidator",
                resourceMap.getParamValidatorClass());
        assertEquals("create", resourceMap.getControllerAction());
        assertEquals("isValidCreateParams", resourceMap.getParamValidatorMethod());
    }

    @Test(expected = AuthResourceNotFoundException.class)
    public void getResourceMapTest_NullAction() throws AuthResourceNotFoundException {
        IAMResourceMapper.getResourceMap(null);
    }

    @Test(expected = AuthResourceNotFoundException.class)
    public void getResourceMapTest_InvalidAction() throws AuthResourceNotFoundException {
        IAMResourceMapper.getResourceMap("RandomAction");
    }
}