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

import java.security.cert.Certificate;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import java.security.GeneralSecurityException;
import java.security.Key;
import java.security.KeyStore;
import java.security.PrivateKey;
import java.security.PublicKey;

/**
 * The Abstract class to retrieves Public key and Private key corresponding
 * to alias from Java Key store file.
 */
public class JKSUtil {
    /**
     * This method retrieves Public key corresponding to given alias from
     * Java Key store file.
     * @param jksFilePath : Location of java Key Store Path
     * @param alias : Certificate Alias
     * @param password : Java Key Store Password
     * @return PublicKey
     * @throws GeneralSecurityException
     */
    private static final Logger LOGGER = LoggerFactory.getLogger(
            JKSUtil.class.getName());
    public static PublicKey getPublicKeyFromJKS(String jksFilePath,
                            String alias, String password)
                            throws GeneralSecurityException {
        KeyStore keyStore = KeyStore.getInstance("JKS");
        char[] keystorePassword = password.toCharArray();
        try {
            FileInputStream fin = new FileInputStream(jksFilePath);
            keyStore.load(fin, keystorePassword);
            Certificate cert = keyStore.getCertificate(alias);
            if(cert == null) {
            	LOGGER.error("Failed to find get keypair from java key store");
            	return null;
            }
            PublicKey publicKey = cert.getPublicKey();
            return publicKey;
        } catch (FileNotFoundException e) {
            LOGGER.error("Failed to open Java Key store file. Cause:" +
                                   e.getCause() + ". Message:" +
                                   e.getMessage());
        } catch (IOException e) {
            LOGGER.error("Failed to Load Java Key store file. Cause:"  +
                    e.getCause() + ". Message:" +
                    e.getMessage());
        }
        LOGGER.error("Failed to find public key from Java Key Store File");
        return null;
    }

    /**
     * This method retrieves Private key corresponding to alias from Java Key store file.
     * @param jksFilePath : Location of java Key Store Path
     * @param alias : Certificate Alias
     * @param password : Java Key Store Password
     * @return PrivateKey
     * @throws GeneralSecurityException
     */
    public static PrivateKey getPrivateKeyFromJKS(String jksFilePath,
                             String alias, String password)
                             throws GeneralSecurityException {
        KeyStore keyStore = KeyStore.getInstance("JKS");
        char[] keystorePassword = password.toCharArray();
        FileInputStream fin;
        try {
            fin = new FileInputStream(jksFilePath);
            keyStore.load(fin, keystorePassword);
            Key key = keyStore.getKey(alias, keystorePassword);
            if(key instanceof PrivateKey) {
                return (PrivateKey)key;
            }
        } catch (FileNotFoundException e) {
        	LOGGER.error("Failed to open Java Key store file. Cause:" +
                    e.getCause() + ". Message:" +
                    e.getMessage());
        } catch (IOException e) {
        	LOGGER.error("Failed to open Java Key store file. Cause:" +
                    e.getCause() + ". Message:" +
                    e.getMessage());

        }
        LOGGER.error("Failed to find private key from Java Key Store File");
        return null;
    }
}
