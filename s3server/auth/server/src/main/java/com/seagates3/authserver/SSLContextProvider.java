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
 * Original creation date: 23-Dec-2015
 */
package com.seagates3.authserver;

import com.seagates3.exception.ServerInitialisationException;
import io.netty.handler.ssl.SslContext;
import io.netty.handler.ssl.SslContextBuilder;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.security.KeyStore;
import java.security.KeyStoreException;
import java.security.NoSuchAlgorithmException;
import java.security.UnrecoverableKeyException;
import java.security.cert.CertificateException;
import javax.net.ssl.KeyManagerFactory;
import org.apache.logging.log4j.LogManager;
import org.apache.logging.log4j.Logger;

public class SSLContextProvider {

    private static int httpsPort;
    private static SslContext authSSLContext;
    private static final Logger LOGGER
            = LogManager.getLogger(SSLContextProvider.class.getName());

    public static void init() throws ServerInitialisationException {
        LOGGER.info("Initializing SSl Context");

        // Set Trust store
        Path s3KeystoreFile = AuthServerConfig.getKeyStorePath();
        System.setProperty("javax.net.ssl.trustStore",
                                      s3KeystoreFile.toString());

        if (AuthServerConfig.isHttpsEnabled()) {
            LOGGER.info("HTTPS is enabled.");

            httpsPort = AuthServerConfig.getHttpsPort();

            char[] keystorePassword = AuthServerConfig.getKeyStorePassword()
                    .toCharArray();
            char[] keyPassword = AuthServerConfig.getKeyPassword().toCharArray();

            KeyStore s3Keystore;
            KeyManagerFactory kmf;
            try(FileInputStream fin =
                    new FileInputStream(s3KeystoreFile.toFile())) {
                s3Keystore = KeyStore.getInstance("JKS");
                s3Keystore.load(fin, keystorePassword);

                kmf = KeyManagerFactory.getInstance(
                        KeyManagerFactory.getDefaultAlgorithm());
                kmf.init(s3Keystore, keyPassword);
                authSSLContext = SslContextBuilder.forServer(kmf).build();

                LOGGER.info("SSL context created.");
            } catch (FileNotFoundException ex) {
                throw new ServerInitialisationException(
                        "Exception occurred while initializing ssl context.\n"
                        + ex.getMessage());
            } catch (IOException | NoSuchAlgorithmException |
                    CertificateException | KeyStoreException |
                    UnrecoverableKeyException ex) {
                throw new ServerInitialisationException(
                        "Exception occurred while initializing ssl context.\n"
                        + ex.getMessage());
            }

        } else {
            LOGGER.info("HTTPS is disabled.");
            authSSLContext = null;
        }
    }

    public static SslContext getServerContext() {
        return authSSLContext;
    }

    public static int getHttpsPort() {
        return httpsPort;
    }
}
