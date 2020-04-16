/*
 * COPYRIGHT 2019 SEAGATE LLC
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
 * Original author:  Shalaka Dharap
 * Original creation date: 08-July-2019
 */

package com.seagates3.parameter.validator;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.util.HashMap;
import java.util.Map;

import org.junit.Before;
import org.junit.Test;

public
class TempAuthCredentialsParameterValidatorTest {

  Map<String, String> requestBody = new HashMap<>();
  TempAuthCredentialsParameterValidator tempAuthCredentialsParameterValidator =
      null;

  @Before public void setUp() throws Exception {
    requestBody.put("AccountName", "abcd");
    requestBody.put("Password", "pwd");
    tempAuthCredentialsParameterValidator =
        new TempAuthCredentialsParameterValidator();
  }

  @Test public void isValidCreateParams_withoutUserName_success_test() {
    assertTrue(
        tempAuthCredentialsParameterValidator.isValidCreateParams(requestBody));
  }

  @Test public void isValidCreateParams_withUserName_success_test() {
    requestBody.put("UserName", "uname");
    assertTrue(
        tempAuthCredentialsParameterValidator.isValidCreateParams(requestBody));
  }
}
