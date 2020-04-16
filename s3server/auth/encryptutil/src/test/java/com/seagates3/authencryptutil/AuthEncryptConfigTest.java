/*
 * COPYRIGHT 2018 SEAGATE LLC
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
 * Original author: Basavaraj Kirunge
 * Original creation date: 01-Jun-2018
 */


package com.seagates3.authencryptutil;

import org.junit.Test;

import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Properties;

import static org.junit.Assert.*;

public class AuthEncryptConfigTest {

    @Test
    public void initTest() throws Exception {
        Properties authEncryptConfig = getAuthProperties();
        AuthEncryptConfig.init(authEncryptConfig);

        assertEquals("s3authserver.jks", AuthEncryptConfig.getKeyStoreName());

        assertEquals("seagate", AuthEncryptConfig.getKeyStorePassword());

        assertEquals("seagate", AuthEncryptConfig.getKeyPassword());
        assertEquals("passencrypt", AuthEncryptConfig.getCertAlias());
        assertEquals("DEBUG", AuthEncryptConfig.getLogLevel());
    }

    @Test
    public void testReadConfig() throws Exception {
        String installDir = "..";
        AuthEncryptConfig.readConfig(installDir);

        assertEquals("s3authserver.jks", AuthEncryptConfig.getKeyStoreName());

        assertEquals("seagate", AuthEncryptConfig.getKeyStorePassword());

        assertEquals("seagate", AuthEncryptConfig.getKeyPassword());
        assertEquals("s3auth_pass", AuthEncryptConfig.getCertAlias());

        installDir = "invaliddir";
        try {
            AuthEncryptConfig.readConfig(installDir);
        } catch (FileNotFoundException e) {
            String errMsg = e.getMessage();
            assertTrue(e.getMessage().equals("invaliddir/resources/keystore.properties (No such file or directory)"));
        }
    }

    private Properties getAuthProperties() throws Exception {
        Properties AuthEncryptConfig = new Properties();

        AuthEncryptConfig.setProperty("s3KeyStoreName", "s3authserver.jks");
        AuthEncryptConfig.setProperty("s3KeyStorePassword", "seagate");
        AuthEncryptConfig.setProperty("s3KeyPassword", "seagate");
        AuthEncryptConfig.setProperty("s3AuthCertAlias", "passencrypt");
        AuthEncryptConfig.setProperty("logLevel", "DEBUG");
        return AuthEncryptConfig;
    }
}
