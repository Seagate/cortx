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
 * Original creation date: 31-Dec-2016
 */
package com.seagates3.dao.ldap;

import com.novell.ldap.LDAPConnection;
import com.novell.ldap.LDAPException;
import com.novell.ldap.LDAPJSSESecureSocketFactory;
import com.novell.ldap.connectionpool.PoolManager;
import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.exception.ServerInitialisationException;
import com.seagates3.fi.FaultPoints;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.powermock.api.mockito.PowerMockito;
import org.powermock.api.mockito.mockpolicies.Slf4jMockPolicy;
import org.powermock.core.classloader.annotations.MockPolicy;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;

import static org.junit.Assert.*;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.powermock.api.mockito.PowerMockito.doReturn;

import java.nio.file.Path;
import java.nio.file.Paths;

@RunWith(PowerMockRunner.class)
@PrepareForTest({AuthServerConfig.class, FaultPoints.class, LdapConnectionManager.class})
@MockPolicy(Slf4jMockPolicy.class)
public class LdapConnectionManagerTest {

    private PoolManager ldapPool;
    private LDAPConnection ldapConnection;
    private LDAPJSSESecureSocketFactory socketFactory;
    private final String LDAP_HOST = "127.0.0.1";
    private final int LDAP_PORT = 389;
    private final int LDAP_SSL_PORT = 636;
    private final int LDAP_MAX_CONN = 5;
    private final int LDAP_MAX_SHARED_CONN = 1;
    private final String LDAP_LOGIN_DN = "cn=admin,dc=seagate,dc=com";
    private final String LDAP_LOGIN_PASSWD = "seagate";

    @Before
    public void setUp() throws Exception {
        PowerMockito.mockStatic(AuthServerConfig.class);

        PowerMockito.doReturn(LDAP_HOST).when(AuthServerConfig.class, "getLdapHost");
        PowerMockito.doReturn(LDAP_PORT).when(AuthServerConfig.class, "getLdapPort");
        PowerMockito.doReturn(LDAP_MAX_CONN).when(AuthServerConfig.class, "getLdapMaxConnections");
        PowerMockito.doReturn(LDAP_MAX_SHARED_CONN).when(AuthServerConfig.class, "getLdapMaxSharedConnections");
        PowerMockito.doReturn(LDAP_LOGIN_DN).when(AuthServerConfig.class, "getLdapLoginDN");
        PowerMockito.doReturn(LDAP_LOGIN_PASSWD).when(AuthServerConfig.class, "getLdapLoginPassword");

        socketFactory = mock(LDAPJSSESecureSocketFactory.class);
        PowerMockito.whenNew(LDAPJSSESecureSocketFactory.class)
                .withNoArguments().thenReturn(socketFactory);

        ldapPool = mock(PoolManager.class);
        PowerMockito.whenNew(PoolManager.class).withArguments(LDAP_HOST,
                LDAP_PORT, LDAP_MAX_CONN, LDAP_MAX_SHARED_CONN, null).thenReturn(ldapPool);

        ldapConnection = mock(LDAPConnection.class);
        PowerMockito.when(ldapPool.getBoundConnection(LDAP_LOGIN_DN,
                LDAP_LOGIN_PASSWD.getBytes("UTF-8"))).thenReturn(ldapConnection);
    }

    @Test
    public void initLdapTest() throws Exception {
        PowerMockito.doReturn(false)
                .when(AuthServerConfig.class, "isSSLToLdapEnabled");

        LdapConnectionManager.initLdap();

        PowerMockito.verifyNew(LDAPJSSESecureSocketFactory.class, times(0))
                .withNoArguments();
        PowerMockito.verifyNew(PoolManager.class, times(1))
                .withArguments(LDAP_HOST, LDAP_PORT, LDAP_MAX_CONN,
                                       LDAP_MAX_SHARED_CONN, null);
    }

    @Test
    public void initLdapTestSSL() throws Exception {
        PowerMockito.doReturn(true)
                .when(AuthServerConfig.class, "isSSLToLdapEnabled");
        Path storePath = Paths.get("/tmp");
        PowerMockito.doReturn(storePath)
        .when(AuthServerConfig.class, "getKeyStorePath");

        PowerMockito.doReturn(LDAP_SSL_PORT)
        .when(AuthServerConfig.class, "getLdapSSLPort");

        LdapConnectionManager.initLdap();
        PowerMockito.verifyNew(LDAPJSSESecureSocketFactory.class, times(1))
                .withNoArguments();
        PowerMockito.verifyNew(PoolManager.class, times(1))
                .withArguments(LDAP_HOST, LDAP_SSL_PORT, LDAP_MAX_CONN,
                                   LDAP_MAX_SHARED_CONN, socketFactory);
    }

    @Test(expected = ServerInitialisationException.class)
    public void initLdapTest_ThrowException() throws Exception {
        PowerMockito.whenNew(PoolManager.class).withArguments(LDAP_HOST, LDAP_PORT,
                LDAP_MAX_CONN, LDAP_MAX_SHARED_CONN, null)
                .thenThrow(LDAPException.class);

        LdapConnectionManager.initLdap();
    }

    @Test
    public void getConnectionTest() throws Exception {
        // Act
        LdapConnectionManager.initLdap();
        LDAPConnection lc = LdapConnectionManager.getConnection();

        // Verify
        assertNotNull(lc);
        verify(ldapPool).getBoundConnection("cn=admin,dc=seagate,dc=com",
                "seagate".getBytes("UTF-8"));
    }

    @Test
    public void getConnectionTest_ShouldFailIfFiIsEnabled() throws Exception {
        // Arrange
        enableFaultInjection("LDAP_CONNECT_FAIL");

        // Act
        LdapConnectionManager.initLdap();
        LDAPConnection lc = LdapConnectionManager.getConnection();

        // Verify
        assertNull(lc);
        verify(ldapPool, times(0)).getBoundConnection("cn=admin,dc=seagate,dc=com",
                "seagate".getBytes("UTF-8"));
    }

    @Test
    public void getConnectionTest_ShouldFailIfLDAPConnInterrupted() throws Exception {
        // Arrange
        enableFaultInjection("LDAP_CONN_INTRPT");

        // Act
        LdapConnectionManager.initLdap();
        LDAPConnection lc = LdapConnectionManager.getConnection();

        // Verify
        assertNull(lc);
        verify(ldapPool, times(0)).getBoundConnection("cn=admin,dc=seagate,dc=com",
                "seagate".getBytes("UTF-8"));
    }

    @Test
    public void releaseConnectionTest() throws Exception {
        LdapConnectionManager.ldapPool = mock(PoolManager.class);

        // Act
        LdapConnectionManager.releaseConnection(ldapConnection);

        // Verify
        verify(LdapConnectionManager.ldapPool).makeConnectionAvailable(ldapConnection);
    }

    private void enableFaultInjection(String faultPoint) throws Exception {
        PowerMockito.mockStatic(FaultPoints.class);
        doReturn(Boolean.TRUE).when(FaultPoints.class, "fiEnabled");
        FaultPoints faultPoints = mock(FaultPoints.class);
        doReturn(faultPoints).when(FaultPoints.class, "getInstance");
        doReturn(Boolean.TRUE).when(faultPoints).isFaultPointActive(faultPoint);
    }
    @Test public void getConnectionParameterizedTest() throws Exception {
      LdapConnectionManager.initLdap();
      LDAPConnection lc = LdapConnectionManager.getConnection(
          AuthServerConfig.getLdapLoginDN(),
          AuthServerConfig.getLdapLoginPassword());
      // Verify
      assertNotNull(lc);
      verify(ldapPool).getBoundConnection("cn=admin,dc=seagate,dc=com",
                                          "seagate".getBytes("UTF-8"));
    }

    @Test public void
    getConnectionParameterizedTest_ShouldFailIfLDAPConnInterrupted()
        throws Exception {
      enableFaultInjection("LDAP_CONN_INTRPT");
      LdapConnectionManager.initLdap();
      LDAPConnection lc = LdapConnectionManager.getConnection(
          AuthServerConfig.getLdapLoginDN(),
          AuthServerConfig.getLdapLoginPassword());
      // Verify
      assertNull(lc);
      verify(ldapPool, times(0)).getBoundConnection(
          "cn=admin,dc=seagate,dc=com", "seagate".getBytes("UTF-8"));
    }
}
