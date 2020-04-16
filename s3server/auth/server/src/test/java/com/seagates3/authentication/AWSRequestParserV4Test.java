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
 * Original creation date: 26-Dec-2016
 */
package com.seagates3.authentication;

import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.exception.InvalidTokenException;

import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import io.netty.handler.codec.http.DefaultHttpHeaders;
import io.netty.handler.codec.http.FullHttpRequest;
import io.netty.handler.codec.http.HttpHeaders;
import io.netty.handler.codec.http.HttpMethod;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.powermock.api.mockito.PowerMockito;
import org.powermock.api.mockito.mockpolicies.Slf4jMockPolicy;
import org.powermock.core.classloader.annotations.MockPolicy;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;

import java.util.Map;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyString;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.when;

@RunWith(PowerMockRunner.class)
@PrepareForTest(AuthServerConfig.class)
@MockPolicy(Slf4jMockPolicy.class)
public class AWSRequestParserV4Test {
    private AWSRequestParserV4 awsRequestParserV4;

    @Before
    public void setUp() {
        awsRequestParserV4 = new AWSRequestParserV4();
    }

    @Test(expected = NullPointerException.class)
    public void parseTest_ShouldThrowExceptionIfNullParamPassed() throws InvalidTokenException {
        // Arrange
        FullHttpRequest fullHttpRequest = mock(FullHttpRequest.class);

        // Act
        awsRequestParserV4.parse(fullHttpRequest);
    }

    @Test
    public void parseTest_ForFullHttpRequest() throws Exception {
       // Arrange
        FullHttpRequest fullHttpRequest = mock(FullHttpRequest.class);
        HttpHeaders httpHeaders = new DefaultHttpHeaders();
        httpHeaders.add("host", "s3.seagate.com");
        httpHeaders.add("authorization", "AWS4-HMAC-SHA256 Credential=" +
                "AKIAIOSFODNN7EXAMPLE/20160321/US/s3/aws4_request,SignedHeaders=host;" +
                "x-amz-content-sha256;x-amz-date,Signature=b9d04cd83010685a1085ece386" +
                "57125c02a6f29093f5bd21dcd6e717f1996852");

        when(fullHttpRequest.headers()).thenReturn(httpHeaders);
        when(fullHttpRequest.getMethod()).thenReturn(HttpMethod.GET);
        when(fullHttpRequest.getUri()).thenReturn("/");

        StringBuilder params = new StringBuilder();
        params.append("Action=IAMAction")
                .append("Version=SomeValue");
        ByteBuf buffer =  Unpooled.wrappedBuffer(params.toString().getBytes());
        when(fullHttpRequest.content()).thenReturn(buffer);

        String[] endpoints = {"s3-us.seagate.com", "s3-europe.seagate.com",
                "s3-asia.seagate.com"};
        PowerMockito.mockStatic(AuthServerConfig.class);
        PowerMockito.doReturn(endpoints).when(AuthServerConfig.class, "getEndpoints");

        // Act
        ClientRequestToken clientRequestToken = awsRequestParserV4.parse(fullHttpRequest);

        // Verify
        assertEquals("GET", clientRequestToken.getHttpMethod().toString());
        assertEquals(ClientRequestToken.AWSSigningVersion.V4, clientRequestToken.getSignVersion());
    }

    @Test
    public void parseTest_ForRequestBody() throws InvalidTokenException {
        // Arrange
        Map<String, String> requestBody = mock(Map.class);

        AWSRequestParserV4 awsRequestParserV4Spy = spy(awsRequestParserV4);
        doNothing().when(awsRequestParserV4Spy).parseRequestHeaders(any(Map.class), any(ClientRequestToken.class));
        doNothing().when(awsRequestParserV4Spy).authHeaderParser(anyString(), any(ClientRequestToken.class));

        // Act && Verify
        assertTrue(awsRequestParserV4Spy.parse(requestBody) instanceof ClientRequestToken);
    }

    @Test
    public void authHeaderParserTest() throws InvalidTokenException {
        // Arrange
        String authorizationHeaderValue = "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/" +
                "20160321/US/s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;x-amz-date," +
                "Signature=b9d04cd83010685a1085ece38657125c02a6f29093f5bd21dcd6e717f1996852";
        ClientRequestToken clientRequestToken = new ClientRequestToken();

        // Act
        awsRequestParserV4.authHeaderParser(authorizationHeaderValue, clientRequestToken);

        // Verify
        assertEquals("AKIAIOSFODNN7EXAMPLE", clientRequestToken.accessKeyId);
        assertEquals("b9d04cd83010685a1085ece38657125c02a6f29093f5bd21dcd6e717f1996852",
                clientRequestToken.getSignature());
    }

    @Test(expected = InvalidTokenException.class)
    public void authHeaderParserTest_InvalidAuthorizationHeader() throws InvalidTokenException {
        // Arrange
        String authorizationHeaderValue = "AWS4-HMAC-SHA256Credential=" +
                "AKIAIOSFODNN7EXAMPLE/20160321/US/s3/aws4_request,SignedHeaders=host;" +
                "x-amz-content-sha256;x-amz-date,Signature=b9d04cd83010685a1085ece386" +
                "57125c02a6f29093f5bd21dcd6e717f1996852";;
        ClientRequestToken clientRequestToken = new ClientRequestToken();

        // Act
        awsRequestParserV4.authHeaderParser(authorizationHeaderValue, clientRequestToken);
    }

    @Test(expected = NullPointerException.class)
    public void authHeaderParserTest_NULLAuthorizationHeader() throws InvalidTokenException {
        // Arrange
        String authorizationHeaderValue = null;
        ClientRequestToken clientRequestToken = new ClientRequestToken();

        // Act
        awsRequestParserV4.authHeaderParser(authorizationHeaderValue, clientRequestToken);
    }

    @Test(expected = InvalidTokenException.class)
    public void authHeaderParserTest_InvalidAuthorizationHeader_V4() throws InvalidTokenException {
        // Arrange
        String authorizationHeaderValue = "AWS AKIAJTYX36YCKQSAJT7Q:uDWiVvxwCUR9YJ8EGJgbtW9tjFM=";
        ClientRequestToken clientRequestToken = new ClientRequestToken();

        // Act
        awsRequestParserV4.authHeaderParser(authorizationHeaderValue, clientRequestToken);
    }
}
