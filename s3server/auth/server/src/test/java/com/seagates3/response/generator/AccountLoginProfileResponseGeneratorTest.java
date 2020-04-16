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
 * Original author:  Basavaraj Kirunge <basavaraj.kirunge@seagate.com>
 * Original creation date: 20-Jun-2019
 */

package com.seagates3.response.generator;

import org.junit.Assert;
import org.junit.Test;

import com.seagates3.model.Account;
import com.seagates3.response.ServerResponse;

import io.netty.handler.codec.http.HttpResponseStatus;

public
class AccountLoginProfileResponseGeneratorTest {

  /**
   * Below method will test generateGetResponse method
   */
  @Test public void generateGetResponseTest() {
    Account account = new Account();
    account.setName("testUser");
    account.setProfileCreateDate("2019-06-16 15:38:53+00:00");
    account.setPwdResetRequired("false");
    final String expectedResponseBody =
        "<?xml version=\"1.0\" " + "encoding=\"UTF-8\" standalone=\"no\"?>" +
        "<GetAccountLoginProfileResponse " +
        "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
        "<GetAccountLoginProfileResult>" + "<LoginProfile>" +
        "<AccountName>testUser</AccountName>" +
        "<CreateDate>2019-06-16 15:38:53+00:00</CreateDate>" +
        "<PasswordResetRequired>false</PasswordResetRequired>" +
        "</LoginProfile>" + "</GetAccountLoginProfileResult>" +
        "<ResponseMetadata>" + "<RequestId>0000</RequestId>" +
        "</ResponseMetadata>" + "</GetAccountLoginProfileResponse>";

    AccountLoginProfileResponseGenerator responseGenerator =
        new AccountLoginProfileResponseGenerator();
    ServerResponse response = responseGenerator.generateGetResponse(account);

    Assert.assertEquals(expectedResponseBody, response.getResponseBody());
    Assert.assertEquals(HttpResponseStatus.OK, response.getResponseStatus());
  }
}
