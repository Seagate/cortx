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
 * Original author: Abhilekh Mustapure <abhilekh.mustapure@seagate.com>
 * Original creation date: 22-May-2019
 */

package com.seagates3.parameter.validator;

import com.seagates3.parameter.validator.UserLoginProfileParameterValidator;
import java.util.Map;
import java.util.TreeMap;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import org.junit.Before;
import org.junit.Test;

public
class UserLoginProfileValidatorParameterTest {
  UserLoginProfileParameterValidator userLoginProfileValidator;
  Map requestBody;

 public
  UserLoginProfileValidatorParameterTest() {
    userLoginProfileValidator = new UserLoginProfileParameterValidator();
  }

  @Before public void setUp() {
    requestBody = new TreeMap();
  }


  @Test public void Create_Password_True() {
    requestBody.put("UserName", "abcd");
    requestBody.put("Password", "abvdef");
    assertTrue(userLoginProfileValidator.isValidCreateParams(requestBody));
  }
}
