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

import com.seagates3.model.Account;
import com.seagates3.authentication.ClientRequestToken;
import com.seagates3.model.Requestor;
import com.seagates3.response.ServerResponse;
import io.netty.handler.codec.http.HttpResponseStatus;
import org.junit.Assert;
import org.junit.Test;

public class AuthenticationResponseGeneratorTest {

    @Test
    public void testCreateResponse() {
        ClientRequestToken requestToken = new ClientRequestToken();
        requestToken.setSignature("testsign");

        Account account = new Account();
        account.setId("12345");
        account.setName("s3test");
        account.setCanonicalId("dsfhgsjhsdgfQW23cdjbjb");
        account.setEmail("abc@sjsj.com");

        Requestor requestor = new Requestor();
        requestor.setId("123");
        requestor.setName("s3test");
        requestor.setAccount(account);

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" +
            "<AuthenticateUserResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<AuthenticateUserResult>" + "<UserId>123</UserId>" +
            "<UserName>s3test</UserName>" + "<AccountId>12345</AccountId>" +
            "<AccountName>s3test</AccountName>" +
            "<SignatureSHA256>testsign</SignatureSHA256>" +
            "<CanonicalId>dsfhgsjhsdgfQW23cdjbjb</CanonicalId>" +
            "<Email>abc@sjsj.com</Email>" + "</AuthenticateUserResult>" +
            "<ResponseMetadata>" + "<RequestId>0000</RequestId>" +
            "</ResponseMetadata>" + "</AuthenticateUserResponse>";

        AuthenticationResponseGenerator responseGenerator
                = new AuthenticationResponseGenerator();
        ServerResponse response = responseGenerator.
                generateAuthenticatedResponse(requestor, requestToken);

        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.OK, response.getResponseStatus());
    }
}

