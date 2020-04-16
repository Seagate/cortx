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

public
class PolicyCondition {

 protected
  String type;
 protected
  String conditionKey;
 protected
  List<String> values;

  /**
   * Returns the type of this condition.
   *
   * @return the type of this condition.
   */
 public
  String getType() { return type; }

  /**
   * Sets the type of this condition.
   *
   * @param type
   *            the type of this condition.
   */
 public
  void setType(String type) { this.type = type; }

  /**
   * @return The name of the condition key involved in this condition.
   */
 public
  String getConditionKey() { return conditionKey; }

  /**
   * @param conditionKey
   *            The name of the condition key involved in this condition.
   */
 public
  void setConditionKey(String conditionKey) {
    this.conditionKey = conditionKey;
  }

  /**
   * @return The values specified for this access control policy condition.
   */
 public
  List<String> getValues() { return values; }

  /**
   * @param values
   *            The values specified for this access control policy condition.
   */
 public
  void setValues(List<String> values) { this.values = values; }

  /**
   * Checks if the condition is satisfied from the request
   * @param requestBody
   * @return
   */
 public
  boolean isSatisfied(Map<String, String> requestBody) {
    // TODO: Add logic to check the headers as per generic Condition type
    // OR keep abstract
    return false;
  }
}
