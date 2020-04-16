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

import java.io.Console;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;

import java.nio.file.Path;
import java.nio.file.Paths;
import java.security.GeneralSecurityException;
import java.security.PublicKey;
import java.util.Arrays;

import java.util.Properties;

import org.apache.logging.log4j.Level;
import org.apache.logging.log4j.core.config.ConfigurationSource;
import org.apache.logging.log4j.core.config.Configurator;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.seagates3.authencryptutil.AuthEncryptConfig;
import com.seagates3.authencryptutil.JKSUtil;
import com.seagates3.authencryptutil.RSAEncryptDecryptUtil;
import com.seagates3.exception.CLIInitializationException;


/**
 * AuthEncryptCLI class
 *
 */
public class AuthEncryptCLI {

    public static String AUTH_INSTALL_DIR = "/opt/seagate/auth";
    private static Logger logger;

    /**
     * @param args
     */
    public static void usage() {
        System.out.println("Usage: java -jar AuthPassEncryptCLI-1.0-0.jar"
                                                          + " [options]");
        System.out.println("-s  <password>       Specify password");
        System.out.println("-help");
    }

    /**
     * This method reads password from console
     * @return String
     */
    public static String readFromConsole() {
        Console console = System.console();
        if (console == null) {
            System.err.println("Couldn't get Console instance.");
            System.exit(1);
        }
        char[] tmp1 = console.readPassword("Enter Password:");
        char[] tmp2 = console.readPassword("Re-enter Password:");
        if(!Arrays.equals(tmp1, tmp2)) {
            System.err.println("Password values do not match.");
            System.exit(1);
        }
        Arrays.fill(tmp1, ' ');

        return new String(tmp2);
    }

    /**
     * This method parses command line arguments and return password
     * @param args
     * @return String
     */
    public static String parseArgs(String[] args) {

        String passwd = null;
        if(args.length > 0) {
            if(args[0] == "-help") {
                usage();
                System.exit(0);
            } else if((args[0].equals("-s")) && (args.length == 2)) {
                passwd = args[1];
            } else {
                logger.error("Incorrect arguments passed.");
                usage();
                System.exit(1);
            }
        }
        return passwd;
    }

    /**
     * This method encrypts password using public key from key store
     * @param passwd
     * @throws GeneralSecurityException
     * @throws Exception
     */
    public static String processEncryptRequest(String passwd)
                       throws GeneralSecurityException, Exception {

        String encryptedPasswd = null;
        if(passwd == null || passwd.isEmpty()
                          || passwd.matches(".*([ \t]).*")) {
            logger.error("Password is null or empty or contains spaces");
            System.err.println("Invalid Password Value.");
            throw new Exception("Invalid Password Value.");
        }

        Path s3KeystoreFile = AuthEncryptConfig.getKeyStorePath();

        logger.debug("Using Java Key Store File: " + s3KeystoreFile.toString());
        String keyPassword = AuthEncryptConfig.getKeyStorePassword();
        String certAlias = AuthEncryptConfig.getCertAlias();
        PublicKey pKey = JKSUtil.getPublicKeyFromJKS(s3KeystoreFile.toString(),
                         certAlias, keyPassword);

        if(pKey == null ) {
            logger.error("Failed get public key." );
            throw new Exception("Failed get public key.");
        }

        logger.debug("Public Key :" + pKey.toString());
        encryptedPasswd = RSAEncryptDecryptUtil.encrypt(passwd, pKey);

        if(encryptedPasswd == null || encryptedPasswd.isEmpty()) {
            logger.error("Encrypted Password is null or empty");
            throw new Exception("Failed to encrypt password.");
        }
        return encryptedPasswd;
    }

    /**
     * This method initializes logger
     * @throws IOException
     */
    static void logInit() throws CLIInitializationException, Exception {

        AuthEncryptConfig.setLogConfigFile();
        String logConfigFilePath = AuthEncryptConfig.getLogConfigFile();

        /**
         * If log4j config file is given, override the default Logging
         * properties file.
         */
        if (logConfigFilePath != null) {
            File logConfigFile = new File(logConfigFilePath);
            if (logConfigFile.exists()) {
                ConfigurationSource source = new ConfigurationSource(
                        new FileInputStream(logConfigFile));
                Configurator.initialize(null, source);
            } else {
                throw new CLIInitializationException("Logging config file "
                                  + logConfigFilePath + " doesn't exist.");
            }
        }

        String logLevel = AuthEncryptConfig.getLogLevel();
        if (logLevel != null) {
            Level level = Level.getLevel(logLevel);
            if (level == null) {
                //set to ALL if couldn't find appropriate level value.
                throw new Exception("Invalid log value [" + logLevel + "]"
                                            + "specified in config file");
            } else {
                Configurator.setRootLevel(level);
            }
        }
    }

    public static void main(String[] args) {

        try {
            AuthEncryptConfig.readConfig(AUTH_INSTALL_DIR);
        } catch (IOException e1) {
            logger.error("Failed to read Properties from install dir:"
                 + AUTH_INSTALL_DIR + " Error Stack :" + e1.getStackTrace());
            System.err.println("Failed to read Properties from install dir:"
                                                        + AUTH_INSTALL_DIR);
            System.exit(1);
        }
        try {
            logInit();
            logger = LoggerFactory.getLogger(AuthEncryptCLI.class.getName());
        } catch (CLIInitializationException e) {
            System.err.println("Failed to Initialize logger");
            e.printStackTrace();
            System.exit(1);
        } catch (Exception e) {

        }
        String passwd = null;

        logger.debug("Initialization successfull");
        if(args.length == 0) {
            passwd = readFromConsole();
        } else {
            passwd = parseArgs(args);
        }
        try {
          String encryptedPasswd =  processEncryptRequest(passwd);
          //Output to console
          System.out.println(encryptedPasswd);
        } catch (GeneralSecurityException e) {
            logger.error("Failed to encrypt password. " + e.getCause()
                                                    + e.getMessage());
            System.err.println("Failed to encrypt password.");
            System.exit(1);
        } catch (Exception e) {
            logger.error("Failed to encrypt password. " + e.getCause()
                                                    + e.getMessage());
            System.err.println("Failed to encrypt password.");
            System.exit(1);
        }
        logger.debug("Operation completed successfully");
    }
}
