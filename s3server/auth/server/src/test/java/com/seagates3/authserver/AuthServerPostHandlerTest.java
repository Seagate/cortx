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
 * Original creation date: 24-Jan-2017
 */

package com.seagates3.authserver;

import com.seagates3.controller.FaultPointsController;
import com.seagates3.controller.IAMController;
import com.seagates3.controller.SAMLWebSSOController;
import com.seagates3.response.ServerResponse;
import io.netty.buffer.ByteBuf;
import io.netty.buffer.ByteBufUtil;
import io.netty.buffer.Unpooled;
import io.netty.channel.ChannelFuture;
import io.netty.channel.ChannelFutureListener;
import io.netty.channel.ChannelHandlerContext;
import io.netty.handler.codec.http.DefaultFullHttpRequest;
import io.netty.handler.codec.http.DefaultFullHttpResponse;
import io.netty.handler.codec.http.FullHttpRequest;
import io.netty.handler.codec.http.FullHttpResponse;
import io.netty.handler.codec.http.HttpHeaders;
import io.netty.handler.codec.http.HttpMethod;
import io.netty.handler.codec.http.HttpResponseStatus;
import io.netty.handler.codec.http.HttpVersion;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.powermock.api.mockito.mockpolicies.Slf4jMockPolicy;
import org.powermock.core.classloader.annotations.MockPolicy;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;
import org.powermock.reflect.internal.WhiteboxImpl;

import java.util.Map;
import java.util.TreeMap;

import static org.hamcrest.CoreMatchers.containsString;
import static org.junit.Assert.*;
import static org.mockito.Matchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.powermock.api.mockito.PowerMockito.mockStatic;
import static org.powermock.api.mockito.PowerMockito.verifyPrivate;
import static org.powermock.api.mockito.PowerMockito.when;
import static org.powermock.api.mockito.PowerMockito.whenNew;

@RunWith(PowerMockRunner.class)
@PrepareForTest({AuthServerPostHandler.class, HttpHeaders.class, AuthServerConfig.class})
@MockPolicy(Slf4jMockPolicy.class)
public class AuthServerPostHandlerTest {

    private ChannelHandlerContext ctx;
    private FullHttpRequest fullHttpRequest;
    private AuthServerPostHandler handler;

    @Before
    public void setUp() throws Exception {
        mockStatic(AuthServerConfig.class);
        ctx = mock(ChannelHandlerContext.class);
        when(AuthServerConfig.getReqId()).thenReturn("0000");
        fullHttpRequest = new DefaultFullHttpRequest(
                HttpVersion.HTTP_1_1, HttpMethod.POST, "/", getRequestBodyAsByteBuf());
    }

    @Test
    public void runTest_ServeIamRequest() throws Exception {
        handler = new AuthServerPostHandler(ctx, fullHttpRequest);
        handler.run();

        verifyPrivate(handler).invoke("serveIamRequest", getExpectedBody());
    }

    @Test
    public void runTest_ServeFiRequest() throws Exception {
        fullHttpRequest = new DefaultFullHttpRequest(
                HttpVersion.HTTP_1_1, HttpMethod.POST, "/", getFiRequestBodyAsByteBuf());
        handler = new AuthServerPostHandler(ctx, fullHttpRequest);
        handler.run();
    }

    @Test
    public void runTest_SAML_Mock() throws Exception {
        Map<String, String> requestBody = getExpectedBody();
        FullHttpResponse response = mock(FullHttpResponse.class);
        SAMLWebSSOController controller = mock(SAMLWebSSOController.class);
        HttpHeaders httpHeaders = mock(HttpHeaders.class);
        whenNew(SAMLWebSSOController.class).withArguments(requestBody).thenReturn(controller);
        when(controller.samlSignIn()).thenReturn(response);
        when(response.headers()).thenReturn(httpHeaders);
        fullHttpRequest.setUri("/saml");

        handler = new AuthServerPostHandler(ctx, fullHttpRequest);
        handler.run();

        verifyPrivate(handler).invoke("returnHTTPResponse", response);
    }

    @Test
    public void returnHTTPResponseTest_ServerResponse_KeepAlive() throws Exception {
        ServerResponse requestResponse = mock(ServerResponse.class);
        String responseBody = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>";
        when(requestResponse.getResponseBody()).thenReturn(responseBody);
        when(requestResponse.getResponseStatus()).thenReturn(HttpResponseStatus.OK);

        handler = new AuthServerPostHandler(ctx, fullHttpRequest);
        WhiteboxImpl.invokeMethod(handler, "returnHTTPResponse", requestResponse);

        verify(ctx).writeAndFlush(any(ServerResponse.class));
    }

    @Test
    public void returnHTTPResponseTest_ServerResponse() throws Exception {
        String responseBody = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>";
        mockStatic(HttpHeaders.class);
        ServerResponse requestResponse = mock(ServerResponse.class);
        ChannelFuture channelFuture = mock(ChannelFuture.class);
        when(requestResponse.getResponseBody()).thenReturn(responseBody);
        when(requestResponse.getResponseStatus()).thenReturn(HttpResponseStatus.OK);
        when(HttpHeaders.isKeepAlive(fullHttpRequest)).thenReturn(Boolean.FALSE);
        when(ctx.write(any(ServerResponse.class))).thenReturn(channelFuture);

        handler = new AuthServerPostHandler(ctx, fullHttpRequest);
        WhiteboxImpl.invokeMethod(handler, "returnHTTPResponse", requestResponse);

        verify(ctx).write(any(ServerResponse.class));
        verify(channelFuture).addListener(ChannelFutureListener.CLOSE);
    }

    @Test
    public void returnHTTPResponseTest_FullHttpResponse_KeepAlive() throws Exception {
        FullHttpResponse response = new DefaultFullHttpResponse(HttpVersion.HTTP_1_1,
                HttpResponseStatus.OK);

        handler = new AuthServerPostHandler(ctx, fullHttpRequest);
        WhiteboxImpl.invokeMethod(handler, "returnHTTPResponse", response);

        verify(ctx).writeAndFlush(response);
    }

    @Test
    public void returnHTTPResponseTest_FullHttpResponse() throws Exception {
        FullHttpResponse response = new DefaultFullHttpResponse(HttpVersion.HTTP_1_1,
                HttpResponseStatus.OK);
        mockStatic(HttpHeaders.class);
        ChannelFuture channelFuture = mock(ChannelFuture.class);
        when(HttpHeaders.isKeepAlive(fullHttpRequest)).thenReturn(Boolean.FALSE);
        when(ctx.write(any(ServerResponse.class))).thenReturn(channelFuture);

        handler = new AuthServerPostHandler(ctx, fullHttpRequest);
        WhiteboxImpl.invokeMethod(handler, "returnHTTPResponse", response);

        verify(ctx).write(response);
        verify(channelFuture).addListener(ChannelFutureListener.CLOSE);
    }

    @Test
    public void getHttpRequestBodyAsMapTest() throws Exception {
        handler = new AuthServerPostHandler(ctx, fullHttpRequest);
        Map<String, String> result = WhiteboxImpl.invokeMethod(handler,
                "getHttpRequestBodyAsMap");

        assertEquals(getExpectedBody(), result);
    }

    @Test
    public void isFiRequestTest() throws Exception {
        handler = new AuthServerPostHandler(ctx, fullHttpRequest);
        Boolean result = WhiteboxImpl.invokeMethod(handler, "isFiRequest", "Authenticate");

        assertFalse(result);
    }

    @Test
    public void isFiRequestTest_True() throws Exception {
        handler = new AuthServerPostHandler(ctx, fullHttpRequest);
        Boolean result = WhiteboxImpl.invokeMethod(handler, "isFiRequest", "ResetFault");

        assertTrue(result);
    }

    @Test
    public void serveFiRequestTest_FiDisabled_BadRequest() throws Exception {
        handler = new AuthServerPostHandler(ctx, fullHttpRequest);
        ServerResponse result = WhiteboxImpl.invokeMethod(handler, "serveFiRequest",
                getExpectedBody());

        assertEquals(HttpResponseStatus.BAD_REQUEST, result.getResponseStatus());
        assertThat(result.getResponseBody(), containsString("<Code>BadRequest</Code>"));
    }

    @Test
    public void serveFiRequestTest_FiEnabled_InjectFault() throws Exception {
        Map<String, String> requestMap = getExpectedBody();
        requestMap.put("Action", "InjectFault");
        FaultPointsController faultPointsController = mock(FaultPointsController.class);
        whenNew(FaultPointsController.class).withNoArguments().thenReturn(faultPointsController);
        when(AuthServerConfig.isFaultInjectionEnabled()).thenReturn(Boolean.TRUE);

        handler = new AuthServerPostHandler(ctx, fullHttpRequest);
        WhiteboxImpl.invokeMethod(handler, "serveFiRequest", requestMap);

        verify(faultPointsController).set(requestMap);
        verify(faultPointsController, times(0)).reset(requestMap);
    }

    @Test
    public void serveFiRequestTest_FiEnabled_ResetFault() throws Exception {
        Map<String, String> requestMap = getExpectedBody();
        requestMap.put("Action", "ResetFault");
        FaultPointsController faultPointsController = mock(FaultPointsController.class);
        whenNew(FaultPointsController.class).withNoArguments().thenReturn(faultPointsController);
        when(AuthServerConfig.isFaultInjectionEnabled()).thenReturn(Boolean.TRUE);

        handler = new AuthServerPostHandler(ctx, fullHttpRequest);
        WhiteboxImpl.invokeMethod(handler, "serveFiRequest", requestMap);

        verify(faultPointsController).reset(requestMap);
        verify(faultPointsController, times(0)).set(requestMap);
    }

    @Test
    public void serveIamRequestTest() throws Exception {
        Map<String, String> requestMap = null;
        IAMController iamController = mock(IAMController.class);
        whenNew(IAMController.class).withNoArguments().thenReturn(iamController);

        handler = new AuthServerPostHandler(ctx, fullHttpRequest);
        WhiteboxImpl.invokeMethod(handler, "serveIamRequest", requestMap);

        verify(iamController).serve(fullHttpRequest, requestMap);
    }

    private ByteBuf getRequestBodyAsByteBuf() {
        String params = "Action=AuthenticateUser&" +
                "x-amz-meta-ics.meta-version=1&" +
                "x-amz-meta-ics.meta-version=2&" +
                "samlAssertion=assertion";
        ByteBuf byteBuf = Unpooled.buffer(params.length());
        ByteBufUtil.writeUtf8(byteBuf, params);

        return byteBuf;
    }

    private ByteBuf getFiRequestBodyAsByteBuf() {
        String params = "Action=InjectFault&" +
                "Mode=FAIL_ONCE&" +
                "Value=1";
        ByteBuf byteBuf = Unpooled.buffer(params.length());
        ByteBufUtil.writeUtf8(byteBuf, params);

        return byteBuf;
    }

    private Map<String, String> getExpectedBody() {
        Map<String, String> expectedRequestBody = new TreeMap<>(String.CASE_INSENSITIVE_ORDER);
        expectedRequestBody.put("Action", "AuthenticateUser");
        expectedRequestBody.put("samlAssertion", "assertion");
        expectedRequestBody.put("x-amz-meta-ics.meta-version", "1,2");

        return expectedRequestBody;
    }
}