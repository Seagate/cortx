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

import java.io.File;
import java.io.FileOutputStream;
import java.math.BigInteger;
import java.security.GeneralSecurityException;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.KeyStore;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.SecureRandom;
import java.security.cert.Certificate;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;
import java.util.Date;

import sun.security.x509.*;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;

import com.seagates3.authencryptutil.JKSUtil;

public class JKSUtilTest {

    static String jksFilePath;
    static String alias = "mycert";
    static String password = "test";
    static PublicKey publicKey;
    static PrivateKey privateKey;

    @BeforeClass
    public static void setUpBeforeClass() throws Exception {
        KeyStore keyStore;
        //Generate Public/Private Key Pair
        KeyPairGenerator keyPairGen = KeyPairGenerator.getInstance("RSA");
        keyPairGen.initialize(2048);
        KeyPair keyPair = keyPairGen.generateKeyPair();
        publicKey = keyPair.getPublic();
        privateKey = keyPair.getPrivate();

        //Create a temporary JKS file for test
        File file = File.createTempFile("test", ".jks");
        file.deleteOnExit();
        jksFilePath = file.getAbsolutePath();
        keyStore = KeyStore.getInstance("JKS");
        keyStore.load(null, null);
        keyStore.store(new FileOutputStream(file), password.toCharArray());

        //Generate  Self Signed certificate
        String dn = "C=IN, ST=Maharashtra, L=Pune, O=Seagate, OU=S3," +
                     "CN=test.iam.seagate.com";
        int days = 365; // certificate valid for next 365 days
        String algorithm = "MD5WithRSA";
        BigInteger sn = new BigInteger(64, new SecureRandom()); //Random serial number
        Date from = new Date();
        Date to = new Date(from.getTime() + days * 86400000l);
        CertificateValidity interval = new  CertificateValidity(from, to);
        X500Name owner = new X500Name(dn);

        X509CertInfo info = new X509CertInfo();
        info.set(X509CertInfo.VALIDITY, interval);
        info.set(X509CertInfo.SUBJECT, owner);
        info.set(X509CertInfo.ISSUER, owner);
        info.set(X509CertInfo.SERIAL_NUMBER, new CertificateSerialNumber(sn));
        info.set(X509CertInfo.KEY, new CertificateX509Key(publicKey));
        info.set(X509CertInfo.VERSION, new CertificateVersion(CertificateVersion.V3));
        AlgorithmId algo = new AlgorithmId(AlgorithmId.md5WithRSAEncryption_oid);
        info.set(X509CertInfo.ALGORITHM_ID, new CertificateAlgorithmId(algo));

        X509CertImpl cert = new X509CertImpl(info);
        cert.sign(privateKey, algorithm);
        algo = (AlgorithmId) cert.get(X509CertImpl.SIG_ALG);
        info.set(CertificateAlgorithmId.NAME + "."
                 + CertificateAlgorithmId.ALGORITHM, algo);
        cert = new X509CertImpl(info);
        cert.sign(privateKey, algorithm);

        //Store certificate in Java Key Store
        keyStore.setKeyEntry(alias, privateKey, password.toCharArray(),
                             new Certificate[] {cert});
        FileOutputStream output = new FileOutputStream(jksFilePath);
        keyStore.store(output, password.toCharArray());
    }

    @AfterClass
    public static void tearDownAfterClass() {
        File file = new File(jksFilePath);
        if(file.exists()) {
            file.delete();
        }
    }

    @Test
    public void testGetPrivateKeyFromJKS() {
        try {
            PrivateKey privateKeyOut = JKSUtil.getPrivateKeyFromJKS(jksFilePath, alias,
                                       password);
            assertNotNull(privateKeyOut);
            //Compare if we get same private key that we stored in JKS file
            assertTrue(privateKeyOut.toString().equals(privateKey.toString()));
        } catch (GeneralSecurityException e) {
            fail("Should not have thrown excption, Error Stack:" + e.getStackTrace());
        }
    }
    @Test
    public void testNegativeGetPrivateKeyFromJKS() {
        try {
            PrivateKey privateKeyOut = JKSUtil.getPrivateKeyFromJKS(jksFilePath, "InvalidAlias",
                                       password);
            assertNull(privateKeyOut);
        } catch (GeneralSecurityException e) {
            fail("Should not have thrown excption, Error Stack:" + e.getStackTrace());
        }
        try {
            PrivateKey privateKeyOut = JKSUtil.getPrivateKeyFromJKS(jksFilePath, alias,
                                       "InvalidPasswd");
            assertNull(privateKeyOut);
        } catch (GeneralSecurityException e) {
            fail("Should not have thrown excption, Error Stack:" + e.getStackTrace());
        }
        try {
            PrivateKey privateKeyOut = JKSUtil.getPrivateKeyFromJKS("InvalidFilePath", alias,
                                       password);
            assertNull(privateKeyOut);
        } catch (GeneralSecurityException e) {
            fail("Should not have thrown excption, Error Stack:" + e.getStackTrace());
        }
    }

    @Test
    public void testGetPublicKeyFromJKS() {
        try {
            PublicKey publicKeyOut = JKSUtil.getPublicKeyFromJKS(jksFilePath, alias,
                                     password);
            assertNotNull(publicKeyOut);
            //Compare if we get same public key that we stored in JKS file
            assertTrue(publicKeyOut.toString().equals(publicKey.toString()));
        } catch (GeneralSecurityException e) {
            fail("Should not have thrown excption, Error Stack:" + e.getStackTrace());
        }
    }

    @Test
    public void testNegativeGetPublicKeyFromJKS() {
        try {
            PublicKey publicKeyOut = JKSUtil.getPublicKeyFromJKS("InvalidPath", alias,
                                     password);
            assertNull(publicKeyOut);
        } catch (GeneralSecurityException e) {
            fail("Should not have thrown excption, Error Stack:" + e.getStackTrace());
        }
        try {
            PublicKey publicKeyOut = JKSUtil.getPublicKeyFromJKS(jksFilePath, "InvalidAlias",
                                     password);
            assertNull(publicKeyOut);
        } catch (GeneralSecurityException e) {
            fail("Should not have thrown excption, Error Stack:" + e.getStackTrace());
        }
        try {
            PublicKey publicKeyOut = JKSUtil.getPublicKeyFromJKS(jksFilePath, alias,
                                     "InvalidPasswd");
            assertNull(publicKeyOut);
        } catch (GeneralSecurityException e) {
            fail("Should not have thrown excption, Error Stack:" + e.getStackTrace());
        }
    }
}
