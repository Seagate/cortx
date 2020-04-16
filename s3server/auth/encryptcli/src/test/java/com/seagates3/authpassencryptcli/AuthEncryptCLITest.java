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
 * Original creation date: 10-06-2018
 */

package com.seagates3.authpassencryptcli;


import java.io.FileInputStream;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.security.PublicKey;
import java.util.Properties;

import org.apache.logging.log4j.core.config.Configurator;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.powermock.api.mockito.mockpolicies.Slf4jMockPolicy;
import org.powermock.core.classloader.annotations.MockPolicy;
import org.powermock.core.classloader.annotations.PowerMockIgnore;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;
import org.powermock.reflect.internal.WhiteboxImpl;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import static org.junit.Assert.*;
import static org.mockito.Mockito.mock;

import com.seagates3.authencryptutil.AuthEncryptConfig;
import com.seagates3.authencryptutil.RSAEncryptDecryptUtil;

@RunWith(PowerMockRunner.class)
@MockPolicy(Slf4jMockPolicy.class)
@PowerMockIgnore({"javax.management.*", "javax.crypto.*"})


public  class AuthEncryptCLITest {

    @Before
    public void setUp() throws Exception {
        //For Unit Tests resource folder is at s3server/auth/resources
        String installDir = "..";
        Logger logger = mock(Logger.class);
        AuthEncryptConfig.readConfig(installDir);
        AuthEncryptConfig.overrideProperty("s3KeyStorePath", "../resources");
        WhiteboxImpl.setInternalState(AuthEncryptCLI.class, "logger", logger);
    }

    @Test
    public void testProcessEncryptRequest() throws Exception {
        String passwd = "test";
        String encryptedPasswd = AuthEncryptCLI.processEncryptRequest(passwd);
        assertFalse(passwd.equals(encryptedPasswd));
    }
    @Test
    public void testProcessEncryptRequestNegative() throws Exception {
        String passwd = "";
        String encryptedPasswd;
        try {
            encryptedPasswd = AuthEncryptCLI.processEncryptRequest(passwd);
            fail("Expecting a exception when password is empty");
        } catch (Exception e) {
            assertEquals(e.getMessage(), "Invalid Password Value.");
        }

        passwd = null;
        try {
            encryptedPasswd = AuthEncryptCLI.processEncryptRequest(passwd);
            fail("Expecting a exception when password is null");
        } catch (Exception e) {
            assertEquals(e.getMessage(), "Invalid Password Value.");
        }

        passwd = "test space";
        try {
            encryptedPasswd = AuthEncryptCLI.processEncryptRequest(passwd);
            fail("Expecting a exception when password contains spaces");
        } catch (Exception e) {
            assertEquals(e.getMessage(), "Invalid Password Value.");
        }
    }
}
