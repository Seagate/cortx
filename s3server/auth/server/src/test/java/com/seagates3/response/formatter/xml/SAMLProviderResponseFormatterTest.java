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
package com.seagates3.response.formatter.xml;

import com.seagates3.response.ServerResponse;
import io.netty.handler.codec.http.HttpResponseStatus;
import java.util.LinkedHashMap;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;

public class SAMLProviderResponseFormatterTest {

    @Rule
    public final ExpectedException exception = ExpectedException.none();

    @Test
    public void testFormatCreateResponse() {
        LinkedHashMap responseElements = new LinkedHashMap();
        responseElements.put("Arn", "arn:seagate:iam::s3test:s3TestIDP");

        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<CreateSAMLProviderResponse "
                + "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<CreateSAMLProviderResult>"
                + "<SAMLProviderArn>arn:seagate:iam::s3test:s3TestIDP"
                + "</SAMLProviderArn>"
                + "</CreateSAMLProviderResult>"
                + "<ResponseMetadata>"
                + "<RequestId>9999</RequestId>"
                + "</ResponseMetadata>"
                + "</CreateSAMLProviderResponse>";

        ServerResponse response = new SAMLProviderResponseFormatter()
                .formatCreateResponse("CreateSAMLProvider",
                        null, responseElements, "9999");

        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.CREATED, response.getResponseStatus());
    }

    @Test
    public void testFormatUpdateResponse() {
        LinkedHashMap responseElements = new LinkedHashMap();
        responseElements.put("Name", "s3TestIDP");

        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<UpdateSAMLProviderResponse "
                + "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<UpdateSAMLProviderResult>"
                + "<SAMLProviderArn>arn:seagate:iam:::s3TestIDP"
                + "</SAMLProviderArn>"
                + "</UpdateSAMLProviderResult>"
                + "<ResponseMetadata>"
                + "<RequestId>9999</RequestId>"
                + "</ResponseMetadata>"
                + "</UpdateSAMLProviderResponse>";

        ServerResponse response = new SAMLProviderResponseFormatter()
                .formatUpdateResponse("s3TestIDP", "9999");

        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.OK, response.getResponseStatus());
    }

    @Test
    public void testFormatUpdateResponseException() {
        exception.expect(UnsupportedOperationException.class);
        new SAMLProviderResponseFormatter().formatUpdateResponse("UpdateSAMLProvider");
    }
}
