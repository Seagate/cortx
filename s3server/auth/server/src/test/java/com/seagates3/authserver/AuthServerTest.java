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
 * Original creation date: 03-Jan-2017
 */

package com.seagates3.authserver;

import com.seagates3.dao.DAODispatcher;
import com.seagates3.exception.ServerInitialisationException;
import com.seagates3.fi.FaultPoints;
import com.seagates3.perf.S3Perf;
import io.netty.bootstrap.ServerBootstrap;
import io.netty.channel.Channel;
import io.netty.channel.ChannelFuture;
import io.netty.channel.ChannelOption;
import io.netty.channel.EventLoopGroup;
import io.netty.channel.socket.nio.NioServerSocketChannel;
import io.netty.handler.logging.LogLevel;
import io.netty.handler.logging.LoggingHandler;
import io.netty.util.concurrent.EventExecutorGroup;
import org.apache.logging.log4j.Level;
import org.apache.logging.log4j.core.LoggerContext;
import org.apache.logging.log4j.core.config.ConfigurationSource;
import org.apache.logging.log4j.core.config.Configurator;
import org.junit.Before;
import org.junit.Test;
import org.junit.Rule;
import org.junit.contrib.java.lang.system.ExpectedSystemExit;
import org.junit.rules.TestRule;
import org.junit.contrib.java.lang.system.SystemOutRule;
import org.junit.runner.RunWith;
import org.powermock.api.mockito.mockpolicies.Slf4jMockPolicy;
import org.powermock.core.classloader.annotations.MockPolicy;
import org.powermock.core.classloader.annotations.PowerMockIgnore;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;

import org.powermock.reflect.internal.WhiteboxImpl;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.nio.file.Paths;
import java.util.Properties;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.powermock.api.mockito.PowerMockito.doNothing;
import static org.powermock.api.mockito.PowerMockito.doReturn;
import static org.powermock.api.mockito.PowerMockito.doThrow;
import static org.powermock.api.mockito.PowerMockito.mockStatic;
import static org.powermock.api.mockito.PowerMockito.spy;
import static org.powermock.api.mockito.PowerMockito.verifyPrivate;
import static org.powermock.api.mockito.PowerMockito.verifyStatic;
import static org.powermock.api.mockito.PowerMockito.whenNew;

@RunWith(PowerMockRunner.class)
@PrepareForTest({SSLContextProvider.class, IAMResourceMapper.class, DAODispatcher.class,
        S3Perf.class, AuthServerConfig.class, LoggerFactory.class, AuthServer.class,
        Paths.class, Configurator.class, FaultPoints.class})
@MockPolicy(Slf4jMockPolicy.class)
@PowerMockIgnore({"javax.management.*"})

public class AuthServerTest {

    @Rule
    public final ExpectedSystemExit exit = ExpectedSystemExit.none();
    public final SystemOutRule systemOutRule = new SystemOutRule().enableLog();
    private int bossGroupThreads = 1;
    private int workerGroupThreads = 2;
    private int eventExecutorThreads = 4;
    private int httpPort = 9085;
    private int httpsPort = 9086;
    private boolean https = false;
    private boolean http = true;
    private boolean enableFaultInjection = false;
    private String defaultHost= "0.0.0.0";
    private String logLevel = "DEBUG";
    private String logConfigFilePath = "/path/to/log/config/file";

    private EventLoopGroup bossGroup;
    private EventLoopGroup workerGroup;
    private EventExecutorGroup executorGroup;
    private Channel serverChannel;
    private ChannelFuture channelFuture;

    @Before
    public void setUp() throws Exception {
        bossGroup = mock(EventLoopGroup.class);
        workerGroup = mock(EventLoopGroup.class);
        executorGroup = mock(EventExecutorGroup.class);

        mockStatic(AuthServerConfig.class);
        doReturn(enableFaultInjection).when(AuthServerConfig.class, "isFaultInjectionEnabled");
        doReturn(bossGroupThreads).when(AuthServerConfig.class, "getBossGroupThreads");
        doReturn(workerGroupThreads).when(AuthServerConfig.class, "getWorkerGroupThreads");
        doReturn(eventExecutorThreads).when(AuthServerConfig.class, "getEventExecutorThreads");
        doReturn(httpPort).when(AuthServerConfig.class, "getHttpPort");
        doReturn(http).when(AuthServerConfig.class, "isHttpEnabled");
        doReturn(https).when(AuthServerConfig.class, "isHttpsEnabled");
        doReturn(httpsPort).when(AuthServerConfig.class, "getHttpsPort");
        doReturn(defaultHost).when(AuthServerConfig.class, "getDefaultHost");
        doReturn(logConfigFilePath).when(AuthServerConfig.class, "getLogConfigFile");
    }

    @Test
    public void mainTest() throws Exception {
        mainTestHelper();

        AuthServer.main(new String[]{});

        verifyStatic();
        SSLContextProvider.init();
        verifyStatic();
        IAMResourceMapper.init();
        verifyStatic();
        DAODispatcher.init();
        verifyStatic();
        S3Perf.init();
        verifyStatic();
        AuthServerConfig.readConfig(AuthServerConstants.RESOURCE_DIR);

        verifyPrivate(AuthServer.class).invoke("logInit");
        AuthServerConfig.loadCredentials();
        verifyPrivate(AuthServer.class).invoke("attachShutDownHook");

        verify(serverChannel).closeFuture();
        verify(channelFuture).sync();
    }

    @Test
    public void mainTest_HttpAndHttpsEnabled() throws Exception {
        mainTestHelper();
        doReturn(Boolean.TRUE).when(AuthServerConfig.class, "isHttpsEnabled");
        doReturn(Boolean.TRUE).when(AuthServerConfig.class, "isHttpEnabled");
        doReturn(serverChannel).when(AuthServer.class, "httpsServerBootstrap",
                any(EventLoopGroup.class), any(EventLoopGroup.class),
                any(EventExecutorGroup.class), any(String.class), anyInt());

        AuthServer.main(new String[]{});

        verifyStatic();
        SSLContextProvider.init();
        verifyStatic();
        IAMResourceMapper.init();
        verifyStatic();
        DAODispatcher.init();
        verifyStatic();
        S3Perf.init();
        verifyStatic();
        AuthServerConfig.readConfig(AuthServerConstants.RESOURCE_DIR);

        verifyPrivate(AuthServer.class).invoke("logInit");
        AuthServerConfig.loadCredentials();
        verifyPrivate(AuthServer.class).invoke("attachShutDownHook");

        verify(serverChannel, times(2)).closeFuture();
        verify(channelFuture, times(2)).sync();
    }

    @Test
    public void mainTest_HttpEnabledAndHttpsDisabled() throws Exception {
        mainTestHelper();
        doReturn(Boolean.FALSE).when(AuthServerConfig.class, "isHttpsEnabled");
        doReturn(Boolean.TRUE).when(AuthServerConfig.class, "isHttpEnabled");
        doReturn(serverChannel).when(AuthServer.class, "httpServerBootstrap",
                any(EventLoopGroup.class), any(EventLoopGroup.class),
                any(EventExecutorGroup.class), any(String.class), anyInt());

        AuthServer.main(new String[]{});

        verifyStatic();
        SSLContextProvider.init();
        verifyStatic();
        IAMResourceMapper.init();
        verifyStatic();
        DAODispatcher.init();
        verifyStatic();
        S3Perf.init();
        verifyStatic();
        AuthServerConfig.readConfig(AuthServerConstants.RESOURCE_DIR);

        verifyPrivate(AuthServer.class).invoke("logInit");
        AuthServerConfig.loadCredentials();
        verifyPrivate(AuthServer.class).invoke("attachShutDownHook");

        verify(serverChannel, times(1)).closeFuture();
        verify(channelFuture, times(1)).sync();
    }


    @Test
    public void mainTest_HttpDisabledAndHttpsEnabled() throws Exception {
        mainTestHelper();
        doReturn(Boolean.FALSE).when(AuthServerConfig.class, "isHttpEnabled");
        doReturn(Boolean.TRUE).when(AuthServerConfig.class, "isHttpsEnabled");
        doReturn(serverChannel).when(AuthServer.class, "httpsServerBootstrap",
                any(EventLoopGroup.class), any(EventLoopGroup.class),
                any(EventExecutorGroup.class), any(String.class), anyInt());

        AuthServer.main(new String[]{});

        verifyStatic();
        AuthServerConfig.readConfig(AuthServerConstants.RESOURCE_DIR);
        verifyStatic();
        SSLContextProvider.init();
        verifyStatic();
        IAMResourceMapper.init();
        verifyStatic();
        DAODispatcher.init();
        verifyStatic();
        S3Perf.init();

        verifyPrivate(AuthServer.class).invoke("logInit");
        AuthServerConfig.loadCredentials();
        verifyPrivate(AuthServer.class).invoke("attachShutDownHook");

        verify(serverChannel, times(1)).closeFuture();
        verify(channelFuture, times(1)).sync();
    }

    @Test
    public void mainTest_HttpAndHttpsDisabled() throws Exception {
        mainTestHelper();
        doReturn(Boolean.FALSE).when(AuthServerConfig.class, "isHttpEnabled");
        doReturn(Boolean.FALSE).when(AuthServerConfig.class, "isHttpsEnabled");

        exit.expectSystemExitWithStatus(1);
        AuthServer.main(new String[]{});
        assertEquals("Both HTTP and HTTPS are disabled. At least one channel should be enabled.", systemOutRule.getLog());

    }

    @Test
    public void mainTest_HttpAndHttpsEnabled_FiEnabled() throws Exception {
        mainTestHelper();
        doReturn(Boolean.TRUE).when(AuthServerConfig.class, "isHttpsEnabled");
        doReturn(serverChannel).when(AuthServer.class, "httpsServerBootstrap",
                any(EventLoopGroup.class), any(EventLoopGroup.class),
                any(EventExecutorGroup.class),any(String.class), anyInt());
        doReturn(Boolean.TRUE).when(AuthServerConfig.class, "isFaultInjectionEnabled");

        AuthServer.main(new String[]{});

        verifyStatic();
        AuthServerConfig.readConfig(AuthServerConstants.RESOURCE_DIR);
        verifyStatic();
        FaultPoints.init();
        verifyStatic();
        SSLContextProvider.init();
        verifyStatic();
        IAMResourceMapper.init();
        verifyStatic();
        DAODispatcher.init();
        verifyStatic();
        S3Perf.init();

        verifyPrivate(AuthServer.class).invoke("logInit");
        AuthServerConfig.loadCredentials();
        verifyPrivate(AuthServer.class).invoke("attachShutDownHook");

        verify(serverChannel, times(2)).closeFuture();
        verify(channelFuture, times(2)).sync();
    }

    @Test
    public void mainTest_HttpEnabledAndHttpsDisabled_FiEnabled() throws Exception {
        mainTestHelper();
        doReturn(Boolean.FALSE).when(AuthServerConfig.class, "isHttpsEnabled");
        doReturn(Boolean.TRUE).when(AuthServerConfig.class, "isHttpEnabled");
        doReturn(serverChannel).when(AuthServer.class, "httpsServerBootstrap",
                any(EventLoopGroup.class), any(EventLoopGroup.class),
                any(EventExecutorGroup.class), any(String.class), anyInt());
        doReturn(Boolean.TRUE).when(AuthServerConfig.class, "isFaultInjectionEnabled");

        AuthServer.main(new String[]{});

        verifyStatic();
        AuthServerConfig.readConfig(AuthServerConstants.RESOURCE_DIR);
        verifyStatic();
        FaultPoints.init();
        verifyStatic();
        SSLContextProvider.init();
        verifyStatic();
        IAMResourceMapper.init();
        verifyStatic();
        DAODispatcher.init();
        verifyStatic();
        S3Perf.init();

        verifyPrivate(AuthServer.class).invoke("logInit");
        AuthServerConfig.loadCredentials();
        verifyPrivate(AuthServer.class).invoke("attachShutDownHook");

        verify(serverChannel, times(1)).closeFuture();
        verify(channelFuture, times(1)).sync();
    }

    @Test
    public void mainTest_HttpDisabledAndHttpsEnabled_FiEnabled() throws Exception {
        mainTestHelper();
        doReturn(Boolean.FALSE).when(AuthServerConfig.class, "isHttpEnabled");
        doReturn(Boolean.TRUE).when(AuthServerConfig.class, "isHttpsEnabled");
        doReturn(serverChannel).when(AuthServer.class, "httpsServerBootstrap",
                any(EventLoopGroup.class), any(EventLoopGroup.class),
                any(EventExecutorGroup.class), any(String.class), anyInt());
        doReturn(Boolean.TRUE).when(AuthServerConfig.class, "isFaultInjectionEnabled");

        AuthServer.main(new String[]{});

        verifyStatic();
        FaultPoints.init();
        verifyStatic();
        SSLContextProvider.init();
        verifyStatic();
        IAMResourceMapper.init();
        verifyStatic();
        DAODispatcher.init();
        verifyStatic();
        S3Perf.init();
        verifyStatic();
        AuthServerConfig.readConfig(AuthServerConstants.RESOURCE_DIR);

        verifyPrivate(AuthServer.class).invoke("logInit");
        AuthServerConfig.loadCredentials();
        verifyPrivate(AuthServer.class).invoke("attachShutDownHook");

        verify(serverChannel, times(1)).closeFuture();
        verify(channelFuture, times(1)).sync();
    }

    @Test
    public void mainTest_HttpAndHttpsDisabled_FiEnabled() throws Exception {
        mainTestHelper();
        doReturn(Boolean.FALSE).when(AuthServerConfig.class, "isHttpEnabled");
        doReturn(Boolean.FALSE).when(AuthServerConfig.class, "isHttpsEnabled");
        doReturn(Boolean.TRUE).when(AuthServerConfig.class, "isFaultInjectionEnabled");

        exit.expectSystemExitWithStatus(1);
        AuthServer.main(new String[]{});
        assertEquals("Both HTTP and HTTPS are disabled. At least one channel should be enabled.", systemOutRule.getLog()); 

    }

    private  void mainTestHelper() throws Exception {
        spy(AuthServer.class);

        doNothing().when(AuthServer.class, "logInit");
        doNothing().when(AuthServer.class, "attachShutDownHook");

        mockStatic(SSLContextProvider.class);
        mockStatic(IAMResourceMapper.class);
        mockStatic(DAODispatcher.class);
        mockStatic(S3Perf.class);
        mockStatic(FaultPoints.class);
        doNothing().when(SSLContextProvider.class, "init");
        doNothing().when(IAMResourceMapper.class, "init");
        doNothing().when(DAODispatcher.class, "init");
        doNothing().when(S3Perf.class, "init");
        doNothing().when(FaultPoints.class, "init");

        serverChannel = mock(Channel.class);
        doReturn(serverChannel).when(AuthServer.class, "httpServerBootstrap",
                any(EventLoopGroup.class), any(EventLoopGroup.class),
                any(EventExecutorGroup.class), any(String.class), anyInt());

        channelFuture = mock(ChannelFuture.class);
        when(serverChannel.closeFuture()).thenReturn(channelFuture);
        when(channelFuture.sync()).thenReturn(channelFuture);
    }

    @Test
    public void logInitTest() throws Exception {
        File logConfigFile = mock(File.class);
        whenNew(File.class).withArguments(logConfigFilePath).thenReturn(logConfigFile);

        when(logConfigFile.exists()).thenReturn(Boolean.TRUE);

        FileInputStream fileInputStream = mock(FileInputStream.class);
        whenNew(FileInputStream.class).withArguments(logConfigFile).thenReturn(fileInputStream);
        ConfigurationSource source = mock(ConfigurationSource.class);
        whenNew(ConfigurationSource.class).withAnyArguments().thenReturn(source);

        mockStatic(Configurator.class);
        LoggerContext loggerContext = mock(LoggerContext.class);
        doReturn(loggerContext).when(Configurator.class, "initialize", null, source);

        doReturn(logLevel).when(AuthServerConfig.class, "getLogLevel");

        AuthServer.logInit();

        verifyStatic();
        Configurator.initialize(null, source);

        verifyStatic();
        Configurator.setRootLevel(Level.getLevel(logLevel));
    }

    @Test(expected = ServerInitialisationException.class)
    public void logInitTest_ShouldThrowExceptionIfLoggingConfFileNotExist() throws Exception {
        String logConfigFilePath = "/path/to/log/config/file";
        mockStatic(AuthServerConfig.class);
        doReturn(logConfigFilePath).when(AuthServerConfig.class, "getLogConfigFile");

        AuthServer.logInit();
    }

    @Test
    public void logInitTest_ShouldThrowExceptionIfLogLevelIsIncorrect() throws Exception {
        String logConfigFilePath = "/path/to/log/config/file";

        mockStatic(AuthServerConfig.class);
        doReturn(logConfigFilePath).when(AuthServerConfig.class, "getLogConfigFile");

        File logConfigFile = mock(File.class);
        whenNew(File.class).withArguments(logConfigFilePath).thenReturn(logConfigFile);

        when(logConfigFile.exists()).thenReturn(Boolean.TRUE);

        FileInputStream fileInputStream = mock(FileInputStream.class);
        whenNew(FileInputStream.class).withArguments(logConfigFile).thenReturn(fileInputStream);
        ConfigurationSource source = mock(ConfigurationSource.class);
        whenNew(ConfigurationSource.class).withAnyArguments().thenReturn(source);

        mockStatic(Configurator.class);
        LoggerContext loggerContext = mock(LoggerContext.class);
        doReturn(loggerContext).when(Configurator.class, "initialize", null, source);

        doReturn("INCORRECT_LOG_LEVEL").when(AuthServerConfig.class, "getLogLevel");

        try {
            AuthServer.logInit();
            fail("Expected ServerInitialisationException.");
        } catch (ServerInitialisationException e) {
            assertEquals("Incorrect logging level.", e.getMessage());
        }

        verifyStatic(times(0));
        Configurator.setRootLevel(any(Level.class));
    }

    @Test
    public void shutdownExecutorsTest() throws Exception {
        EventLoopGroup bossGroup = mock(EventLoopGroup.class);
        EventLoopGroup workerGroup = mock(EventLoopGroup.class);
        EventExecutorGroup executorGroup = mock(EventExecutorGroup.class);
        Logger logger = mock(Logger.class);

        WhiteboxImpl.setInternalState(AuthServer.class, "bossGroup", bossGroup);
        WhiteboxImpl.setInternalState(AuthServer.class, "workerGroup", workerGroup);
        WhiteboxImpl.setInternalState(AuthServer.class, "executorGroup", executorGroup);
        WhiteboxImpl.setInternalState(AuthServer.class, "logger", logger);

        WhiteboxImpl.invokeMethod(AuthServer.class, "shutdownExecutors");

        verifyPrivate(bossGroup).invoke("shutdownGracefully");
        verifyPrivate(workerGroup).invoke("shutdownGracefully");
        verifyPrivate(executorGroup).invoke("shutdownGracefully");
    }

    @Test
    public void httpServerBootstrapTest() throws Exception {
        EventLoopGroup bossGroup = mock(EventLoopGroup.class);
        EventLoopGroup workerGroup = mock(EventLoopGroup.class);
        EventExecutorGroup executorGroup = mock(EventExecutorGroup.class);
        int port = 80;
        String defaultHost = "0.0.0.0";

        ServerBootstrap serverBootstrap = mock(ServerBootstrap.class);
        whenNew(ServerBootstrap.class).withNoArguments().thenReturn(serverBootstrap);

        LoggingHandler handler = mock(LoggingHandler.class);
        whenNew(LoggingHandler.class).withArguments(LogLevel.INFO).thenReturn(handler);

        AuthServerHTTPInitializer initializer = mock(AuthServerHTTPInitializer.class);
        whenNew(AuthServerHTTPInitializer.class).withArguments(executorGroup)
                .thenReturn(initializer);

        when(serverBootstrap.option(ChannelOption.SO_BACKLOG, 1024)).thenReturn(serverBootstrap);
        when(serverBootstrap.group(bossGroup, workerGroup)).thenReturn(serverBootstrap);
        when(serverBootstrap.channel(NioServerSocketChannel.class)).thenReturn(serverBootstrap);
        when(serverBootstrap.handler(handler)).thenReturn(serverBootstrap);
        when(serverBootstrap.childHandler(initializer)).thenReturn(serverBootstrap);

        Channel serverChannel = mock(Channel.class);
        ChannelFuture channelFuture = mock(ChannelFuture.class);
        when(serverBootstrap.bind(defaultHost, port)).thenReturn(channelFuture);
        when(channelFuture.sync()).thenReturn(channelFuture);
        when(channelFuture.channel()).thenReturn(serverChannel);

        Object channel = WhiteboxImpl.invokeMethod(AuthServer.class,
                "httpServerBootstrap", bossGroup, workerGroup, executorGroup, defaultHost, port);

        assertTrue(channel instanceof Channel);
        verify(serverBootstrap).bind(defaultHost, port);
        verify(channelFuture).sync();
        verify(channelFuture).channel();
    }

    @Test
    public void httpsServerBootstrapTest() throws Exception {
        int port = 443;
        String defaultHost = "0.0.0.0";

        ServerBootstrap serverBootstrap = mock(ServerBootstrap.class);
        whenNew(ServerBootstrap.class).withNoArguments().thenReturn(serverBootstrap);

        LoggingHandler handler = mock(LoggingHandler.class);
        whenNew(LoggingHandler.class).withArguments(LogLevel.INFO).thenReturn(handler);

        AuthServerHTTPSInitializer initializer = mock(AuthServerHTTPSInitializer.class);
        whenNew(AuthServerHTTPSInitializer.class).withArguments(executorGroup)
                .thenReturn(initializer);

        when(serverBootstrap.option(ChannelOption.SO_BACKLOG, 1024)).thenReturn(serverBootstrap);
        when(serverBootstrap.group(bossGroup, workerGroup)).thenReturn(serverBootstrap);
        when(serverBootstrap.channel(NioServerSocketChannel.class)).thenReturn(serverBootstrap);
        when(serverBootstrap.handler(handler)).thenReturn(serverBootstrap);
        when(serverBootstrap.childHandler(initializer)).thenReturn(serverBootstrap);

        Channel serverChannel = mock(Channel.class);
        ChannelFuture channelFuture = mock(ChannelFuture.class);
        when(serverBootstrap.bind(defaultHost, port)).thenReturn(channelFuture);
        when(channelFuture.sync()).thenReturn(channelFuture);
        when(channelFuture.channel()).thenReturn(serverChannel);

        Object channel = WhiteboxImpl.invokeMethod(AuthServer.class,
                "httpsServerBootstrap", bossGroup, workerGroup, executorGroup, defaultHost, port);

        assertTrue(channel instanceof Channel);

        verify(serverBootstrap).bind(defaultHost, port);
        verify(channelFuture).sync();
        verify(channelFuture).channel();
    }

    @Test
    public void attachShutDownHookTest() throws Exception {
        mockStatic(S3Perf.class);
        doNothing().when(S3Perf.class, "clean");

        EventLoopGroup bossGroup = mock(EventLoopGroup.class);
        EventLoopGroup workerGroup = mock(EventLoopGroup.class);
        EventExecutorGroup executorGroup = mock(EventExecutorGroup.class);
        Logger logger = mock(Logger.class);

        WhiteboxImpl.setInternalState(AuthServer.class, "bossGroup", bossGroup);
        WhiteboxImpl.setInternalState(AuthServer.class, "workerGroup", workerGroup);
        WhiteboxImpl.setInternalState(AuthServer.class, "executorGroup", executorGroup);
        WhiteboxImpl.setInternalState(AuthServer.class, "logger", logger);

        WhiteboxImpl.invokeMethod(AuthServer.class, "attachShutDownHook");
    }
}
