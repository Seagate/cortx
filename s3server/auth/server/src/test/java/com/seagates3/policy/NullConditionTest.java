/*
 * COPYRIGHT 2019 SEAGATE LLC
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
 * Original author:  Ajinkya Dhumal
 * Original creation date: 09-January-2019
 */

package com.seagates3.policy;

import static org.junit.Assert.*;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.junit.Before;
import org.junit.Test;

public
class NullConditionTest {

  NullCondition condition = null;
  List<String> values = null;
  Map<String, String> requestBody = null;
  String key = "x-amz-acl";
  String invalidKeyHeader = "x-amz";

  @Before public void setUp() throws Exception {
    values = new ArrayList<>();
    requestBody = new HashMap<String, String>();
  }

  @Test public void testIsSatisfied_true_success() {
    values.add("true");
    condition = new NullCondition(key, values);
    requestBody.put(invalidKeyHeader, "abc");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_true_fail() {
    values.add("true");
    condition = new NullCondition(key, values);
    requestBody.put(key, "abc");
    assertFalse(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_true_keyNull_success() {
    values.add("true");
    condition = new NullCondition(null, values);
    requestBody.put(key, "abc");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_false_keyNull_fail() {
    values.add("false");
    condition = new NullCondition(null, values);
    requestBody.put(key, "abc");
    assertFalse(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_true_valueNull_success() {
    values.add("true");
    condition = new NullCondition(key, null);
    requestBody.put(key, "abc");
    assertFalse(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_false_success() {
    values.add("false");
    condition = new NullCondition(key, values);
    requestBody.put(key, "abc");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_false_success2() {
    values.add("false");
    condition = new NullCondition(key, values);
    requestBody.put(key, "");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_false_fail() {
    values.add("false");
    condition = new NullCondition(key, values);
    requestBody.put(key, null);
    assertFalse(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_false_fail2() {
    values.add("false");
    condition = new NullCondition(key, values);
    requestBody.put(invalidKeyHeader, "abc");
    assertFalse(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_trueAndfalse_success() {
    values.add("true");
    values.add("false");
    condition = new NullCondition(key, values);
    requestBody.put(key, "abc");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_trueAndfalse_success3() {
    values.add("true");
    values.add("false");
    condition = new NullCondition(key, values);
    requestBody.put(invalidKeyHeader, "abc");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_trueAndfalse_fail() {
    values.add("true");
    values.add("false");
    condition = new NullCondition(key, values);
    requestBody.put(key, null);
    assertFalse(condition.isSatisfied(requestBody));
  }
}
