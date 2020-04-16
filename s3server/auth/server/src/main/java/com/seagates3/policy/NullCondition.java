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

import java.util.List;
import java.util.Map;
import java.util.Map.Entry;

public
class NullCondition extends PolicyCondition {

 public
  NullCondition(String key, List<String> values) {
    super.type = "Null";
    super.conditionKey = key;
    super.values = values;
  }

  /**
   * Checks if this Null Condition is satisfied from the request.
   * Sample condition
   * "Condition":{"Null":{"aws:TokenIssueTime":"true"}}
   */
  @Override public boolean isSatisfied(Map<String, String> requestBody) {
    if (values == null) return false;
    for (String v : values) {
      if ("true".equalsIgnoreCase(v) || "false".equalsIgnoreCase(v)) {
        boolean value = Boolean.parseBoolean(v);

        if (value) {
          // If true, the conditionKey should not be present in the
          // requestBody
          boolean found = false;
          for (Entry<String, String> entry : requestBody.entrySet()) {
            if (entry.getKey().equalsIgnoreCase(this.conditionKey)) {
              found = true;
              break;
            } else if (entry.getKey().equals("ClientQueryParams")) {
              if (PolicyUtil.fetchQueryParamValue(entry.getValue(),
                                                  this.conditionKey) != null) {
                found = true;
                break;
              }
            }
          }
          if (!found) return true;
        } else {
          // if false, the key should exist in the requestBody and its
          // value should be not null
          for (Entry<String, String> entry : requestBody.entrySet()) {
            if (entry.getKey().equalsIgnoreCase(this.conditionKey) &&
                entry.getValue() != null) {
              return true;
            } else if (entry.getKey().equals("ClientQueryParams")) {
              if (PolicyUtil.fetchQueryParamValue(entry.getValue(),
                                                  this.conditionKey) != null) {
                return true;
              }
            }
          }
        }
      }
    }
    return false;
  }
}
