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
 * Original creation date: 26-December-2019
 */

package com.seagates3.policy;

import java.util.List;
import java.util.Map;
import java.util.Map.Entry;

public
class StringCondition extends PolicyCondition {

 public
  static enum StringComparisonType {
    StringEquals,
    StringEqualsIgnoreCase,
    StringLike,
    StringNotEquals,
    StringNotEqualsIgnoreCase,
    StringNotLike,
    StringEqualsIfExists,
    StringEqualsIgnoreCaseIfExists,
    StringLikeIfExists,
    StringNotEqualsIfExists,
    StringNotEqualsIgnoreCaseIfExists,
    StringNotLikeIfExists;
  }

 public StringCondition(StringComparisonType type, String key,
                        List<String> values) {
    if (type == null)
      super.type = null;
    else
      super.type = type.toString();
    if (key == null)
      super.conditionKey = null;
    else
      super.conditionKey = key.toLowerCase();
    super.values = values;
  }

  /**
   * Checks if this String Condition is satisfied from the request.
   * Sample policy condition -
   * "Condition": {
   *     "StringEquals": {
   *       "s3:x-amz-acl": ["bucket-owner-read", "bucket-owner-full-control"]
   *     },
   *     "StringNotEquals": {
   *       "s3:x-amz-acl": "public-read-write"
   *     }
   *  }
   */
  @Override public boolean isSatisfied(Map<String, String> requestBody) {
    if (this.values == null) return false;
    StringComparisonType enumType = StringComparisonType.valueOf(this.type);
    if (enumType == null) return false;

    // Fetch the header value for corresponding Condition key
    String headerVal = null;
    for (Entry<String, String> entry : requestBody.entrySet()) {
      if (entry.getKey().equalsIgnoreCase(this.conditionKey)) {
        headerVal = entry.getValue();
      } else if (entry.getKey().equals("ClientQueryParams")) {
        headerVal = PolicyUtil.fetchQueryParamValue(entry.getValue(),
                                                    this.conditionKey);
        if (headerVal != null) break;
      }
    }
    boolean result = false;

    switch (enumType) {

      case StringEquals:
        result = stringEquals(headerVal);
        break;

      case StringEqualsIgnoreCase:
        result = stringEqualsIgnoreCase(headerVal);
        break;

      case StringLike:
        result = stringLike(headerVal);
        break;

      case StringNotEquals:
        result = !stringEquals(headerVal);
        break;

      case StringNotEqualsIgnoreCase:
        result = !stringEqualsIgnoreCase(headerVal);
        break;

      case StringNotLike:
        result = !stringLike(headerVal);
        break;

      case StringEqualsIfExists:
        if (headerVal != null)
          result = stringEquals(headerVal);
        else
          result = true;
        break;

      case StringEqualsIgnoreCaseIfExists:
        if (headerVal != null)
          result = stringEqualsIgnoreCase(headerVal);
        else
          result = true;
        break;

      case StringLikeIfExists:
        if (headerVal != null)
          result = stringLike(headerVal);
        else
          result = true;
        break;

      case StringNotEqualsIfExists:
        if (headerVal != null)
          result = !stringEquals(headerVal);
        else
          result = true;
        break;

      case StringNotEqualsIgnoreCaseIfExists:
        if (headerVal != null)
          result = !stringEqualsIgnoreCase(headerVal);
        else
          result = true;
        break;

      case StringNotLikeIfExists:
        if (headerVal != null)
          result = !stringLike(headerVal);
        else
          result = true;
        break;

      default:
        result = false;
        break;
    }
    return result;
  }

  /**
   * Check if the input String is equal to any of the condition values.
   * This is a case sensitive comparison. Returns false if headerVal is null.
   * @param headerVal
   * @return true if condition values contains input String
   */
 private
  boolean stringEquals(String headerVal) {
    return this.values.contains(headerVal);
  }

  /**
   * Check if the input String is equal to any of the condition values.
   * This is a case insensitive comparison. Returns false if headerVal is null.
   * @param headerVal
   * @return true if condition values contains input String regardless of case
   */
 private
  boolean stringEqualsIgnoreCase(String headerVal) {
    for (String val : this.values) {
      if (val.equalsIgnoreCase(headerVal)) {
        return true;
      }
    }
    return false;
  }

  /**
   * Case-sensitive matching.
   * The values can include a multi-character match wildcard (*) or
   * a single-character match wildcard (?) anywhere in the string.
   * Returns false if headerVal is null.
   * @param headerVal
   * @return
   */
 private
  boolean stringLike(String headerVal) {
    for (String val : this.values) {
      if (PolicyUtil.isPatternMatching(headerVal, val)) {
        return true;
      }
    }
    return false;
  }

  /**
   * Returns the enum value of the String
   * @param enumName
   * @return
   */
 public
  static StringComparisonType getEnum(String enumName) {
    try {
      return StringComparisonType.valueOf(enumName);
    }
    catch (Exception ex) {
      return null;
    }
  }
}
