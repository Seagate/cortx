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
 * Original creation date: 11-June-2019
 */

package com.seagates3.response.generator;

import org.junit.Assert;
import org.junit.Test;

import com.seagates3.model.User;
import com.seagates3.response.ServerResponse;

import io.netty.handler.codec.http.HttpResponseStatus;

public
class UserLoginProfileResponseGeneratorTest {

  /**
   * Below method will test generateGetResponse method
   */
  @Test public void generateGetResponseTest() {
    User user = new User();
    user.setName("testUser");
    user.setCreateDate("2019-06-10T07:38:07Z");
    user.setProfileCreateDate("2019-06-16 15:38:53+00:00");
    user.setPwdResetRequired("false");
    final String expectedResponseBody =
        "<?xml version=\"1.0\" " + "encoding=\"UTF-8\" standalone=\"no\"?>" +
        "<GetLoginProfileResponse " +
        "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
        "<GetLoginProfileResult>" + "<LoginProfile>" +
        "<UserName>testUser</UserName>" +
        "<CreateDate>2019-06-16 15:38:53+00:00</CreateDate>" +
        "<PasswordResetRequired>false</PasswordResetRequired>" +
        "</LoginProfile>" + "</GetLoginProfileResult>" + "<ResponseMetadata>" +
        "<RequestId>0000</RequestId>" + "</ResponseMetadata>" +
        "</GetLoginProfileResponse>";

    UserLoginProfileResponseGenerator responseGenerator =
        new UserLoginProfileResponseGenerator();
    ServerResponse response = responseGenerator.generateGetResponse(user);

    Assert.assertEquals(expectedResponseBody, response.getResponseBody());
    Assert.assertEquals(HttpResponseStatus.OK, response.getResponseStatus());
  }

  /**
   * Below test will check successful UpdateLoginProfile response
   */
  @Test public void generateUpdateResponseTest() {
    UserLoginProfileResponseGenerator responseGenerator =
        new UserLoginProfileResponseGenerator();
    final String expectedResponseBody =
        "<?xml version=\"1.0\" " + "encoding=\"UTF-8\" standalone=\"no\"?>" +
        "<UpdateLoginProfileResponse " +
        "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
        "<ResponseMetadata>" + "<RequestId>0000</RequestId>" +
        "</ResponseMetadata>" + "</UpdateLoginProfileResponse>";
    ServerResponse response = responseGenerator.generateUpdateResponse();
    Assert.assertEquals(expectedResponseBody, response.getResponseBody());
    Assert.assertEquals(HttpResponseStatus.OK, response.getResponseStatus());
  }
}
