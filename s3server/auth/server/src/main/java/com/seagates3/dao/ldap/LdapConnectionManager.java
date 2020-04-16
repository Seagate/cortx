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
 * Original creation date: 17-Sep-2015
 */
package com.seagates3.dao.ldap;

import java.io.UnsupportedEncodingException;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.novell.ldap.LDAPConnection;
import com.novell.ldap.LDAPException;
import com.novell.ldap.LDAPJSSESecureSocketFactory;
import com.novell.ldap.LDAPSocketFactory;
import com.novell.ldap.connectionpool.PoolManager;
import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.exception.ServerInitialisationException;
import com.seagates3.fi.FaultPoints;
import com.seagates3.util.IEMUtil;

public class LdapConnectionManager {

    private static final Logger LOGGER = LoggerFactory.getLogger(LdapConnectionManager.class.getName());
    static PoolManager ldapPool;
    static String ldapLoginDN, ldapLoginPW;

    public static void initLdap()
            throws ServerInitialisationException {

        try {
            LDAPSocketFactory socketFactory = null;
            int port;
            if (AuthServerConfig.isSSLToLdapEnabled()) {
                port = AuthServerConfig.getLdapSSLPort();
                LOGGER.info("Connecting ldap on SSL port :" + port);
                socketFactory = new LDAPJSSESecureSocketFactory();
            }
            else {
                port = AuthServerConfig.getLdapPort();
                LOGGER.info("Connecting ldap on port :" + port);
            }

            ldapPool = new PoolManager(AuthServerConfig.getLdapHost(),
                    port,
                    AuthServerConfig.getLdapMaxConnections(),
                    AuthServerConfig.getLdapMaxSharedConnections(),
                    socketFactory);

            ldapLoginDN = AuthServerConfig.getLdapLoginDN();
            ldapLoginPW = AuthServerConfig.getLdapLoginPassword();
        } catch (LDAPException ex) {
            String msg = "Failed to initialise LDAP.\n" + ex.toString();
            LOGGER.error(msg);
            IEMUtil.log(IEMUtil.Level.ERROR, IEMUtil.LDAP_EX,
                    "LDAP exception occurred",
                    String.format("\"cause\": \"%s\"", ex.getCause()));
            throw new ServerInitialisationException(msg);
        }
    }

    public static LDAPConnection getConnection() {
        LDAPConnection lc = null;
        try {
            if (FaultPoints.fiEnabled()) {
                if (FaultPoints.getInstance().isFaultPointActive("LDAP_CONNECT_FAIL")) {
                    throw new LDAPException("Connection failed", LDAPException.CONNECT_ERROR,
                            null);
                } else if (FaultPoints.getInstance().isFaultPointActive("LDAP_CONN_INTRPT")) {
                    throw new InterruptedException("Connection interrupted");
                }
            }

            lc = ldapPool.getBoundConnection(
                    ldapLoginDN, ldapLoginPW.getBytes("UTF-8"));
        } catch (LDAPException ex) {
            LOGGER.error("LDAPException Cause: " + ex.getCause()
                         + ". Message: " + ex.getMessage());
            IEMUtil.log(IEMUtil.Level.ERROR, IEMUtil.LDAP_EX, "LDAP exception occurred",
                    String.format("\"cause\": \"%s\"", ex.getCause()));
        } catch (InterruptedException ex) {
            LOGGER.error("Failed to connect to LDAP server. Cause: "
                    + ex.getCause() + ". Message: "
                    + ex.getMessage());
        } catch (UnsupportedEncodingException ex) {
            LOGGER.error("UnsupportedEncodingException Cause: " + ex.getCause()
                       + ". Message: " + ex.getMessage());
            IEMUtil.log(IEMUtil.Level.ERROR, IEMUtil.UTF8_UNAVAILABLE,
                    "UTF-8 encoding is not supported", null);
        }

        return lc;
    }

    public static void releaseConnection(LDAPConnection lc) {
        ldapPool.makeConnectionAvailable(lc);
    }

   public
    static LDAPConnection getConnection(String dn,
                                        String password) throws LDAPException {
      LDAPConnection lc = null;
      try {
        if (FaultPoints.fiEnabled()) {
          if (FaultPoints.getInstance().isFaultPointActive(
                  "LDAP_CONNECT_FAIL")) {
            throw new LDAPException("Connection failed",
                                    LDAPException.CONNECT_ERROR, null);
          } else if (FaultPoints.getInstance().isFaultPointActive(
                         "LDAP_CONN_INTRPT")) {
            throw new InterruptedException("Connection interrupted");
          }
        }

        lc = ldapPool.getBoundConnection(dn, password.getBytes("UTF-8"));
      }
      catch (InterruptedException ex) {
        LOGGER.error("Failed to connect to LDAP server. Cause: " +
                     ex.getCause() + ". Message: " + ex.getMessage());
      }
      catch (UnsupportedEncodingException ex) {
        LOGGER.error("UnsupportedEncodingException Cause: " + ex.getCause() +
                     ". Message: " + ex.getMessage());
        IEMUtil.log(IEMUtil.Level.ERROR, IEMUtil.UTF8_UNAVAILABLE,
                    "UTF-8 encoding is not supported", null);
      }

      return lc;
    }
}

