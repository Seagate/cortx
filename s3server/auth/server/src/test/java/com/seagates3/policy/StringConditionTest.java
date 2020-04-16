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
 * Original creation date: 08-January-2020
 */

package com.seagates3.policy;

import static org.junit.Assert.*;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.junit.BeforeClass;
import org.junit.Test;

import com.seagates3.policy.StringCondition.StringComparisonType;

public
class StringConditionTest {

  StringCondition condition = null;
  static List<String> values = new ArrayList<>();
  Map<String, String> requestBody = null;
  String key = "x-amz-acl";
  String invalidKeyHeader = "x-amz";
  String invalidValue = "garbage";

  @BeforeClass public static void setUpBeforeClass() {
    values.add("abc");
    values.add("Pqr");
    values.add("qwe*");
    values.add("");
  }

  @Test public void testIsSatisfied_stringEquals_success() {
    condition =
        new StringCondition(StringComparisonType.StringEquals, key, values);
    requestBody = new HashMap<>();
    requestBody.put(key, "abc");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_stringEquals_fail() {
    condition =
        new StringCondition(StringComparisonType.StringEquals, key, values);
    requestBody = new HashMap<>();
    requestBody.put(key, "Abc");
    assertFalse(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_stringNotEquals_success() {
    condition =
        new StringCondition(StringComparisonType.StringNotEquals, key, values);
    requestBody = new HashMap<>();
    requestBody.put(key, "Abc");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_stringNotEquals_fail() {
    condition =
        new StringCondition(StringComparisonType.StringNotEquals, key, values);
    requestBody = new HashMap<>();
    requestBody.put(key, "abc");
    assertFalse(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_stringEqualsIgnoreCase_success() {
    condition = new StringCondition(StringComparisonType.StringEqualsIgnoreCase,
                                    key, values);
    requestBody = new HashMap<>();
    requestBody.put(key, "Abc");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_stringEqualsIgnoreCase_fail() {
    condition = new StringCondition(StringComparisonType.StringEqualsIgnoreCase,
                                    key, values);
    requestBody = new HashMap<>();
    requestBody.put(key, "xyz");
    assertFalse(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_stringNotEqualsIgnoreCase_success() {
    condition = new StringCondition(
        StringComparisonType.StringNotEqualsIgnoreCase, key, values);
    requestBody = new HashMap<>();
    requestBody.put(key, "xyz");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_stringNotEqualsIgnoreCase_fail() {
    condition = new StringCondition(
        StringComparisonType.StringNotEqualsIgnoreCase, key, values);
    requestBody = new HashMap<>();
    requestBody.put(key, "Abc");
    assertFalse(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_stringLike_success() {
    condition =
        new StringCondition(StringComparisonType.StringLike, key, values);
    requestBody = new HashMap<>();
    requestBody.put(key, "qwerty");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_stringLike_fail() {
    condition =
        new StringCondition(StringComparisonType.StringLike, key, values);
    requestBody = new HashMap<>();
    requestBody.put(key, "bqwer");
    assertFalse(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_stringNotLike_success() {
    condition =
        new StringCondition(StringComparisonType.StringNotLike, key, values);
    requestBody = new HashMap<>();
    requestBody.put(key, "bqwer");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_stringNotLike_fail() {
    condition =
        new StringCondition(StringComparisonType.StringNotLike, key, values);
    requestBody = new HashMap<>();
    requestBody.put(key, "qwer");
    assertFalse(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_stringEqualsIfExists_invalidKey_success() {
    condition = new StringCondition(StringComparisonType.StringEqualsIfExists,
                                    key, values);
    requestBody = new HashMap<>();
    requestBody.put(invalidKeyHeader, invalidValue);
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_stringEqualsIfExists_validKey_success() {
    condition = new StringCondition(StringComparisonType.StringEqualsIfExists,
                                    key, values);
    requestBody = new HashMap<>();
    requestBody.put(key, "Pqr");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_stringEqualsIfExists_fail() {
    condition = new StringCondition(StringComparisonType.StringEqualsIfExists,
                                    key, values);
    requestBody = new HashMap<>();
    requestBody.put(key, invalidValue);
    assertFalse(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_stringNotEqualsIfExists_success() {
    condition = new StringCondition(
        StringComparisonType.StringNotEqualsIfExists, key, values);
    requestBody = new HashMap<>();
    requestBody.put(invalidKeyHeader, invalidValue);
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_stringNotEqualsIfExists_fail() {
    condition = new StringCondition(
        StringComparisonType.StringNotEqualsIfExists, key, values);
    requestBody = new HashMap<>();
    requestBody.put(key, "abc");
    assertFalse(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_stringEqualsIgnoreCaseIfExists_success1() {
    condition = new StringCondition(
        StringComparisonType.StringEqualsIgnoreCaseIfExists, key, values);
    requestBody = new HashMap<>();
    requestBody.put(key, "pqr");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_stringEqualsIgnoreCaseIfExists_success2() {
    condition = new StringCondition(
        StringComparisonType.StringEqualsIgnoreCaseIfExists, key, values);
    requestBody = new HashMap<>();
    requestBody.put(invalidKeyHeader, "pqrst");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_stringEqualsIgnoreCaseIfExists_fail() {
    condition = new StringCondition(
        StringComparisonType.StringEqualsIgnoreCaseIfExists, key, values);
    requestBody = new HashMap<>();
    requestBody.put(key, "pqrst");
    assertFalse(condition.isSatisfied(requestBody));
  }

  @Test public void
  testIsSatisfied_stringNotEqualsIgnoreCaseIfExists_success() {
    condition = new StringCondition(
        StringComparisonType.StringNotEqualsIgnoreCaseIfExists, key, values);
    requestBody = new HashMap<>();
    requestBody.put(invalidKeyHeader, "pqrst");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_stringNotEqualsIgnoreCaseIfExists_fail() {
    condition = new StringCondition(
        StringComparisonType.StringNotEqualsIgnoreCaseIfExists, key, values);
    requestBody = new HashMap<>();
    requestBody.put(key, "pqr");
    assertFalse(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_stringLikeIfExists_success() {
    condition = new StringCondition(StringComparisonType.StringLikeIfExists,
                                    key, values);
    requestBody = new HashMap<>();
    requestBody.put(invalidKeyHeader, "pqrst");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_stringLikeIfExists_fail() {
    condition = new StringCondition(StringComparisonType.StringLikeIfExists,
                                    key, values);
    requestBody = new HashMap<>();
    requestBody.put(key, "pqrst");
    assertFalse(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_stringNotLikeIfExists_success1() {
    condition = new StringCondition(StringComparisonType.StringNotLikeIfExists,
                                    key, values);
    requestBody = new HashMap<>();
    requestBody.put(invalidKeyHeader, "pqr");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_stringNotLikeIfExists_success2() {
    condition = new StringCondition(StringComparisonType.StringNotLikeIfExists,
                                    key, values);
    requestBody = new HashMap<>();
    requestBody.put(key, "pqrst");
    assertTrue(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_stringNotLikeIfExists_fail() {
    condition = new StringCondition(StringComparisonType.StringNotLikeIfExists,
                                    key, values);
    requestBody = new HashMap<>();
    requestBody.put(key, "Pqr");
    assertFalse(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_stringEquals_keyNull_fail() {
    condition =
        new StringCondition(StringComparisonType.StringEquals, null, values);
    requestBody = new HashMap<>();
    requestBody.put(key, "abc");
    assertFalse(condition.isSatisfied(requestBody));
  }

  @Test public void testIsSatisfied_stringEquals_valuesNull_fail() {
    condition =
        new StringCondition(StringComparisonType.StringEquals, key, null);
    requestBody = new HashMap<>();
    requestBody.put(key, "abc");
    assertFalse(condition.isSatisfied(requestBody));
  }
}
