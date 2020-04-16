/*
 * COPYRIGHT 2017 SEAGATE LLC
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
 * Original creation date: 14-Feb-2017
 */

package com.seagates3.authentication;

import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.aws.AWSV2RequestHelper;
import com.seagates3.aws.AWSV4RequestHelper;
import com.seagates3.exception.InvalidTokenException;
import com.seagates3.exception.InvalidAccessKeyException;
import com.seagates3.exception.InvalidArgumentException;

import io.netty.handler.codec.http.DefaultFullHttpRequest;
import io.netty.handler.codec.http.FullHttpRequest;
import io.netty.handler.codec.http.HttpMethod;
import io.netty.handler.codec.http.HttpVersion;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.powermock.api.mockito.mockpolicies.Slf4jMockPolicy;
import org.powermock.core.classloader.annotations.MockPolicy;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;

import java.util.Map;
import java.util.TreeMap;

import static org.junit.Assert.*;
import static org.mockito.Mockito.mock;
import static org.powermock.api.mockito.PowerMockito.mockStatic;
import static org.powermock.api.mockito.PowerMockito.when;

@RunWith(PowerMockRunner.class)
@PrepareForTest(AuthServerConfig.class)
@MockPolicy(Slf4jMockPolicy.class)
public class ClientRequestParserTest {

    private FullHttpRequest httpRequest;
    private Map<String, String> requestBody;

    @Before
    public void setUp() {
        String[] endpoints = {"s3-us.seagate.com",
                "s3-europe.seagate.com", "s3-asia.seagate.com"};
        requestBody = new TreeMap<>();
        httpRequest = mock(FullHttpRequest.class);
        mockStatic(AuthServerConfig.class);
        when(AuthServerConfig.getEndpoints()).thenReturn(endpoints);
    }

    @Test
    public void parseTest_AWSSign2_PathStyle() throws InvalidAccessKeyException,
            InvalidArgumentException{
        // Arrange
        requestBody = AWSV2RequestHelper.getRequestHeadersPathStyle();

        // Act
        ClientRequestToken token =  ClientRequestParser.parse(httpRequest, requestBody);

        // Verify
        assertNotNull(token);
        assertEquals("AKIAJTYX36YCKQSAJT7Q", token.getAccessKeyId());
    }

    @Test
    public void parseTest_AWSSign4_VirtualHostStyle() throws InvalidAccessKeyException,
            InvalidArgumentException{
        // Arrange
        requestBody = AWSV4RequestHelper.getRequestHeadersVirtualHostStyle();

        // Act
        ClientRequestToken token =  ClientRequestParser.parse(httpRequest, requestBody);

        // Verify
        assertNotNull(token);
        assertEquals("AKIAIOSFODNN7EXAMPLE", token.getAccessKeyId());
    }

    @Test(expected = NullPointerException.class)
    public void parseTest_RequestBodyNull_NullPointerException() throws InvalidAccessKeyException,
            InvalidArgumentException {

        ClientRequestParser.parse(httpRequest, null);
    }

    @Test
    public void parseTest_AuthenticateUser_AuthorizationHeaderNull() throws InvalidAccessKeyException,
            InvalidArgumentException {

        requestBody.put("Action", "AuthenticateUser");

        assertNull(ClientRequestParser.parse(httpRequest, requestBody));
    }

    @Test
    public void parseTest_AuthorizeUser_AuthorizationHeaderNull() throws InvalidAccessKeyException,
            InvalidArgumentException{

        requestBody.put("Action", "AuthorizeUser");

        assertNull(ClientRequestParser.parse(httpRequest, requestBody));
    }

    @Test
    public void parseTest_OtherAction_AuthorizationHeaderNull() throws InvalidAccessKeyException,
            InvalidArgumentException {
        requestBody.put("Action", "OtherAction");
        FullHttpRequest fullHttpRequest
                = new DefaultFullHttpRequest(HttpVersion.HTTP_1_1, HttpMethod.GET, "/");
        fullHttpRequest.headers().add("XYZ", "");
        when(httpRequest.headers()).thenReturn(fullHttpRequest.headers());

        assertNull(ClientRequestParser.parse(httpRequest, requestBody));
    }

    @Test
    public void parseTest_InvalidToken_AuthenticateUser() throws InvalidAccessKeyException,
            InvalidArgumentException{
        requestBody.put("Action", "AuthenticateUser");
        requestBody.put("host", "iam.seagate.com:9086");
        requestBody.put("authorization","AWS4-HMAC-SHA256 Credential=vcevceece/"
                + "2018 1001/us-west2/iam/aws4_request, SignedHeaders=host;x-amz-date, "
                + "Signature=676b4c41ad34f611ada591072cd2977cf948d4556ffca32164af1cf1b8d4f181");

        assertNull(ClientRequestParser.parse(httpRequest, requestBody));
    }
}
