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

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;

public
class BooleanCondition extends PolicyCondition {

 public
  static enum BooleanComparisonType {
    Bool,
    BoolIfExists;
  };

 public BooleanCondition(BooleanComparisonType type, String key,
                         List<String> values) {
    if (type == null)
      super.type = null;
    else
      super.type = type.toString();
    super.conditionKey = key;
    super.values = values;
  }

  /**
   * Checks if this Boolean Condition is satisfied from the request.
   * Sample condition -
   * "Condition": {"Bool": {"aws:SecureTransport": "true"}}
   */
  @Override public boolean isSatisfied(Map<String, String> requestBody) {
    if (this.values == null) return false;
    BooleanComparisonType enumType = BooleanComparisonType.valueOf(this.type);
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
      case Bool:
        result = booleanEquals(headerVal);
        break;

      case BoolIfExists:
        if (headerVal != null)
          result = booleanEquals(headerVal);
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
   * Returns false if the headerVal or any of the condition values is not a
   * {@link Number}
   * @param headerVal
   * @return
   */
 private
  boolean booleanEquals(String headerVal) {
    if (headerVal == null) return false;
    Boolean boolHeader = null;
    List<Boolean> boolValues = null;
    boolHeader = Boolean.parseBoolean(headerVal.trim());
    boolValues = new ArrayList<Boolean>();
    for (String s : this.values) {
      if (!("true".equalsIgnoreCase(s) || "false".equalsIgnoreCase(s)))
        continue;
      boolValues.add(Boolean.parseBoolean(s));
    }
    return boolValues.contains(boolHeader);
  }

  /**
   * Returns the enum value of the String
   * @param enumName
   * @return
   */
 public
  static BooleanComparisonType getEnum(String enumName) {
    try {
      return BooleanComparisonType.valueOf(enumName);
    }
    catch (Exception ex) {
      return null;
    }
  }
}
