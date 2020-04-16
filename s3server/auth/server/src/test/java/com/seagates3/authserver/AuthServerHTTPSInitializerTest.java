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
 * Original creation date: 01-Feb-2017
 */

package com.seagates3.authserver;

import io.netty.buffer.ByteBufAllocator;
import io.netty.channel.ChannelPipeline;
import io.netty.channel.socket.SocketChannel;
import io.netty.handler.ssl.SslContext;
import io.netty.handler.ssl.SslHandler;
import io.netty.util.concurrent.EventExecutorGroup;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.powermock.core.classloader.annotations.PowerMockIgnore;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;

import static org.mockito.Matchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.powermock.api.mockito.PowerMockito.mockStatic;
import static org.powermock.api.mockito.PowerMockito.verifyStatic;

@RunWith(PowerMockRunner.class)
@PrepareForTest({SSLContextProvider.class, SslContext.class,
        AuthServerHTTPSInitializer.class})
@PowerMockIgnore({"javax.management.*"})
public class AuthServerHTTPSInitializerTest {

    private EventExecutorGroup executorGroup;
    private SocketChannel socketChannel;
    private ChannelPipeline channelPipeline;
    private AuthServerHTTPSInitializer httpsInitializer;

    @Before
    public void setUp() throws Exception {
        executorGroup = mock(EventExecutorGroup.class);
        socketChannel = mock(SocketChannel.class);
        channelPipeline = mock(ChannelPipeline.class);
        when(socketChannel.pipeline()).thenReturn(channelPipeline);
    }

    @Test
    public void initChannelTest() {
        httpsInitializer = new AuthServerHTTPSInitializer(executorGroup);
        httpsInitializer.initChannel(socketChannel);

        verify(socketChannel).pipeline();
        verify(channelPipeline).addLast(any(EventExecutorGroup.class),
                any(AuthServerHandler.class));
    }

    @Test
    public void initChannelTest_SslContextNotNull() {
        mockStatic(SslContext.class);
        mockStatic(SSLContextProvider.class);
        SslContext sslContext = mock(SslContext.class);
        SslHandler sslHandler = mock(SslHandler.class);
        when(SSLContextProvider.getServerContext()).thenReturn(sslContext);
        when(sslContext.newHandler(any(ByteBufAllocator.class))).thenReturn(sslHandler);

        httpsInitializer = new AuthServerHTTPSInitializer(executorGroup);
        httpsInitializer.initChannel(socketChannel);

        verifyStatic();
        SSLContextProvider.getServerContext();
        verify(socketChannel).pipeline();
        verify(channelPipeline).addLast(sslHandler);
        verify(channelPipeline).addLast(any(EventExecutorGroup.class),
                any(AuthServerHandler.class));
    }
}