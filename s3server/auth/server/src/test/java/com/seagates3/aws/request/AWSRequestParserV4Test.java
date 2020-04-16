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
 * Original creation date: 21-Mar-2016
 */
package com.seagates3.aws.request;

import com.seagates3.authentication.AWSRequestParserV4;
import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.aws.AWSV4RequestHelper;
import com.seagates3.exception.InvalidTokenException;
import com.seagates3.authentication.ClientRequestToken;
import io.netty.buffer.Unpooled;
import io.netty.handler.codec.http.DefaultFullHttpRequest;
import io.netty.handler.codec.http.FullHttpRequest;
import io.netty.handler.codec.http.HttpMethod;
import java.util.Map;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.internal.matchers.apachecommons.ReflectionEquals;
import org.powermock.api.mockito.PowerMockito;
import org.powermock.core.classloader.annotations.PowerMockIgnore;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;

@RunWith(PowerMockRunner.class)
@PrepareForTest(AuthServerConfig.class)
@PowerMockIgnore({"javax.management.*"})
public class AWSRequestParserV4Test {

    private final AWSRequestParserV4 parser;

    public AWSRequestParserV4Test() {
        parser = new AWSRequestParserV4();
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
                = AWSV4RequestHelper.getRequestHeadersPathStyle();
        ClientRequestToken requestToken
                = AWSV4RequestHelper.getRequestClientTokenPathStyle();

        Assert.assertThat(requestToken,
                new ReflectionEquals(parser.parse(requestHeaders)));

    }

    @Test
    public void Parse_RequestBody_VirtualHostStyle() throws InvalidTokenException {
        Map<String, String> requestHeaders
                = AWSV4RequestHelper.getRequestHeadersVirtualHostStyle();
        ClientRequestToken requestToken
                = AWSV4RequestHelper.getRequestClientTokenVirtualHostStyle();

        Assert.assertThat(requestToken,
                new ReflectionEquals(parser.parse(requestHeaders)));

    }

    @Test
    public void Parse_RequestHeadersEmptyHttpQuery_VirtualHostStyle() throws InvalidTokenException {
        FullHttpRequest request = Mockito.mock(DefaultFullHttpRequest.class);

        Mockito.when(request.content()).thenReturn(Unpooled.buffer(0));
        Mockito.when(request.headers()).thenReturn(
                AWSV4RequestHelper.getHttpHeaders());
        Mockito.when(request.getMethod()).thenReturn(HttpMethod.GET);
        Mockito.when(request.getUri()).thenReturn("/");

        ClientRequestToken expectedRequestToken
                = AWSV4RequestHelper.getFullHttpRequestClientTokenEmptyQuery();

        Assert.assertThat(expectedRequestToken,
                new ReflectionEquals(parser.parse(request)));
    }

    @Test
    public void Parse_RequestHeaderWithHttpQuery_VirtualHostStyle() throws InvalidTokenException {
        FullHttpRequest request = Mockito.mock(DefaultFullHttpRequest.class);

        Mockito.when(request.content()).thenReturn(Unpooled.buffer(0));
        Mockito.when(request.headers()).thenReturn(
                AWSV4RequestHelper.getHttpHeaders());
        Mockito.when(request.getMethod()).thenReturn(HttpMethod.GET);
        Mockito.when(request.getUri()).thenReturn("/?delimiter=");

        ClientRequestToken expectedRequestToken
                = AWSV4RequestHelper.getFullHttpRequestClientTokenWithQuery();

        ClientRequestToken actual = parser.parse(request);
        Assert.assertThat(expectedRequestToken,
                new ReflectionEquals(parser.parse(request)));
    }
}
