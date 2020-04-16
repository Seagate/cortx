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
 * Original creation date: 29-November-2019
 */

package com.seagates3.policy;

import java.io.InputStreamReader;
import java.lang.reflect.Type;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map.Entry;
import java.util.Set;

import com.google.gson.Gson;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;
import com.google.gson.reflect.TypeToken;

public
class ConditionUtil {

 private
  HashSet<String> conditionTypes = new HashSet<>();

 private
  HashSet<String> conditionKeys = new HashSet<>();

 private
  HashMap<String, Set<String>> s3ActionsMap = new HashMap<>();

 private
  static final String CONDITION_FIELDS_FILE =
      "/policy/PolicyConditionFields.json";

  /**
   * private constructor - initializes the ConditionTypes and ConditionKeys
   */
 private
  ConditionUtil() { initConditionFields(); }

  /**
   * Singleton class holder
   */
 private
  static class ConditionUtilHolder {
    static final ConditionUtil CONDITION_INSTANCE = new ConditionUtil();
  }

  /**
   * Gets the singleton instance of {@link ConditionUtil}
   * @return {@link ConditionUtil} instance
   */
  public static ConditionUtil
  getInstance() {
    return ConditionUtilHolder.CONDITION_INSTANCE;
  }

  /**
   * Initialize conditionTypes and conditionKeys set from
   * PolicyConditionFields.json file
   */
 private
  void initConditionFields() {
    InputStreamReader reader = new InputStreamReader(
        PolicyValidator.class.getResourceAsStream(CONDITION_FIELDS_FILE));

    Gson gson = new Gson();
    Type setType = new TypeToken<HashSet<String>>() {}
    .getType();
    JsonParser jsonParser = new JsonParser();
    JsonObject element = (JsonObject)jsonParser.parse(reader);
    conditionTypes = gson.fromJson(element.get("ConditionTypes"), setType);
    JsonObject keyElement = (JsonObject)element.get("ConditionKeys");
    HashSet<String> globalKeys =
        gson.fromJson(keyElement.get("GlobalKeys"), setType);
    HashSet<String> s3Keys = gson.fromJson(keyElement.get("S3Keys"), setType);
    conditionKeys.addAll(globalKeys);
    conditionKeys.addAll(s3Keys);

    // Convert conditionKeys elements to lower case
    String[] keysArray = conditionKeys.toArray(new String[0]);
    for (int i = 0; i < keysArray.length; ++i) {
      keysArray[i] = keysArray[i].toLowerCase();
    }
    conditionKeys.clear();
    conditionKeys.addAll(Arrays.asList(keysArray));

    s3ActionsMap.putAll(S3Actions.getInstance().getBucketOperations());
    s3ActionsMap.putAll(S3Actions.getInstance().getObjectOperations());
  }

  /**
   * Check if the condition type is valid
   * Condition types can be subtypes of - String, Numeric, Date, Boolean,
   * Arn, Binary, IpAddress, ..IfExists, Null
   * @param condition
   * @return
   */
 public
  boolean isConditionTypeValid(String conditionType) {
    if (conditionTypes.contains(conditionType)) {
      return true;
    }
    return false;
  }

  /**
   * Check if the condition key is valid
   * The condition keys could be one of - AWS global keys / custom keys
   * or S3 specific keys
   * e.g. - AWS wide keys - aws:CurrentTime, aws:SourceIp, aws:SourceArn, etc.
   * S3 specific keys - s3:x-amz-acl, s3:prefix, etc.
   * Custom Keys can also be used which should be in format - aws:<key>
   * @param condition
   * @return - true if the Condition key is one of the above specified keys
   */
 public
  boolean isConditionKeyValid(String conditionKey) {

    if (conditionKey == null) return false;
    conditionKey = conditionKey.toLowerCase();
    if (conditionKeys.contains(conditionKey) ||
        conditionKey.startsWith("aws:")) {
      return true;
    }
    return false;
  }

  /**
   * Validate if the combination of Action to the Condition Key is proper.
   * Method validates the condition against the Action with the help of
   * S3Actions.json resource
   * @param conditionKey
   * @param action
   * @return
   */
 public
  boolean isConditionCombinationValid(String conditionKey, String action) {

    if (conditionKey == null || conditionKey.isEmpty() || action == null ||
        action.isEmpty())
      return false;

    // Combination check does not apply for global condition keys.
    if (conditionKey.startsWith("aws:")) return true;

    for (Entry<String, Set<String>> entry : s3ActionsMap.entrySet()) {
      if (action.equalsIgnoreCase(entry.getKey())) {
        for (String val : entry.getValue()) {
          if (val.equalsIgnoreCase(conditionKey)) return true;
        }
      }
    }
    return false;
  }

  /**
   * Removes the prefix ('s3:' or 'aws:') from Condition key
   * @param key
   * @return String after removing prefix, if present.
   */
 public
  static String removeKeyPrefix(String key) {
    if (key != null) {
      if (key.startsWith("s3:"))
        return key.substring(3);
      else if (key.startsWith("aws:"))
        return key.substring(4);
    }
    return key;
  }
}
