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
 * Original creation date: 09-Feb-2017
 */

package com.seagates3.fi;

import org.junit.Test;

import static org.junit.Assert.*;

public class FaultPointTest {

    private FaultPoint faultPoint;

    @Test
    public void isActiveTest_FAIL_ONCE() {
        faultPoint = new FaultPoint("LDAP_SEARCH", "FAIL_ONCE", 0);

        assertTrue(faultPoint.isActive());
        assertFalse(faultPoint.isActive());
        assertFalse(faultPoint.isActive());
    }

    @Test
    public void isActiveTest_FAIL_N_TIMES() {
        faultPoint = new FaultPoint("LDAP_SEARCH", "FAIL_N_TIMES", 2);

        assertTrue(faultPoint.isActive());
        assertTrue(faultPoint.isActive());
        assertFalse(faultPoint.isActive());
        assertFalse(faultPoint.isActive());
    }

    @Test
    public void isActiveTest_FAIL_0_TIMES() {
        faultPoint = new FaultPoint("LDAP_SEARCH", "FAIL_N_TIMES", 0);

        assertFalse(faultPoint.isActive());
        assertFalse(faultPoint.isActive());
    }

    @Test
    public void isActiveTest_SKIP_FIRST_N_TIMES() {
        faultPoint = new FaultPoint("LDAP_SEARCH", "SKIP_FIRST_N_TIMES", 2);

        assertFalse(faultPoint.isActive());
        assertFalse(faultPoint.isActive());
        assertTrue(faultPoint.isActive());
        assertTrue(faultPoint.isActive());
        assertTrue(faultPoint.isActive());
    }

    @Test
    public void isActiveTest_SKIP_FIRST_0_TIMES() {
        faultPoint = new FaultPoint("LDAP_SEARCH", "SKIP_FIRST_N_TIMES", 0);

        assertTrue(faultPoint.isActive());
        assertTrue(faultPoint.isActive());
        assertTrue(faultPoint.isActive());
    }

    @Test
    public void isActiveTest_FAIL_ALWAYS() {
        faultPoint = new FaultPoint("LDAP_SEARCH", "FAIL_ALWAYS", 0);

        assertTrue(faultPoint.isActive());
        assertTrue(faultPoint.isActive());
        assertTrue(faultPoint.isActive());
    }
}