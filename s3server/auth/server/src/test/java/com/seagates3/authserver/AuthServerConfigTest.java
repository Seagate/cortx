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



import org.junit.Test;

import java.io.FileInputStream;
import java.io.IOException;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.security.GeneralSecurityException;
import java.util.Properties;

import static org.junit.Assert.*;
import static org.mockito.Matchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.powermock.api.mockito.PowerMockito.doNothing;
import static org.powermock.api.mockito.PowerMockito.doThrow;
import static org.powermock.api.mockito.PowerMockito.mockStatic;
import static org.powermock.api.mockito.PowerMockito.verifyStatic;
import static org.powermock.api.mockito.PowerMockito.whenNew;

public class AuthServerConfigTest {

    @Test
    public void initTest() throws Exception {

        Properties authServerConfig = getAuthProperties();
        AuthServerConfig.authResourceDir = "../resources";
        AuthServerConfig.init(authServerConfig);
        assertEquals("s3.seagate.com", AuthServerConfig.getDefaultEndpoint());

        assertEquals("resources/static/saml-metadata.xml",
                AuthServerConfig.getSAMLMetadataFilePath());

        assertEquals(9085, AuthServerConfig.getHttpPort());

        assertEquals(9086, AuthServerConfig.getHttpsPort());

        assertEquals("s3authserver.jks", AuthServerConfig.getKeyStoreName());

        assertEquals("seagate", AuthServerConfig.getKeyStorePassword());

        assertEquals("seagate", AuthServerConfig.getKeyPassword());

        assertTrue(AuthServerConfig.isHttpsEnabled());

        assertEquals("ldap", AuthServerConfig.getDataSource());

        assertEquals("127.0.0.1", AuthServerConfig.getLdapHost());

        assertEquals(389, AuthServerConfig.getLdapPort());

        assertEquals(636, AuthServerConfig.getLdapSSLPort());

        assertEquals(true, AuthServerConfig.isSSLToLdapEnabled());

        assertEquals(5, AuthServerConfig.getLdapMaxConnections());

        assertEquals(1, AuthServerConfig.getLdapMaxSharedConnections());

        assertEquals("cn=admin,dc=seagate,dc=com", AuthServerConfig.getLdapLoginDN());

        assertEquals("https://console.s3.seagate.com:9292/sso", AuthServerConfig.getConsoleURL());

        assertNull(AuthServerConfig.getLogConfigFile());

        assertNull(AuthServerConfig.getLogLevel());

        assertEquals(1, AuthServerConfig.getBossGroupThreads());

        assertEquals(2, AuthServerConfig.getWorkerGroupThreads());

        assertFalse(AuthServerConfig.isPerfEnabled());

        assertEquals("/var/log/seagate/auth/perf.log", AuthServerConfig.getPerfLogFile());

        assertEquals(4, AuthServerConfig.getEventExecutorThreads());

        assertFalse(AuthServerConfig.isFaultInjectionEnabled());

        String[] expectedEndPoints = {"s3-us-west-2.seagate.com", "s3-us.seagate.com",
                "s3-europe.seagate.com", "s3-asia.seagate.com"};
        assertArrayEquals(expectedEndPoints, AuthServerConfig.getEndpoints());

        Path keyStorePath =
             Paths.get("..", "resources", "s3authserver.jks");
        assertTrue(keyStorePath.toString().equals(
                        AuthServerConfig.getKeyStorePath().toString()));
        assertTrue(AuthServerConfig.isEnableHttpsToS3());
    }

    @Test
    public void loadCredentialsTest() throws GeneralSecurityException, Exception {
        Properties authServerConfig = getAuthProperties();
        AuthServerConfig.authResourceDir = "../resources";
        AuthServerConfig.init(authServerConfig);

        AuthServerConfig.loadCredentials();
        assertEquals("ldapadmin", AuthServerConfig.getLdapLoginPassword());
    }

    @Test
    public void readConfigTest() throws Exception {
        AuthServerConfig.readConfig("../resources");

        assertEquals("s3.seagate.com", AuthServerConfig.getDefaultEndpoint());

        assertEquals("resources/static/saml-metadata.xml",
                AuthServerConfig.getSAMLMetadataFilePath());

        assertEquals(9085, AuthServerConfig.getHttpPort());

        assertEquals(9086, AuthServerConfig.getHttpsPort());

        assertEquals("s3authserver.jks", AuthServerConfig.getKeyStoreName());

        assertEquals("seagate", AuthServerConfig.getKeyStorePassword());

        assertEquals("seagate", AuthServerConfig.getKeyPassword());

        assertFalse(AuthServerConfig.isHttpsEnabled());

        assertEquals("ldap", AuthServerConfig.getDataSource());

        assertEquals("127.0.0.1", AuthServerConfig.getLdapHost());

        assertEquals(389, AuthServerConfig.getLdapPort());

        assertEquals(636, AuthServerConfig.getLdapSSLPort());

        assertEquals(false, AuthServerConfig.isSSLToLdapEnabled());
    }

    @Test(expected = IOException.class)
    public void readConfigTest_ShouldThrowIOException() throws Exception {
        //Pass Invalid Path
        AuthServerConfig.readConfig("/invalid/path");
    }

    private Properties getAuthProperties() throws Exception {
        Properties authServerConfig = new Properties();

        authServerConfig.setProperty("s3Endpoints", "s3-us-west-2.seagate.com," +
                "s3-us.seagate.com,s3-europe.seagate.com,s3-asia.seagate.com");
        authServerConfig.setProperty("defaultEndpoint", "s3.seagate.com");
        authServerConfig.setProperty("samlMetadataFileName", "saml-metadata.xml");
        authServerConfig.setProperty("nettyBossGroupThreads","1");
        authServerConfig.setProperty("nettyWorkerGroupThreads", "2");
        authServerConfig.setProperty("nettyEventExecutorThreads", "4");
        authServerConfig.setProperty("httpPort", "9085");
        authServerConfig.setProperty("httpsPort", "9086");
        authServerConfig.setProperty("logFilePath", "/var/log/seagate/auth/");
        authServerConfig.setProperty("dataSource", "ldap");
        authServerConfig.setProperty("ldapHost", "127.0.0.1");
        authServerConfig.setProperty("ldapPort", "389");
        authServerConfig.setProperty("ldapSSLPort", "636");
        authServerConfig.setProperty("enableSSLToLdap", "true");
        authServerConfig.setProperty("ldapMaxCons", "5");
        authServerConfig.setProperty("ldapMaxSharedCons", "1");
        authServerConfig.setProperty("ldapLoginDN", "cn=admin,dc=seagate,dc=com");
        authServerConfig.setProperty("ldapLoginPW",
        "Rofaa+mJRYLVbuA2kF+CyVUJBjPx5IIFDBQfajmrn23o5aEZHonQj1ikUU9iMBoC6p/dZtVXMO1KFGzHXX3y1A==");
        authServerConfig.setProperty("consoleURL", "https://console.s3.seagate.com:9292/sso");
        authServerConfig.setProperty("enable_https", "true");
        authServerConfig.setProperty("enable_http", "false");
        authServerConfig.setProperty("enableFaultInjection", "false");
        authServerConfig.setProperty("perfEnabled", "false");
        authServerConfig.setProperty("perfLogFile", "/var/log/seagate/auth/perf.log");
        authServerConfig.setProperty("s3KeyStorePath", "../resources/");
        authServerConfig.setProperty("s3KeyStoreName", "s3authserver.jks");
        authServerConfig.setProperty("s3KeyStorePassword", "seagate");
        authServerConfig.setProperty("s3KeyPassword", "seagate");
        authServerConfig.setProperty("s3AuthCertAlias", "s3auth_pass");
        authServerConfig.setProperty("enableHttpsToS3", "true");

        return authServerConfig;
    }
}
