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
import com.seagates3.model.SAMLResponseTokens;
import com.seagates3.model.User;
import com.seagates3.response.ServerResponse;
import io.netty.handler.codec.http.HttpResponseStatus;
import org.junit.Assert;
import org.junit.Test;

public class AssumeRoleWithSAMLResponseGeneratorTest {

    @Test
    public void testGenerateCreateResponse() {
        User user = new User();
        user.setPath("/");
        user.setName("s3user");
        user.setId("123");
        user.setRoleName("s3test");

        AccessKey accessKey = new AccessKey();
        accessKey.setId("AKIA1234");
        accessKey.setSecretKey("kas-1aks12");
        accessKey.setToken("/siauaAsatOTkes");
        accessKey.setExpiry("2015-12-19T07:20:29.000+0530");
        accessKey.setStatus(AccessKey.AccessKeyStatus.ACTIVE);

        SAMLResponseTokens samlResponse = new SAMLResponseTokens();
        samlResponse.setAudience("urn:seagate:webservices");
        samlResponse.setIssuer("s3TestIDP");
        samlResponse.setSubject("Test\\S3");
        samlResponse.setSubjectType("persistent");

        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<AssumeRoleWithSAMLResponse "
                + "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<AssumeRoleWithSAMLResult>"
                + "<Credentials>"
                + "<SessionToken>/siauaAsatOTkes</SessionToken>"
                + "<SecretAccessKey>kas-1aks12</SecretAccessKey>"
                + "<Expiration>2015-12-19T07:20:29.000+0530</Expiration>"
                + "<AccessKeyId>AKIA1234</AccessKeyId>"
                + "</Credentials>"
                + "<AssumedRoleUser><Arn>arn:seagate:sts:::s3test</Arn>"
                + "<AssumedRoleId>123:s3test</AssumedRoleId>"
                + "</AssumedRoleUser>"
                + "<Issuer>s3TestIDP</Issuer>"
                + "<Audience>urn:seagate:webservices</Audience>"
                + "<Subject>Test\\S3</Subject>"
                + "<SubjectType>persistent</SubjectType>"
                + "<NameQualifier>S4jYAZtpWsPHGsy1j5sKtEKOyW4=</NameQualifier>"
                + "<PackedPolicySize>6</PackedPolicySize>"
                + "</AssumeRoleWithSAMLResult>"
                + "<ResponseMetadata>"
                + "<RequestId>0000</RequestId>"
                + "</ResponseMetadata>"
                + "</AssumeRoleWithSAMLResponse>";

        AssumeRoleWithSAMLResponseGenerator responseGenerator
                = new AssumeRoleWithSAMLResponseGenerator();
        ServerResponse response
                = responseGenerator.generateCreateResponse(user, accessKey,
                        samlResponse);

        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.CREATED,
                response.getResponseStatus());

    }

    @Test
    public void testInvalidIdentityToken() {
        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<Error><Code>InvalidIdentityToken</Code>"
                + "<Message>The web identity token that was passed could not "
                + "be validated. Get a new identity token from the identity "
                + "provider and then retry the request.</Message></Error>"
                + "<RequestId>0000</RequestId>"
                + "</ErrorResponse>";

        AssumeRoleWithSAMLResponseGenerator responseGenerator
                = new AssumeRoleWithSAMLResponseGenerator();
        ServerResponse response = responseGenerator.invalidIdentityToken();

        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.BAD_REQUEST,
                response.getResponseStatus());
    }

    @Test
    public void testExpiredToken() {
        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<Error><Code>ExpiredToken</Code>"
                + "<Message>The web identity token that was passed is expired "
                + "or is not valid. Get a new identity token from the identity "
                + "provider and then retry the request.</Message></Error>"
                + "<RequestId>0000</RequestId>"
                + "</ErrorResponse>";

        AssumeRoleWithSAMLResponseGenerator responseGenerator
                = new AssumeRoleWithSAMLResponseGenerator();
        ServerResponse response = responseGenerator.expiredToken();

        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.BAD_REQUEST,
                response.getResponseStatus());
    }

    @Test
    public void testIDPRejectedClaim() {
        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<Error><Code>IDPRejectedClaim</Code>"
                + "<Message>The identity provider (IdP) reported that "
                + "authentication failed. This might be because the claim is "
                + "invalid.\nIf this error is returned for the "
                + "AssumeRoleWithWebIdentity operation, it can also "
                + "mean that the claim has expired or has been explicitly "
                + "revoked.</Message></Error>"
                + "<RequestId>0000</RequestId>"
                + "</ErrorResponse>";

        AssumeRoleWithSAMLResponseGenerator responseGenerator
                = new AssumeRoleWithSAMLResponseGenerator();
        ServerResponse response = responseGenerator.idpRejectedClaim();

        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.FORBIDDEN,
                response.getResponseStatus());
    }
}
