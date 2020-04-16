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
 * Original author: Sushant Mane <sushant.mane@seagate.com>
 * Original creation date: 01-Jul-2016
 */
package com.seagates3.authentication;

import java.util.Map;
import java.util.TreeMap;

import io.netty.handler.codec.http.DefaultHttpHeaders;
import io.netty.handler.codec.http.FullHttpRequest;
import io.netty.handler.codec.http.HttpMethod;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.powermock.api.mockito.PowerMockito;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;

import com.seagates3.authserver.AuthServerConfig;

import static org.mockito.Matchers.anyMap;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

@RunWith(PowerMockRunner.class)
@PrepareForTest(AuthServerConfig.class)
public class AWSRequestParserTest {

    AWSRequestParser awsRequestParser;
    ClientRequestToken clientRequestToken;
    Map<String, String> requestBody;

    @Before
    public void setup() throws Exception {
        awsRequestParser = Mockito.mock(AWSRequestParser.class,
                Mockito.CALLS_REAL_METHODS);
        clientRequestToken = new ClientRequestToken();
        requestBody = new TreeMap<>(String.CASE_INSENSITIVE_ORDER);

        String defaultEndPoints = "s3.seagate.com";
        String[] endpoints = {"s3-us.seagate.com", "s3-europe.seagate.com",
                "s3-asia.seagate.com"};

        PowerMockito.mockStatic(AuthServerConfig.class);
        PowerMockito.doReturn(endpoints).when(AuthServerConfig.class,
                "getEndpoints");
        PowerMockito.doReturn(defaultEndPoints).when(AuthServerConfig.class,
                "getDefaultEndpoint");
    }

    @Test
    public void parseRequestHeadersTest_VerifyBucketName() {
        requestBody.put("Host", "seagatebucket.s3.seagate.com");
        awsRequestParser.parseRequestHeaders(requestBody, clientRequestToken);
        Assert.assertTrue(clientRequestToken.isVirtualHost());
        Assert.assertEquals("seagatebucket",
                clientRequestToken.getBucketName());
    }

    @Test
    public void parseRequestHeadersTest_VerifyBucketName_WithPeriod() {
        requestBody.put("Host", "seagate.bucket.s3.seagate.com");
        awsRequestParser.parseRequestHeaders(requestBody, clientRequestToken);
        Assert.assertTrue(clientRequestToken.isVirtualHost());
        Assert.assertEquals("seagate.bucket",
                clientRequestToken.getBucketName());
    }

    @Test
    public void parseRequestHeadersTest_VerifyBucketName_WithHyphen() {
        requestBody.put("Host", "seagate-100200300.s3.seagate.com");
        awsRequestParser.parseRequestHeaders(requestBody, clientRequestToken);
        Assert.assertTrue(clientRequestToken.isVirtualHost());
        Assert.assertEquals("seagate-100200300",
                clientRequestToken.getBucketName());
    }

    @Test
    public void parseRequestHeadersTest_VerifyIsVirtualHost_False() {
        requestBody.put("Host", "s3.seagate.com");
        awsRequestParser.parseRequestHeaders(requestBody, clientRequestToken);
        Assert.assertFalse(clientRequestToken.isVirtualHost());
        Assert.assertEquals(null, clientRequestToken.getBucketName());
    }

    @Test
    public void parseRequestHeadersTest_HttpRequest() {
        // Arrange
        FullHttpRequest httpRequest = Mockito.mock(FullHttpRequest.class);
        ClientRequestToken clientRequestToken = Mockito.mock(ClientRequestToken.class);

        when(httpRequest.headers()).thenReturn(new DefaultHttpHeaders().add("host", "s3.seagate.com"));
        when(httpRequest.getMethod()).thenReturn(HttpMethod.GET);
        when(httpRequest.getUri()).thenReturn("/");

        // Act
        awsRequestParser.parseRequestHeaders(httpRequest, clientRequestToken);

        // Verify
        verify(clientRequestToken).setHttpMethod("GET");
        verify(clientRequestToken).setUri("/");
        verify(clientRequestToken).setQuery("");
        verify(clientRequestToken).setRequestHeaders(anyMap());
    }
}
