/*
 * COPYRIGHT 2016 SEAGATE LLC
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
 * Original creation date: 22-Mar-2016
 */
package com.seagates3.aws.request;

import com.seagates3.authentication.AWSRequestParserV2;
import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.aws.AWSV2RequestHelper;
import com.seagates3.exception.InvalidTokenException;
import com.seagates3.authentication.ClientRequestToken;
import java.util.Map;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.internal.matchers.apachecommons.ReflectionEquals;
import org.powermock.api.mockito.PowerMockito;
import org.powermock.core.classloader.annotations.PowerMockIgnore;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;

@RunWith(PowerMockRunner.class)
@PrepareForTest(AuthServerConfig.class)
@PowerMockIgnore( {"javax.management.*"})

public class AWSRequestParserV2Test {

    private final AWSRequestParserV2 parser;

    public AWSRequestParserV2Test() {
        parser = new AWSRequestParserV2();
    }

    @Before
    public void setup() throws Exception {
        String defaultEndPoints = "s3.seagate.com";
        String[] endpoints = {"s3-us.seagate.com",
            "s3-europe.seagate.com", "s3-asia.seagate.com"};

        PowerMockito.mockStatic(AuthServerConfig.class);
        PowerMockito.doReturn(endpoints).when(AuthServerConfig.class,
                "getEndpoints");

        PowerMockito.doReturn(defaultEndPoints).when(AuthServerConfig.class,
                "getDefaultEndpoint");
    }

    @Test
    public void Parse_RequestBody_PathStyle() throws InvalidTokenException {
        Map<String, String> requestHeaders
                = AWSV2RequestHelper.getRequestHeadersPathStyle();
        ClientRequestToken requestToken
                = AWSV2RequestHelper.getRequestClientTokenPathStyle();

        Assert.assertThat(requestToken,
                new ReflectionEquals(parser.parse(requestHeaders)));
    }

    @Test
    public void Parse_RequestBody_VirtualHostStyle() throws InvalidTokenException {
        Map<String, String> requestHeaders
                = AWSV2RequestHelper.getRequestHeadersVirtualHostStyle();
        ClientRequestToken requestToken
                = AWSV2RequestHelper.getRequestClientTokenVirtualHostStyle();

        ClientRequestToken actualtoken = parser.parse(requestHeaders);
        Assert.assertThat(requestToken,
                new ReflectionEquals(parser.parse(requestHeaders)));
    }
}
