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

package com.seagates3.authencryptutil;

import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.lang.management.ManagementFactory;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Properties;


public class AuthEncryptConfig {
    private static Properties authEncryptConfig;
    private static String AUTH_INSTALL_DIR;

    /**
     * Initialize default endpoint and s3 endpoints etc.
     *
     * @param authServerConfig Server configuration parameters.
     */
    public static void init(Properties authServerConfig) {
        AuthEncryptConfig.authEncryptConfig = authServerConfig;
        String jvm = ManagementFactory.getRuntimeMXBean().getName();
        AuthEncryptConfig.authEncryptConfig.put("pid", jvm.substring(0, jvm.indexOf("@")));
    }

    /**
     * This method reads property file
     * @param authPropertiesFile, Properties file
     * @throws FileNotFoundException
     * @throws IOException
     */
    public static void readConfig(String installDir) throws FileNotFoundException, IOException {
        AUTH_INSTALL_DIR = installDir;
        Path authPropertiesFile = Paths.get(AUTH_INSTALL_DIR, "resources", "keystore.properties");

        Properties authEncryptConfig = new Properties();
        InputStream input = new FileInputStream(authPropertiesFile.toString());
        authEncryptConfig.load(input);

        init(authEncryptConfig);
    }

    public static String getKeyStoreName() {
        return authEncryptConfig.getProperty("s3KeyStoreName");
    }

    public static Path getKeyStorePath() {
        return Paths.get(authEncryptConfig.getProperty("s3KeyStorePath"),
                         getKeyStoreName());
    }

    public static String getKeyPassword() {
        return authEncryptConfig.getProperty("s3KeyPassword");
    }

    public static String getCertAlias() {
        return authEncryptConfig.getProperty("s3AuthCertAlias");
    }

    public static String getKeyStorePassword() {
        return authEncryptConfig.getProperty("s3KeyStorePassword");
    }

    public static String getLogLevel() {
        return authEncryptConfig.getProperty("logLevel");
    }

    public static void setLogConfigFile() {
        String logConfigFile = Paths.get(AUTH_INSTALL_DIR, "resources", "authencryptcli-log4j2.xml").toString();
        authEncryptConfig.setProperty("logConfigFile", logConfigFile );
    }

    public static String getLogConfigFile() {
        return authEncryptConfig.getProperty("logConfigFile");
    }

    // Overide helper for UT.
    public static void overrideProperty(String key, String value) {
      authEncryptConfig.setProperty(key, value);
    }
}
