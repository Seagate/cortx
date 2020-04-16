package com.seagates3.util;

import java.io.UnsupportedEncodingException;

public class AWSSignUtil {

    /**
     * Calculates signature using AWS V2 Sign method
     * @param stringToSign
     * @param secretKey
     * @return String, null in case of error
     */
    public static String calculateSignatureAWSV2(String stringToSign,
                                                  String secretKey) {

        byte[] kStringToSign = null;
        try {
            kStringToSign = BinaryUtil.hmacSHA1(
                    secretKey.getBytes(),
                    stringToSign.getBytes("UTF-8"));
            return BinaryUtil.encodeToBase64String(kStringToSign);
        } catch (UnsupportedEncodingException ex) {
            IEMUtil.log(IEMUtil.Level.ERROR, IEMUtil.UTF8_UNAVAILABLE,
                    "UTF-8 encoding is not supported", null);
        }
        return null;
    }

}
