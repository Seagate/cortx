/*
 * COPYRIGHT 2017 SEAGATE LLC
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
 * Original creation date: 02-Jan-2017
 */
package com.seagates3.util;

import org.junit.Test;

import static org.junit.Assert.*;

import java.security.NoSuchAlgorithmException;

public class KeyGenUtilTest {

    @Test
    public void createUserIdTest() {
        String userID = KeyGenUtil.createUserId();

        assertNotNull(userID);
        assertFalse(userID.startsWith("-") || userID.startsWith("_"));
    }

    @Test
    public void createUserAccessKeyIdTest() {
        String accessKeyId = KeyGenUtil.createUserAccessKeyId();

        assertNotNull(accessKeyId);
        assertFalse(accessKeyId.startsWith("-") || accessKeyId.startsWith("_"));
    }

    @Test
    public void createUserSecretKeyTest() {
        String secretKey = KeyGenUtil.generateSecretKey();

        assertNotNull(secretKey);
        assertFalse(secretKey.startsWith("-") || secretKey.startsWith("_"));
        assertEquals(40, secretKey.length());
    }

    @Test
    public void createSessionIdTest() {
        String sessionId = KeyGenUtil.createSessionId("SomeRandomString");

        assertNotNull(sessionId);
        assertFalse(sessionId.startsWith("-") || sessionId.startsWith("_"));
    }

    @Test(expected = NullPointerException.class)
    public void createSessionIdTest_ShouldThrowException() {
        KeyGenUtil.createSessionId(null);
    }

    @Test
    public void createIdTest() {
        String id = KeyGenUtil.createId();

        assertNotNull(id);
        assertFalse(id.startsWith("-") || id.startsWith("_"));
    }

    @Test public void generateSSHATest() {
      String id = null;
      try {
        id = KeyGenUtil.generateSSHA("testtext");
      }
      catch (NoSuchAlgorithmException e) {
        e.printStackTrace();
        fail();
      }

      assertNotNull(id);
      assertTrue(id.startsWith("{SSHA}"));
    }

    @Test public void createCanonicalIdTest() {
      String id = null;
      // test multiple canonical ids and validate it
      for (int i = 0; i < 10; i++) {
        id = KeyGenUtil.createCanonicalId();

        assertNotNull(id);
        assertFalse(id.contains("-") || id.contains("_") || id.contains("&") ||
                    id.contains("+"));
        assertEquals(64, id.length());
      }
    }

    @Test public void createAccountIdTest() {
      String id = null;
      // test multiple account ids and validate it
      for (int i = 0; i < 10; i++) {
        id = KeyGenUtil.createAccountId();

        assertNotNull(id);
        assertFalse(id.contains("-") || id.contains("_") || id.contains("&") ||
                    id.contains("+"));
        assertEquals(12, id.length());
      }
    }

    @Test public void createIamUserIdTest() {
      String id = null;
      // test multiple iam user ids and validate it
      for (int i = 0; i < 10; i++) {
        id = KeyGenUtil.createIamUserId();

        assertNotNull(id);
        assertFalse(id.contains("-") || id.contains("_") || id.contains("&") ||
                    id.contains("+"));
        assertEquals(17, id.length());
      }
    }
}
