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
package com.seagates3.response.formatter.xml;

import com.seagates3.response.ServerResponse;
import io.netty.handler.codec.http.HttpResponseStatus;
import java.util.LinkedHashMap;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;

public class AssumeRolewithSAMLResponseFormatterTest {

    @Rule
    public final ExpectedException exception = ExpectedException.none();

    @Test
    public void testFormatCreateResponse() {
        LinkedHashMap<String, String> credentials = new LinkedHashMap<>();
        credentials.put("SessionToken", "ajsksmz/1235");
        credentials.put("SecretAccessKey", "kask-sk3nv");
        credentials.put("Expiration", "2015-12-19T07:20:29.000+0530");
        credentials.put("AccessKeyId", "AKSIAS");

        LinkedHashMap<String, String> federatedUser = new LinkedHashMap<>();
        federatedUser.put("Arn", "arn:aws:sts::1:assumed-role/s3test");
        federatedUser.put("AssumedRoleId", "123:s3testsession");

        LinkedHashMap<String, String> samlAttributes = new LinkedHashMap<>();
        samlAttributes.put("Issuer", "s3TestIDP");
        samlAttributes.put("Audience", "urn:seagate:webservices");
        samlAttributes.put("Subject", "Test\\S3");
        samlAttributes.put("SubjectType", "persistent");
        samlAttributes.put("NameQualifier", "S4jYAZtpWsPHGsy1j5sKtEKOyW4=");

        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<AssumeRoleWithSAMLResponse "
                + "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<AssumeRoleWithSAMLResult>"
                + "<Credentials>"
                + "<SessionToken>ajsksmz/1235</SessionToken>"
                + "<SecretAccessKey>kask-sk3nv</SecretAccessKey>"
                + "<Expiration>2015-12-19T07:20:29.000+0530</Expiration>"
                + "<AccessKeyId>AKSIAS</AccessKeyId>"
                + "</Credentials>"
                + "<AssumedRoleUser>"
                + "<Arn>arn:aws:sts::1:assumed-role/s3test</Arn>"
                + "<AssumedRoleId>123:s3testsession</AssumedRoleId>"
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

        ServerResponse response
                = new AssumeRoleWithSAMLResponseFormatter().formatCreateResponse(
                        credentials, federatedUser, samlAttributes, "6", "0000");

        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.CREATED, response.getResponseStatus());
    }

    @Test
    public void testFormatCreateResponseException() {
        exception.expect(UnsupportedOperationException.class);
        new AssumeRoleWithSAMLResponseFormatter().formatCreateResponse(
                "AssumeRoleWithSAML", "AssumeRoleSAML", null, "0000");
    }
}
