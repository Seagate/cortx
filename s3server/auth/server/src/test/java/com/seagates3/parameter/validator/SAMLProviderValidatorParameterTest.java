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
 * Original creation date: 17-Nov-2015
 */
package com.seagates3.parameter.validator;

import com.seagates3.parameter.validator.SAMLProviderParameterValidator;
import java.util.Map;
import java.util.TreeMap;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import org.junit.Before;
import org.junit.Test;

public class SAMLProviderValidatorParameterTest {

    SAMLProviderParameterValidator validator;
    Map requestBody;

    public SAMLProviderValidatorParameterTest() {
        validator = new SAMLProviderParameterValidator();
    }

    @Before
    public void setUp() {
        requestBody = new TreeMap();
    }

    /**
     * Test SAMLProvider#isValidCreateParams. Case - Provider name is null (also tests
     * invalid name).
     */
    @Test
    public void Create_NameNull_False() {
        assertFalse(validator.isValidCreateParams(requestBody));
    }

    /**
     * Test SAMLProvider#isValidCreateParams. Case - SAML Metadata is null (also tests for
     * invalid metadata).
     */
    @Test
    public void Create_SAMLMetadataNull_False() {
        requestBody.put("Name", "Test");
        assertFalse(validator.isValidCreateParams(requestBody));
    }

    /**
     * Test SAMLProvider#isValidCreateParams. Case - Valid inputs.
     */
    @Test
    public void Create_ValidInputs_True() {
        requestBody.put("Name", "Test");
        String metadataDoc = new String(new char[1010]).replace('\0', 'a');
        requestBody.put("SAMLMetadataDocument", metadataDoc);

        assertTrue(validator.isValidCreateParams(requestBody));
    }

    /**
     * Test SAMLProvider#isValidDeleteParams. Case - SAML Provider ARN is not provided (also
     * tests for invalid ARN).
     */
    @Test
    public void Delete_SAMLProviderARNNull_False() {
        assertFalse(validator.isValidDeleteParams(requestBody));
    }

    /**
     * Test SAMLProvider#isValidDeleteParams. Case - Valid inputs
     */
    @Test
    public void Delete_ValidInputs_True() {
        requestBody.put("SAMLProviderArn", "arn:seagate:iam:::saml-provider/Test");
        assertTrue(validator.isValidDeleteParams(requestBody));
    }

    /**
     * Test SAMLProvider#isValidListParams. Case - Empty input.
     */
    @Test
    public void List_NoInput_True() {
        assertTrue(validator.isValidListParams(requestBody));
    }

    /**
     * Test SAMLProvider#isValidUpdateParams. Case - SAML Provider ARN is null (also tests
     * invalid name).
     */
    @Test
    public void Update_SAMLProviderARNNull_False() {
        assertFalse(validator.isValidUpdateParams(requestBody));
    }

    /**
     * Test SAMLProvider#isValidUpdateParams. Case - SAML Metadata is null (also tests for
     * invalid metadata).
     */
    @Test
    public void Update_SAMLMetadataNull_False() {
        requestBody.put("SAMLProviderArn", "arn:seagate:iam:::saml-provider/Test");
        assertFalse(validator.isValidUpdateParams(requestBody));
    }

    /**
     * Test SAMLProvider#isValidUpdateParams. Case - Valid inputs.
     */
    @Test
    public void Update_ValidInputs_True() {
        requestBody.put("SAMLProviderArn", "arn:seagate:iam:::saml-provider/Test");
        String metadataDoc = new String(new char[1010]).replace('\0', 'a');
        requestBody.put("SAMLMetadataDocument", metadataDoc);

        assertTrue(validator.isValidUpdateParams(requestBody));
    }
}
