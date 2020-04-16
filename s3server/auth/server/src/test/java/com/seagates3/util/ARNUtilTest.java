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

import static org.junit.Assert.*;

public class ARNUtilTest {

    @Test
    public void createARNTest() {
        String accountId = "testID";
        String resource = "testResource";
        String expected = "arn:seagate:iam::testID:testResource";

        String result = ARNUtil.createARN(accountId, resource);

        assertNotNull(result);
        assertEquals(expected, result);
    }

    @Test
    public void createARNTest_WithResourceType() {
        String accountId = "testID";
        String resource = "testResource";
        String resourceType = "testType";
        String expected = "arn:seagate:iam::testID:testType/testResource";

        String result = ARNUtil.createARN(accountId, resourceType, resource);

        assertNotNull(result);
        assertEquals(expected, result);
    }
}
