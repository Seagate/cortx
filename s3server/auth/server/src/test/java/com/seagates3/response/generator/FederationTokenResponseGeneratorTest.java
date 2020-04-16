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
 * Original creation date: 27-Dec-2015
 */
package com.seagates3.response.generator;

import com.seagates3.model.AccessKey;
import com.seagates3.model.User;
import com.seagates3.response.ServerResponse;
import io.netty.handler.codec.http.HttpResponseStatus;
import org.junit.Assert;
import org.junit.Test;

public class FederationTokenResponseGeneratorTest {

    @Test
    public void testGenerateCreateResponse() {
        User user = new User();
        user.setPath("/");
        user.setName("s3user");
        user.setId("123");

        AccessKey accessKey = new AccessKey();
        accessKey.setId("AKIA1234");
        accessKey.setSecretKey("kas-1aks12");
        accessKey.setToken("/siauaAsatOTkes");
        accessKey.setExpiry("2015-12-19T07:20:29.000+0530");
        accessKey.setStatus(AccessKey.AccessKeyStatus.ACTIVE);

        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<GetFederationTokenResponse "
                + "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<GetFederationTokenResult>"
                + "<Credentials>"
                + "<SessionToken>/siauaAsatOTkes</SessionToken>"
                + "<SecretAccessKey>kas-1aks12</SecretAccessKey>"
                + "<Expiration>2015-12-19T07:20:29.000+0530</Expiration>"
                + "<AccessKeyId>AKIA1234</AccessKeyId>"
                + "</Credentials>"
                + "<FederatedUser>"
                + "<Arn>arn:aws:sts::1:federated-user/s3user</Arn>"
                + "<FederatedUserId>123:s3user</FederatedUserId>"
                + "</FederatedUser>"
                + "<PackedPolicySize>6</PackedPolicySize>"
                + "</GetFederationTokenResult>"
                + "<ResponseMetadata>"
                + "<RequestId>0000</RequestId>"
                + "</ResponseMetadata>"
                + "</GetFederationTokenResponse>";

        FederationTokenResponseGenerator responseGenerator = new FederationTokenResponseGenerator();
        ServerResponse response = responseGenerator.generateCreateResponse(user, accessKey);

        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.CREATED, response.getResponseStatus());

    }
}
