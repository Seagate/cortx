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

import static org.junit.Assert.*;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertEquals;
import java.security.GeneralSecurityException;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.PrivateKey;
import java.security.PublicKey;
import org.junit.Before;
import org.junit.Test;

import com.seagates3.authencryptutil.RSAEncryptDecryptUtil;

public class RSAEncryptDecryptUtilTest {
    private PrivateKey privateKey;
    private PublicKey publicKey;
    private String sampleText = "Pass@w0rd!430";
    String encryptedText;

    @Before
    public void setUp() throws Exception {
        KeyPairGenerator keyPairGenerator = KeyPairGenerator.getInstance("RSA");
        keyPairGenerator.initialize(2048);
        KeyPair keyPair = keyPairGenerator.generateKeyPair();
        privateKey = keyPair.getPrivate();
        publicKey = keyPair.getPublic();
    }

    @Test
    public void testEncryptDecrypt() {
        try {
            encryptedText = RSAEncryptDecryptUtil.encrypt(sampleText, publicKey);
            assertNotNull(encryptedText);
            assertFalse(sampleText.equals(encryptedText));
        } catch (GeneralSecurityException e) {
            fail("Should not have thrown excption, Error Stack:" + e.getStackTrace());
        } catch (Exception e) {
            fail("Should not have thrown excption, Error Stack:" + e.getStackTrace());
        }
        try {
            String plainText = RSAEncryptDecryptUtil.decrypt(encryptedText, privateKey);
            assertNotNull(plainText);
            assertTrue(plainText.equals(sampleText));
        } catch (GeneralSecurityException e) {
            fail("Should not have thrown excption, Error Stack:" + e.getStackTrace());
        } catch (Exception e) {
            fail("Should not have thrown excption, Error Stack:" + e.getStackTrace());
        }
    }

    @Test
    public void testNegativeEncryptDecrypt() {
        try {
            String nullText = null;
            encryptedText = RSAEncryptDecryptUtil.encrypt(nullText, publicKey);
            fail("Expecting a excption to be thrown");
        } catch (GeneralSecurityException e) {
            fail("Should not have thrown excption, Error Stack:" + e.getStackTrace());
        } catch (Exception e) {
             assertEquals(e.getMessage(), "Input text is null or empty");
        }

        try {
            String nullText = "";
            encryptedText = RSAEncryptDecryptUtil.encrypt(nullText, publicKey);
            fail("Expecting a excption to be thrown");
        } catch (GeneralSecurityException e) {
            fail("Should not have thrown excption, Error Stack:" + e.getStackTrace());
        } catch (Exception e) {
             assertEquals(e.getMessage(), "Input text is null or empty");
        }

        try {
        	encryptedText = null;
            sampleText = RSAEncryptDecryptUtil.decrypt(encryptedText, privateKey);
            fail("Expecting a excption to be thrown");
        } catch (GeneralSecurityException e) {
            fail("Should not have thrown excption, Error Stack:" + e.getStackTrace());
        } catch (Exception e) {
             assertEquals(e.getMessage(), "Encrypted Text is null or empty");
        }

        try {
        	encryptedText = "";
            sampleText = RSAEncryptDecryptUtil.decrypt(encryptedText, privateKey);
            fail("Expecting a excption to be thrown");
        } catch (GeneralSecurityException e) {
            fail("Should not have thrown excption, Error Stack:" + e.getStackTrace());
        } catch (Exception e) {
             assertEquals(e.getMessage(), "Encrypted Text is null or empty");
        }

        try {
            String inText = "test";
            PublicKey pKey = null;
            encryptedText = RSAEncryptDecryptUtil.encrypt(inText, pKey);
            fail("Expecting a excption to be thrown");
        } catch (GeneralSecurityException e) {
            fail("Should not have thrown excption, Error Stack:" + e.getStackTrace());
        } catch (Exception e) {
             assertEquals(e.getMessage(), "Invalid Public Key");
        }

        try {
        	encryptedText = "hfuoerhfow8028r200hfh20";
        	PrivateKey pvtKey = null;
            sampleText = RSAEncryptDecryptUtil.decrypt(encryptedText, pvtKey);
            fail("Expecting a excption to be thrown");
        } catch (GeneralSecurityException e) {
            fail("Should not have thrown excption, Error Stack:" + e.getStackTrace());
        } catch (Exception e) {
             assertEquals(e.getMessage(), "Invalid Private Key");
        }
    }

}
