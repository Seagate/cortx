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
 * Original creation date: 24-Dec-2015
 */

package com.seagates3.authserver;

import io.netty.channel.ChannelHandlerContext;
import io.netty.handler.codec.http.FullHttpRequest;
import io.netty.handler.codec.http.HttpMethod;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import org.powermock.api.mockito.PowerMockito;
import static org.powermock.api.mockito.PowerMockito.whenNew;

import org.powermock.api.mockito.mockpolicies.Slf4jMockPolicy;
import org.powermock.core.classloader.annotations.MockPolicy;
import org.powermock.core.classloader.annotations.PowerMockIgnore;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;
import org.powermock.reflect.internal.WhiteboxImpl;

@RunWith(PowerMockRunner.class)
@PrepareForTest({AuthServerHandler.class, AuthServerConfig.class})
@PowerMockIgnore({"javax.management.*"})
@MockPolicy(Slf4jMockPolicy.class)
public class AuthServerHandlerTest {

    private AuthServerHandler testHandler;
    private ChannelHandlerContext ctx;
    private FullHttpRequest httpRequest;

    @BeforeClass
    public static void setUpStatic() throws Exception {
        PowerMockito.mockStatic(AuthServerConfig.class);
        PowerMockito.doReturn(false).when(AuthServerConfig.class,
                "isPerfEnabled");
        PowerMockito.doReturn("/tmp/perf.log").when(AuthServerConfig.class,
                "getPerfLogFile");
    }

    @Before
    public void setUp() {
        ctx = Mockito.mock(ChannelHandlerContext.class);
        httpRequest = mock(FullHttpRequest.class);
        testHandler = new AuthServerHandler();
    }

    @Test
    public void channelReadTest() throws Exception {
        AuthServerPostHandler postHandler = mock(AuthServerPostHandler.class);
        when(httpRequest.getMethod()).thenReturn(HttpMethod.POST);
        whenNew(AuthServerPostHandler.class).withArguments(
                ctx, httpRequest).thenReturn(postHandler);

        testHandler.channelRead(ctx, httpRequest);
        Mockito.verify(postHandler).run();
    }

    @Test
    public void channelReadTest_GetRequest() throws Exception {
        AuthServerGetHandler getHandler = mock(AuthServerGetHandler.class);
        when(httpRequest.getMethod()).thenReturn(HttpMethod.GET);
        whenNew(AuthServerGetHandler.class)
                .withArguments(ctx, httpRequest).thenReturn(getHandler);

        testHandler.channelRead(ctx, httpRequest);

        Mockito.verify(getHandler).run();
    }

    @Test
    public void channelReadCompleteTest() {
        testHandler.channelReadComplete(ctx);

        verify(ctx).flush();
    }

    @Test
    public void exceptionCaughtTest() {
        Throwable cause = mock(Throwable.class);

        testHandler.exceptionCaught(ctx, cause);

        verify(ctx).close();
    }

    @Test
    public void createPostHandlerTest() throws Exception {
        AuthServerPostHandler postHandler = mock(AuthServerPostHandler.class);
        when(httpRequest.getMethod()).thenReturn(HttpMethod.POST);
        whenNew(AuthServerPostHandler.class).withArguments(
                ctx, httpRequest).thenReturn(postHandler);

        AuthServerPostHandler result = WhiteboxImpl.invokeMethod(
                testHandler, "createPostHandler", ctx, httpRequest);

        assertEquals(postHandler, result);
    }
}
