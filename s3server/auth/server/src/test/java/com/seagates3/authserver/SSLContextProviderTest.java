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

import com.seagates3.exception.ServerInitialisationException;

import io.netty.channel.EventLoopGroup;
import io.netty.handler.ssl.SslContext;
import io.netty.handler.ssl.SslContextBuilder;
import io.netty.util.concurrent.EventExecutorGroup;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.powermock.api.mockito.mockpolicies.Slf4jMockPolicy;
import org.powermock.core.classloader.annotations.MockPolicy;
import org.powermock.core.classloader.annotations.PowerMockIgnore;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;

import javax.net.ssl.KeyManagerFactory;
import javax.net.ssl.SSLException;

import java.nio.file.Path;
import java.nio.file.Paths;
import java.security.NoSuchAlgorithmException;

import static org.junit.Assert.*;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Matchers.anyString;
import static org.powermock.api.mockito.PowerMockito.doReturn;
import static org.powermock.api.mockito.PowerMockito.mock;
import static org.powermock.api.mockito.PowerMockito.mockStatic;
import static org.powermock.api.mockito.PowerMockito.when;

@RunWith(PowerMockRunner.class)
@PrepareForTest({AuthServerConfig.class, KeyManagerFactory.class, SslContextBuilder.class})
@MockPolicy(Slf4jMockPolicy.class)
@PowerMockIgnore({"javax.management.*"})
public class SSLContextProviderTest {

    private static Path filePath = Paths.get("..", "resources",
          "s3authserver.jks");

    @Before
    public void setUp() throws Exception {
        mockStatic(AuthServerConfig.class);
        when(AuthServerConfig.getHttpsPort()).thenReturn(9086);
        when(AuthServerConfig.isHttpsEnabled()).thenReturn(Boolean.TRUE);
    }

    @Test
    public void initTest_HttpsDisabled() throws ServerInitialisationException {
        when(AuthServerConfig.isHttpsEnabled()).thenReturn(Boolean.FALSE);

        when(AuthServerConfig.getKeyStorePath()).thenReturn(Paths.get("..",
            "resources", "s3authserver.jks"));
        SSLContextProvider.init();

        assertNull(SSLContextProvider.getServerContext());
    }

    @Test
    public void initTest_HttpsEnabled() throws ServerInitialisationException,
            NoSuchAlgorithmException, SSLException, Exception{
        mockStatic(KeyManagerFactory.class);
        mockStatic(SslContextBuilder.class);

        KeyManagerFactory kmf = mock(KeyManagerFactory.class);
        SslContextBuilder contextBuilder = mock(SslContextBuilder.class);
        SslContext sslContext = mock(SslContext.class);
        when(AuthServerConfig.getKeyStorePath()).thenReturn(Paths.get("..",
                "resources", "s3authserver.jks"));
        when(AuthServerConfig.getKeyStorePassword()).thenReturn("seagate");
        when(AuthServerConfig.getKeyPassword()).thenReturn("seagate");
        when(KeyManagerFactory.getInstance(anyString())).thenReturn(kmf);
        when(SslContextBuilder.forServer(kmf)).thenReturn(contextBuilder);

        when(contextBuilder.build()).thenReturn(sslContext);

        SSLContextProvider.init();

        assertEquals(sslContext, SSLContextProvider.getServerContext());
        assertEquals(9086, SSLContextProvider.getHttpsPort());
    }

    @Test(expected = ServerInitialisationException.class)
    public void initTest_HttpsEnabled_NoSuchAlgorithm()
                                    throws ServerInitialisationException {
        when(AuthServerConfig.getKeyStorePath()).thenReturn(Paths.get("..",
                "resources", "s3authserver.jks"));
        when(AuthServerConfig.getKeyStorePassword()).thenReturn("seagate");
        when(AuthServerConfig.getKeyPassword()).thenReturn("seagate");

        SSLContextProvider.init();
    }
}
