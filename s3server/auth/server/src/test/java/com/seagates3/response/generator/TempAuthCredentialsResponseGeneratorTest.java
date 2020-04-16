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

package com.seagates3.response.generator;

import java.util.Date;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;

import com.seagates3.model.AccessKey;
import com.seagates3.response.ServerResponse;
import com.seagates3.util.DateUtil;

import io.netty.handler.codec.http.HttpResponseStatus;

public
class TempAuthCredentialsResponseGeneratorTest {

  @Before public void setUp() throws Exception {
  }

  /**
   * Below test will check successful GetTempAuthCredentials response
   */
  @Test public void generateCreateResponseTest() {
    TempAuthCredentialsResponseGenerator responseGenerator =
        new TempAuthCredentialsResponseGenerator();

    AccessKey accessKey = new AccessKey();
    accessKey.setUserId("123");
    accessKey.setId("sdfjgsdfhRRTT335");
    accessKey.setSecretKey("djegjhgwhfg667766333EERTFFFxxxxxx3c");
    accessKey.setToken("djgfhsgshfgTTTTTVVQQAPP90jbdjcbsbdcshb");
    accessKey.setStatus(AccessKey.AccessKeyStatus.ACTIVE);
    Date expiryDate = new Date(DateUtil.getCurrentTime());
    accessKey.setExpiry(DateUtil.toServerResponseFormat(expiryDate));

    ServerResponse response =
        responseGenerator.generateCreateResponse("user1", accessKey);
    Assert.assertEquals(HttpResponseStatus.CREATED,
                        response.getResponseStatus());
  }
}
