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

import org.junit.BeforeClass;
import org.junit.Test;

import com.seagates3.policy.NumericCondition.NumericComparisonType;

public
class NumericConditionTest {

  NumericCondition condition = null;
  static List<String> values = new ArrayList<>();
  Map<String, String> requestBody = null;
  String key = "max-keys";
  String invalidKeyHeader = "x-amz-acl";
  String invalidValue = "garbage";

  @BeforeClass public static void setUpBeforeClass() throws Exception {
    values.add("10");
  }

  @Test public void testIsSatisfied_numericEquals_success() {
    condition =
        new NumericCondition(NumericComparisonType.NumericEquals, key, values);
    requestBody = new HashMap<String, String>();
    requestBody.put(key, "10");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_numericEquals_fail() {
    condition =
        new NumericCondition(NumericComparisonType.NumericEquals, key, values);
    requestBody = new HashMap<String, String>();
    requestBody.put(key, "11");
    assertFalse(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_numericNotEquals_success() {
    condition = new NumericCondition(NumericComparisonType.NumericNotEquals,
                                     key, values);
    requestBody = new HashMap<String, String>();
    requestBody.put(key, "11");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_numericNotEquals_fail() {
    condition = new NumericCondition(NumericComparisonType.NumericNotEquals,
                                     key, values);
    requestBody = new HashMap<String, String>();
    requestBody.put(key, "10");
    assertFalse(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_numericGreaterThan_success() {
    condition = new NumericCondition(NumericComparisonType.NumericGreaterThan,
                                     key, values);
    requestBody = new HashMap<String, String>();
    requestBody.put(key, "11");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_numericGreaterThan_fail() {
    condition = new NumericCondition(NumericComparisonType.NumericGreaterThan,
                                     key, values);
    requestBody = new HashMap<String, String>();
    requestBody.put(key, "10");
    assertFalse(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_numericGreaterThanEquals_success() {
    condition = new NumericCondition(
        NumericComparisonType.NumericGreaterThanEquals, key, values);
    requestBody = new HashMap<String, String>();
    requestBody.put(key, "10");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_numericGreaterThanEquals_success2() {
    condition = new NumericCondition(
        NumericComparisonType.NumericGreaterThanEquals, key, values);
    requestBody = new HashMap<String, String>();
    requestBody.put(key, "11");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_numericGreaterThanEquals_fail() {
    condition = new NumericCondition(
        NumericComparisonType.NumericGreaterThanEquals, key, values);
    requestBody = new HashMap<String, String>();
    requestBody.put(key, "9");
    assertFalse(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_numericLessThan_success() {
    condition = new NumericCondition(NumericComparisonType.NumericLessThan, key,
                                     values);
    requestBody = new HashMap<String, String>();
    requestBody.put(key, "-1");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_numericLessThan_fail() {
    condition = new NumericCondition(NumericComparisonType.NumericLessThan, key,
                                     values);
    requestBody = new HashMap<String, String>();
    requestBody.put(key, "11");
    assertFalse(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_numericLessThanEquals_success() {
    condition = new NumericCondition(NumericComparisonType.NumericLessThan, key,
                                     values);
    requestBody = new HashMap<String, String>();
    requestBody.put(key, "9");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_numericLessThanEquals_success2() {
    condition = new NumericCondition(
        NumericComparisonType.NumericLessThanEquals, key, values);
    requestBody = new HashMap<String, String>();
    requestBody.put(key, "10");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_numericLessThanEquals_fail() {
    condition = new NumericCondition(
        NumericComparisonType.NumericLessThanEquals, key, values);
    requestBody = new HashMap<String, String>();
    requestBody.put(key, "11");
    assertFalse(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_numericEqualsIfExists_success() {
    condition = new NumericCondition(
        NumericComparisonType.NumericEqualsIfExists, key, values);
    requestBody = new HashMap<String, String>();
    requestBody.put(invalidKeyHeader, "10");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_numericEqualsIfExists_success2() {
    condition = new NumericCondition(
        NumericComparisonType.NumericEqualsIfExists, key, values);
    requestBody = new HashMap<String, String>();
    requestBody.put(key, "10");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_numericEqualsIfExists_fail() {
    condition = new NumericCondition(
        NumericComparisonType.NumericEqualsIfExists, key, values);
    requestBody = new HashMap<String, String>();
    requestBody.put(key, "11");
    assertFalse(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_numericNotEqualsIfExists_success() {
    condition = new NumericCondition(
        NumericComparisonType.NumericNotEqualsIfExists, key, values);
    requestBody = new HashMap<String, String>();
    requestBody.put(invalidKeyHeader, "10");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_numericNotEqualsIfExists_success2() {
    condition = new NumericCondition(
        NumericComparisonType.NumericNotEqualsIfExists, key, values);
    requestBody = new HashMap<String, String>();
    requestBody.put(key, "11");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_numericNotEqualsIfExists_fail() {
    condition = new NumericCondition(
        NumericComparisonType.NumericNotEqualsIfExists, key, values);
    requestBody = new HashMap<String, String>();
    requestBody.put(key, "10");
    assertFalse(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_numericGreaterThanIfExists_success() {
    condition = new NumericCondition(
        NumericComparisonType.NumericGreaterThanIfExists, key, values);
    requestBody = new HashMap<String, String>();
    requestBody.put(invalidKeyHeader, "8");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_numericGreaterThanIfExists_fail() {
    condition = new NumericCondition(
        NumericComparisonType.NumericGreaterThanIfExists, key, values);
    requestBody = new HashMap<String, String>();
    requestBody.put(key, "9");
    assertFalse(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_numericGreaterThanEqualsIfExists_success() {
    condition = new NumericCondition(
        NumericComparisonType.NumericGreaterThanEqualsIfExists, key, values);
    requestBody = new HashMap<String, String>();
    requestBody.put(invalidKeyHeader, "9");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_numericGreaterThanEqualsIfExists_fail() {
    condition = new NumericCondition(
        NumericComparisonType.NumericGreaterThanEqualsIfExists, key, values);
    requestBody = new HashMap<String, String>();
    requestBody.put(key, "9");
    assertFalse(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_numericLessThanIfExists_success() {
    condition = new NumericCondition(
        NumericComparisonType.NumericLessThanIfExists, key, values);
    requestBody = new HashMap<String, String>();
    requestBody.put(invalidKeyHeader, "11");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_numericLessThanIfExists_success2() {
    condition = new NumericCondition(
        NumericComparisonType.NumericLessThanIfExists, key, values);
    requestBody = new HashMap<String, String>();
    requestBody.put(key, "9");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_numericLessThanIfExists_fail() {
    condition = new NumericCondition(
        NumericComparisonType.NumericLessThanIfExists, key, values);
    requestBody = new HashMap<String, String>();
    requestBody.put(key, "11");
    assertFalse(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_numericLessThanEqualsIfExists_success() {
    condition = new NumericCondition(
        NumericComparisonType.NumericLessThanEqualsIfExists, key, values);
    requestBody = new HashMap<String, String>();
    requestBody.put(invalidKeyHeader, "11");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_numericLessThanEqualsIfExists_success2() {
    condition = new NumericCondition(
        NumericComparisonType.NumericLessThanEqualsIfExists, key, values);
    requestBody = new HashMap<String, String>();
    requestBody.put(key, "10");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_numericLessThanEqualsIfExists_fail() {
    condition = new NumericCondition(
        NumericComparisonType.NumericLessThanEqualsIfExists, key, values);
    requestBody = new HashMap<String, String>();
    requestBody.put(key, "11");
    assertFalse(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_numericEquals_nullKey_fail() {
    condition =
        new NumericCondition(NumericComparisonType.NumericEquals, null, values);
    requestBody = new HashMap<String, String>();
    requestBody.put(key, "10");
    assertFalse(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_numericEquals_nullValues_fail() {
    condition =
        new NumericCondition(NumericComparisonType.NumericEquals, key, null);
    requestBody = new HashMap<String, String>();
    requestBody.put(key, "10");
    assertFalse(condition.isSatisfied(requestBody));
  }
}
