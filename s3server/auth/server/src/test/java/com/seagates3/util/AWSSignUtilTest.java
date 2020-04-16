package com.seagates3.util;

import static org.junit.Assert.*;

import org.junit.Test;

public class AWSSignUtilTest {

    @Test
    public void testCalculateSignatureAWSV2() {

        String d = "Tue, 10 Jul 2018 06:38:28 GMT";
        String payload = "POST" + "\n\n\n" + d + "\n" + "/account/actid1";
        String secretKey = "kjldjsoijweoiiwjiwfjjijwpeijfpfjpfjep+kdo=";

        String expectedSignedValue = "cKqcWGZFIFqFYufbCeBXVhpXXm0=";

        String signedValue = AWSSignUtil.calculateSignatureAWSV2(payload, secretKey);

        assertTrue(signedValue.equals(expectedSignedValue));

    }

}
