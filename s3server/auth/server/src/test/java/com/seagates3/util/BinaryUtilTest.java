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
 * Original creation date: 10-Jan-2017
 */
package com.seagates3.util;

import org.junit.Test;

import java.io.UnsupportedEncodingException;

import static org.junit.Assert.*;

public class BinaryUtilTest {

    @Test
    public void hexEncodedHashTest_String() {
        String expected = "B62D867A2B6874DFFD2DD402FB912960766CB83ADE6133365F5D51400311C968";

        String result = BinaryUtil.hexEncodedHash("The Great A.I. Awakening");

        assertNotNull(result);
        assertEquals(expected.toLowerCase(), result);
    }

    @Test(expected = NullPointerException.class)
    public void hexEncodedHashTest_String_ShouldThrowNullPointerException() {
        String text = null;
        BinaryUtil.hexEncodedHash(text);
    }

    @Test
    public void hexEncodedHashTest_Bytes() throws UnsupportedEncodingException {
        byte[] text = "The Great A.I. Awakening".getBytes("UTF-8");
        String expected = "B62D867A2B6874DFFD2DD402FB912960766CB83ADE6133365F5D51400311C968";

        String result = BinaryUtil.hexEncodedHash(text);

        assertNotNull(result);
        assertEquals(expected.toLowerCase(), result);
    }

    @Test
    public void base64EncodedHashTest() {
        String input = "The Great A.I. Awakening";
        String expected = "ti2GeitodN_9LdQC-5EpYHZsuDreYTM2X11RQAMRyWg";

        String result = BinaryUtil.base64EncodedHash(input);

        assertNotNull(result);
        assertEquals(expected, result);
    }

    @Test
    public void hmacSHA256Test() throws UnsupportedEncodingException {
        byte[] key = "ti2GeitodN_9LdQC-5EpYHZsuDreYTM2X11RQAMRyWg".getBytes("UTF-8");
        byte[] data = "The Great A.I. Awakening".getBytes("UTF-8");
        byte[] expected = new byte[] {83, 66, 87, 64, 92, 0, -60, -46, -68, 34, 70, -32,
                -37, 54,115, 28, -100, 105, 104, 26, 116, -124, 80, -61, 123, 38, -26, -46,
                -69, 76, -14, -123};

        byte[] result = BinaryUtil.hmacSHA256(key, data);

        assertNotNull(result);
        assertArrayEquals(expected, result);
    }

    @Test(expected = IllegalArgumentException.class)
    public void hmacSHA256Test_Null_ShouldThrowIllegalArgumentException()
            throws UnsupportedEncodingException {
        byte[] key = null;
        byte[] data = "The Great A.I. Awakening".getBytes("UTF-8");

        BinaryUtil.hmacSHA256(key, data);
    }

    @Test
    public void hmacSHA1Test() throws UnsupportedEncodingException {
        byte[] key = "ti2GeitodN_9LdQC-5EpYHZsuDreYTM2X11RQAMRyWg".getBytes("UTF-8");
        byte[] data = "The Great A.I. Awakening".getBytes("UTF-8");
        byte[] expected = new byte[] {-59, -126, -87, 14, -78, 39, 34, 124, 53, 127,
                -114, -30, -92, 90, 22, 66, 92, 61, -83, -14};

        byte[] result = BinaryUtil.hmacSHA1(key, data);

        assertNotNull(result);
        assertArrayEquals(expected, result);
    }

    @Test(expected = IllegalArgumentException.class)
    public void hmacSHA1Test_Null_ShouldThrowIllegalArgumentException()
            throws UnsupportedEncodingException {
        byte[] key = null;
        byte[] data = "The Great A.I. Awakening".getBytes("UTF-8");

        BinaryUtil.hmacSHA1(key, data);
    }

    @Test
    public void hashSHA256Test() {
        String text = "The Great A.I. Awakening";
        byte[] expected = new byte[] {-74, 45, -122, 122, 43, 104, 116, -33, -3,
                45, -44, 2, -5, -111, 41, 96, 118, 108, -72, 58, -34, 97, 51, 54,
                95, 93, 81, 64, 3, 17, -55, 104};

        byte result[] = BinaryUtil.hashSHA256(text);

        assertNotNull(result);
        assertArrayEquals(expected, result);
    }

    @Test
    public void hashSHA256Test_Byte() throws UnsupportedEncodingException {
        byte[] bytes = "The Great A.I. Awakening".getBytes("UTF-8");
        byte[] expected = new byte[] {-74, 45, -122, 122, 43, 104, 116, -33, -3,
                45, -44, 2, -5, -111, 41, 96, 118, 108, -72, 58, -34, 97, 51, 54,
                95, 93, 81, 64, 3, 17, -55, 104};

        byte[] result = BinaryUtil.hashSHA256(bytes);

        assertNotNull(result);
        assertArrayEquals(expected, result);
    }

    @Test(expected = NullPointerException.class)
    public void hashSHA256Test_ShouldThrowNullPointerException() {
        String text = null;

        BinaryUtil.hashSHA256(text);
    }

    @Test
    public void toHexTest() throws UnsupportedEncodingException {
        byte[] bytes = "The Great A.I. Awakening".getBytes("UTF-8");
        String expected = "54686520477265617420412e492e204177616b656e696e67";

        String result = BinaryUtil.toHex(bytes);

        assertNotNull(result);
        assertEquals(expected, result);
    }

    @Test
    public void base64UUIDTest() {
        String result = BinaryUtil.base64UUID();

        assertNotNull(result);
        assertTrue(BinaryUtil.isBase64Encoded(result));
    }

    @Test
    public void encodeToUrlSafeBase64StringTest() throws UnsupportedEncodingException {
        byte[] bytes = "The Great A.I. Awakening".getBytes("UTF-8");

        String result = BinaryUtil.encodeToUrlSafeBase64String(bytes);

        assertNotNull(result);
        assertFalse(result.contains("/"));
        assertFalse(result.contains(" "));
    }

    @Test
    public void isBase64EncodedTest() {
        String input = "VGhlIEdyZWF0IEEuSS4gQXdha2VuaW5n";

        Boolean result = BinaryUtil.isBase64Encoded(input);

        assertTrue(result);
    }

    @Test
    public void isBase64EncodedTest_False() {
        String input = "VGhlIEdyZWF0IEE{}uSS4gQXdha2VuaW5n";

        Boolean result = BinaryUtil.isBase64Encoded(input);

        assertFalse(result);
    }

    @Test
    public void base64DecodedBytesTest() throws UnsupportedEncodingException {
        String data = "VGhlIEdyZWF0IEEuSS4gQXdha2VuaW5n";

        byte[] result = BinaryUtil.base64DecodedBytes(data);

        assertNotNull(result);
    }

    @Test
    public void base64DecodeStringTest() throws UnsupportedEncodingException {
        String data = "VGhlIEdyZWF0IEEuSS4gQXdha2VuaW5n";
        String expected = "The Great A.I. Awakening";

        String result = BinaryUtil.base64DecodeString(data);

        assertNotNull(result);
        assertEquals(expected, result);
    }

    @Test
    public void encodeToBase64BytesTest() {
        String input = "The Great A.I. Awakening";
        byte[] expected = new byte[] {86, 71, 104, 108, 73, 69, 100, 121, 90, 87, 70, 48, 73,69,
                69, 117, 83, 83, 52, 103, 81, 88, 100, 104, 97, 50, 86, 117, 97, 87, 53, 110};

        byte[] result = BinaryUtil.encodeToBase64Bytes(input);

        assertNotNull(result);
        assertArrayEquals(expected, result);
    }

    @Test
    public void encodeToBase64StringTest_Byte() throws UnsupportedEncodingException {
        byte[] input = "The Great A.I. Awakening".getBytes("UTF-8");
        String expected = "VGhlIEdyZWF0IEEuSS4gQXdha2VuaW5n";

        String result = BinaryUtil.encodeToBase64String(input);

        assertNotNull(result);
        assertEquals(expected, result);
    }

    @Test
    public void encodeToBase64StringTest_String() {
        String input = "The Great A.I. Awakening";
        String expected = "VGhlIEdyZWF0IEEuSS4gQXdha2VuaW5n";

        String result = BinaryUtil.encodeToBase64String(input);

        assertNotNull(result);
        assertEquals(expected, result);
    }

    @Test
    public void encodeToHexTest() throws UnsupportedEncodingException {
        byte[] input = "The Great A.I. Awakening".getBytes("UTF-8");
        byte[] expected = new byte[] {53, 52, 54, 56, 54, 53, 50, 48, 52, 55, 55, 50, 54, 53,
                54, 49, 55, 52, 50, 48, 52, 49, 50, 101, 52, 57, 50, 101, 50, 48, 52, 49, 55,
                55, 54, 49, 54, 98, 54, 53, 54, 101, 54, 57, 54, 101, 54, 55};

        byte[] result = BinaryUtil.encodeToHex(input);

        assertNotNull(result);
        assertArrayEquals(expected, result);
    }
}
