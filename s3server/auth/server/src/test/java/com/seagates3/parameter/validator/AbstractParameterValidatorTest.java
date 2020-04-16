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
import com.seagates3.parameter.validator.AbstractParameterValidator;
import java.util.Map;
import java.util.TreeMap;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import org.junit.Before;
import org.junit.Test;

public class AbstractParameterValidatorTest extends AbstractParameterValidator {
    RoleParameterValidator roleValidator;
    Map requestBody;

    public AbstractParameterValidatorTest() {
        roleValidator = new RoleParameterValidator();
    }

    @Before
    public void setUp() {
        requestBody = new TreeMap();
    }

    /**
     * Test AbstractValidator#isValidCreateParams.
     */
    @Test
    public void Create_EmptyInput_True() {
        assertTrue(isValidCreateParams(requestBody));
    }

    /**
     * Test AbstractValidator#isValidDeleteParams.
     */
    @Test
    public void Delete_EmptyInput_True() {
        assertTrue(isValidDeleteParams(requestBody));
    }


    /**
     * Test AbstractValidator#isValidListParams.
     * Case - Empty input.
     */
    @Test
    public void List_EmptyInput_True() {
        assertTrue(isValidListParams(requestBody));
    }

    /**
     * Test AbstractValidator#isValidListParams.
     * Case - Invalid Path Prefix.
     */
    @Test
    public void List_InvalidPathPrefix_False() {
        requestBody.put("PathPrefix", "segate/");
        assertFalse(isValidListParams(requestBody));
    }

    /**
     * Test AbstractValidator#isValidListParams.
     * Case - Invalid Max Items.
     */
    @Test
    public void List_InvalidMaxItems_False() {
        requestBody.put("PathPrefix", "/segate/");
        requestBody.put("MaxItems", "0");
        assertFalse(isValidListParams(requestBody));
    }

    /**
     * Test AbstractValidator#isValidListParams.
     * Case - Invalid Marker.
     */
    @Test
    public void List_InvalidMarker_False() {
        requestBody.put("PathPrefix", "/segate/");
        requestBody.put("MaxItems", "100");

        char c = "\u0100".toCharArray()[0];
        String marker = String.valueOf(c);

        requestBody.put("Marker", marker);
        assertFalse(isValidListParams(requestBody));
    }

    /**
     * Test AbstractValidator#isValidListParams.
     * Case - Valid inputs.
     */
    @Test
    public void List_ValidInputs_True() {
        requestBody.put("PathPrefix", "/segate/");
        requestBody.put("MaxItems", "100");
        requestBody.put("Marker", "abc");
        assertTrue(isValidListParams(requestBody));
    }

    /**
     * Test AbstractValidator#isValidUpdateParams.
     */
    @Test
    public void Update_EmptyInput_True() {
        assertTrue(isValidUpdateParams(requestBody));
    }
}
