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
 * Original creation date: 21-Dec-2015
 */
package com.seagates3.response.generator;

import com.seagates3.model.Account;
import com.seagates3.model.SAMLProvider;
import com.seagates3.response.ServerResponse;
import io.netty.handler.codec.http.HttpResponseStatus;
import java.util.ArrayList;
import org.junit.Assert;
import org.junit.Test;

public class SAMLProviderResponseGeneratorTest {

    @Test
    public void testCreateResponse() {
        Account account = new Account();
        account.setName("s3test");

        SAMLProvider samlProvider = new SAMLProvider();
        samlProvider.setName("s3TestIDP");
        samlProvider.setAccount(account);

        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<CreateSAMLProviderResponse "
                + "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<CreateSAMLProviderResult>"
                + "<SAMLProviderArn>arn:seagate:iam::s3test:s3TestIDP"
                + "</SAMLProviderArn>"
                + "</CreateSAMLProviderResult>"
                + "<ResponseMetadata>"
                + "<RequestId>0000</RequestId>"
                + "</ResponseMetadata>"
                + "</CreateSAMLProviderResponse>";

        SAMLProviderResponseGenerator responseGenerator
                = new SAMLProviderResponseGenerator();
        ServerResponse response = responseGenerator.generateCreateResponse(
                samlProvider);

        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.CREATED,
                response.getResponseStatus());
    }

    @Test
    public void testListResponse() {
        ArrayList<SAMLProvider> providers = new ArrayList<>();

        SAMLProvider provider1 = new SAMLProvider();
        provider1.setName("S3Test1");
        provider1.setExpiry("2016-12-19T07:20:29.000+0530");
        provider1.setCreateDate("2015-12-19T07:20:29.000+0530");

        SAMLProvider provider2 = new SAMLProvider();
        provider2.setName("S3Test2");
        provider2.setExpiry("2016-12-18T07:20:29.000+0530");
        provider2.setCreateDate("2015-12-18T07:20:29.000+0530");

        providers.add(provider1);
        providers.add(provider2);

        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<ListSAMLProvidersResponse "
                + "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<ListSAMLProvidersResult>"
                + "<SAMLProviderList>"
                + "<member>"
                + "<Arn>arn:seagate:iam:::S3Test1</Arn>"
                + "<ValidUntil>2016-12-19T07:20:29.000+0530</ValidUntil>"
                + "<CreateDate>2015-12-19T07:20:29.000+0530</CreateDate>"
                + "</member>"
                + "<member>"
                + "<Arn>arn:seagate:iam:::S3Test2</Arn>"
                + "<ValidUntil>2016-12-18T07:20:29.000+0530</ValidUntil>"
                + "<CreateDate>2015-12-18T07:20:29.000+0530</CreateDate>"
                + "</member>"
                + "</SAMLProviderList>"
                + "<IsTruncated>false</IsTruncated>"
                + "</ListSAMLProvidersResult>"
                + "<ResponseMetadata>"
                + "<RequestId>0000</RequestId>"
                + "</ResponseMetadata>"
                + "</ListSAMLProvidersResponse>";

        SAMLProvider[] samlProviderList = new SAMLProvider[providers.size()];
        SAMLProviderResponseGenerator responseGenerator
                = new SAMLProviderResponseGenerator();
        ServerResponse response = responseGenerator.generateListResponse(
                providers.toArray(samlProviderList));

        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.OK, response.getResponseStatus());
    }

    @Test
    public void testDeleteResponse() {
        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<DeleteSAMLProviderResponse "
                + "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<ResponseMetadata>"
                + "<RequestId>0000</RequestId>"
                + "</ResponseMetadata>"
                + "</DeleteSAMLProviderResponse>";

        SAMLProviderResponseGenerator responseGenerator
                = new SAMLProviderResponseGenerator();
        ServerResponse response = responseGenerator.generateDeleteResponse();

        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.OK, response.getResponseStatus());
    }

    @Test
    public void testUpdateResponse() {
        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<UpdateSAMLProviderResponse "
                + "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<UpdateSAMLProviderResult>"
                + "<SAMLProviderArn>arn:seagate:iam:::s3TestIDP"
                + "</SAMLProviderArn>"
                + "</UpdateSAMLProviderResult>"
                + "<ResponseMetadata>"
                + "<RequestId>0000</RequestId>"
                + "</ResponseMetadata>"
                + "</UpdateSAMLProviderResponse>";

        SAMLProviderResponseGenerator responseGenerator
                = new SAMLProviderResponseGenerator();
        ServerResponse response
                = responseGenerator.generateUpdateResponse("s3TestIDP");

        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.OK, response.getResponseStatus());
    }
}
