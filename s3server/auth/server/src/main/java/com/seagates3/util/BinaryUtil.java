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
package com.seagates3.util;

import java.io.UnsupportedEncodingException;
import java.nio.ByteBuffer;
import java.security.InvalidKeyException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.UUID;
import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;
import org.apache.commons.codec.binary.Base64;

public class BinaryUtil {
    /*
     * Calculate the hash of the string and encode it to hexadecimal characters.
     * The characters have to be lower case.
     */

    private static final int MASK_4BITS = (1 << 4) - 1;

    private static final byte[] hexChars = "0123456789abcdef".getBytes();

    /*
     * <IEM_INLINE_DOCUMENTATION>
     *     <event_code>048002001</event_code>
     *     <application>S3 Authserver</application>
     *     <submodule>JRE</submodule>
     *     <description>UTF-8 encoding is not supported</description>
     *     <audience>Service</audience>
     *     <details>
     *         The encoding UTF-8 is not supported by the Java runtime.
     *         The data section of the event has following keys:
     *           time - timestamp
     *           node - node name
     *           pid - process id of Authserver
     *           file - source code filename
     *           line - line number within file where error occurred
     *     </details>
     *     <service_actions>
     *         Stop authserver. Replace Java runtime with stable Java runtime
     *         which supports UTF-8 charset. Restart authserver.
     *     </service_actions>
     * </IEM_INLINE_DOCUMENTATION>
     *
     */
    /*
     * Return Hex encoded String.
     * All alphabets are lower case.
     */
    public static String hexEncodedHash(String text) {
        try {
            byte[] hashedText = hashSHA256(text.getBytes("UTF-8"));
            return (hashedText != null) ? toString(encodeToHex(hashedText))
                                        : null;
        } catch (UnsupportedEncodingException ex) {
            IEMUtil.log(IEMUtil.Level.ERROR, IEMUtil.UTF8_UNAVAILABLE,
                    "UTF-8 encoding is not supported", null);
        }

        return null;
    }

    /*
     * Compute the hash of the byte array and encode it to hex format.
     * All alphabets are lower case.
     */
    public static String hexEncodedHash(byte[] text) {
        byte[] hashedText = hashSHA256(text);
        return (hashedText != null) ? toString(encodeToHex(hashedText)) : null;
    }

    /*
     * Return a base 64 encoded hash generated using SHA-256.
     */
    public static String base64EncodedHash(String text) {
        ByteBuffer bb;
        String secret_key;

        byte[] digestBuff = BinaryUtil.hashSHA256(text);
        bb = ByteBuffer.wrap(digestBuff);
        secret_key = encodeToUrlSafeBase64String(bb.array());

        return secret_key;
    }

    /*
     * <IEM_INLINE_DOCUMENTATION>
     *     <event_code>048002002</event_code>
     *     <application>S3 Authserver</application>
     *     <submodule>JRE</submodule>
     *     <description>Algorithm HmacSHA256 not available</description>
     *     <audience>Service</audience>
     *     <details>
     *         Algorithm HmacSHA256 is not available.
     *         The data section of the event has following keys:
     *           time - timestamp
     *           node - node name
     *           pid - process id of Authserver
     *           file - source code filename
     *           line - line number within file where error occurred
     *     </details>
     *     <service_actions>
     *         Stop authserver. Configure security providers correctly in
     *         java.security policy file. Restart authserver.
     *     </service_actions>
     * </IEM_INLINE_DOCUMENTATION>
     *
     */
    /*
     * Calculate the HMAC using SHA-256.
     */
    public static byte[] hmacSHA256(byte[] key, byte[] data) {
        Mac mac;
        try {
            mac = Mac.getInstance("HmacSHA256");
            mac.init(new SecretKeySpec(key, "HmacSHA256"));

            return mac.doFinal(data);
        } catch (NoSuchAlgorithmException ex) {
            IEMUtil.log(IEMUtil.Level.ERROR, IEMUtil.HMACSHA256_UNAVAILABLE,
                    "Algorithm HmacSHA256 not available", null);
        } catch (InvalidKeyException ex) {
        }

        return null;
    }

    /*
     * <IEM_INLINE_DOCUMENTATION>
     *     <event_code>048002003</event_code>
     *     <application>S3 Authserver</application>
     *     <submodule>JRE</submodule>
     *     <description>Algorithm HmacSHA1 not available</description>
     *     <audience>Service</audience>
     *     <details>
     *         Algorithm HmacSHA1 is not available.
     *         The data section of the event has following keys:
     *           time - timestamp
     *           node - node name
     *           pid - process id of Authserver
     *           file - source code filename
     *           line - line number within file where error occurred
     *     </details>
     *     <service_actions>
     *         Stop authserver. Configure security providers correctly in
     *         java.security policy file. Restart authserver.
     *     </service_actions>
     * </IEM_INLINE_DOCUMENTATION>
     *
     */
    /*
     * Calculate the HMAC using SHA-1.
     */
    public static byte[] hmacSHA1(byte[] key, byte[] data) {
        Mac mac;
        try {
            mac = Mac.getInstance("HmacSHA1");
            mac.init(new SecretKeySpec(key, "HmacSHA1"));

            return mac.doFinal(data);
        } catch (NoSuchAlgorithmException ex) {
            IEMUtil.log(IEMUtil.Level.ERROR, IEMUtil.HMACSHA1_UNAVAILABLE,
                    "Algorithm HmacSHA1 not available", null);
        } catch (InvalidKeyException ex) {
        }

        return null;
    }

    /*
     * <IEM_INLINE_DOCUMENTATION>
     *     <event_code>048002004</event_code>
     *     <application>S3 Authserver</application>
     *     <submodule>JRE</submodule>
     *     <description>Algorithm SHA-256 not available</description>
     *     <audience>Service</audience>
     *     <details>
     *         Algorithm SHA-256 is not available.
     *         The data section of the event has following keys:
     *           time - timestamp
     *           node - node name
     *           pid - process id of Authserver
     *           file - source code filename
     *           line - line number within file where error occurred
     *     </details>
     *     <service_actions>
     *         Stop authserver. Configure security providers correctly in
     *         java.security policy file. Restart authserver.
     *     </service_actions>
     * </IEM_INLINE_DOCUMENTATION>
     *
     */
    /*
     * Hash the text using SHA-256 algorithm.
     */
    public static byte[] hashSHA256(String text) {
        MessageDigest md;
        try {
            md = MessageDigest.getInstance("SHA-256");
            md.update(text.getBytes());

            return md.digest();
        } catch (NoSuchAlgorithmException ex) {
            IEMUtil.log(IEMUtil.Level.ERROR, IEMUtil.SHA256_UNAVAILABLE,
                    "Algorithm SHA-256 not available", null);
        }

        return null;
    }

    /*
     * Hash the text using SHA-256 algorithm.
     */
    public static byte[] hashSHA256(byte[] text) {
        MessageDigest md;
        try {
            md = MessageDigest.getInstance("SHA-256");
            md.update(text);

            return md.digest();
        } catch (NoSuchAlgorithmException ex) {
            IEMUtil.log(IEMUtil.Level.ERROR, IEMUtil.SHA256_UNAVAILABLE,
                    "Algorithm SHA-256 not available", null);
        }

        return null;
    }

    /*
     * Convert the byte array to hex representation.
     */
    public static String toHex(byte[] src) {
        return toString(encodeToHex(src));
    }

     /*
      * Return a base 64 encoded UUID.
      * This uuid might contains speical characters i.e "-","_"
      */
    public static String base64UUID() {
        return encodeToUrlSafeBase64String(getRandomUUIDAsByteArray());
    }

    /*
     * Generate random alphanumeric UUID string of length 32 characters
     * e.g 8048ddd913c540d29ddac1f174aa1072
     */
   public
    static String getAlphaNumericUUID() {
      UUID uid = UUID.randomUUID();
      // uid will be in format of "8048ddd9-13c5-40d2-9dda-c1f174aa1072" of
      // length 36 characters
      // after replacing "-" with "", uid becomes 32 character string
      String id = uid.toString().replace("-", "");
      return id;
    }

    /*
     * Generate random UUID
     */
    public static byte[] getRandomUUIDAsByteArray() {
        UUID uid = UUID.randomUUID();
        ByteBuffer bb = ByteBuffer.wrap(new byte[16]);
        bb.putLong(uid.getMostSignificantBits());
        bb.putLong(uid.getLeastSignificantBits());

        return bb.array();
    }

    /*
     * Encode byte array into url safe base64 string
     */
    public static String encodeToUrlSafeBase64String(byte[] text) {
        return Base64.encodeBase64URLSafeString(text);
    }

    /*
     * Return true if the text is base 64 encoded.
     */
    public static Boolean isBase64Encoded(String text) {
        return Base64.isBase64(text);
    }

    /*
     * Decode the base 64 text and return the bytes.
     */
    public static byte[] base64DecodedBytes(String text) {
        return Base64.decodeBase64(text);
    }

    /*
     * Decode base 64 string and return the string.
     */
    public static String base64DecodeString(String text) {
        return new String(base64DecodedBytes(text));
    }

    /*
     * TODO
     * Replace encodeToBase64 with encodeToUrlSafeBase64.
     * Encode to base 64 format and return bytes.
     */
    public static byte[] encodeToBase64Bytes(String text) {
        return Base64.encodeBase64(text.getBytes());
    }

    /*
     * Encode the text to base 64 and return the string.
     */
    public static String encodeToBase64String(byte[] text) {
        return Base64.encodeBase64String(text);
    }

    public static String encodeToBase64String(String text) {
        return encodeToBase64String(text.getBytes());
    }

    /*
     * Convert bytes to String
     */
    private static String toString(byte[] bytes) {
        final char[] dest = new char[bytes.length];
        int i = 0;

        for (byte b : bytes) {
            dest[i++] = (char) b;
        }

        return new String(dest);
    }

    /*
     * Convert the bytes into its hex representation.
     * Each byte will be represented with 2 hex characters.
     * Use lower case alphabets.
     */
    public static byte[] encodeToHex(byte[] src) {
        byte[] dest = new byte[src.length * 2];
        byte p;

        for (int i = 0, j = 0; i < src.length; i++) {
            dest[j++] = (byte) hexChars[(p = src[i]) >>> 4 & MASK_4BITS];
            dest[j++] = (byte) hexChars[p & MASK_4BITS];
        }
        return dest;
    }
}
