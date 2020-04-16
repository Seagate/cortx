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
 * Original author:  Shalaka Dharap
 * Original creation date: 20-November-2019
*/

package com.seagates3.policy;

import java.io.InputStream;
import java.io.InputStreamReader;
import java.lang.reflect.Type;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;

import com.google.gson.Gson;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;
import com.google.gson.reflect.TypeToken;

public
class S3Actions {

 private
  static final String S3_ACTIONS_FILE = "/S3Actions.json";

 private
  Map<String, Set<String>> bucketOperations = new HashMap<>();
 private
  Map<String, Set<String>> objectOperations = new HashMap<>();

 private
  S3Actions() { init(); }

 public
  void init() {
    InputStream in = null;
    in = S3Actions.class.getResourceAsStream(S3_ACTIONS_FILE);
    InputStreamReader reader = new InputStreamReader(in);
    JsonParser jsonParser = new JsonParser();
    JsonObject element = (JsonObject)jsonParser.parse(reader);
    Type setType = new TypeToken<Set<String>>() {}
    .getType();
    Gson gson = new Gson();
    Set<String> keys = new HashSet<>();

    for (Entry<String, JsonElement>
             entry : element.get("Bucket").getAsJsonObject().entrySet()) {
      JsonElement setElem = entry.getValue();
      keys = gson.fromJson(setElem, setType);
      bucketOperations.put(entry.getKey().toLowerCase(), keys);
    }

    for (Entry<String, JsonElement>
             entry : element.get("Object").getAsJsonObject().entrySet()) {
      JsonElement setElem = entry.getValue();
      keys = gson.fromJson(setElem, setType);
      objectOperations.put(entry.getKey().toLowerCase(), keys);
    }
  }

 private
  static class S3ActionsHolder {
    static final S3Actions S3ACTIONS_INSTANCE = new S3Actions();
  } public static S3Actions getInstance() {
    return S3ActionsHolder.S3ACTIONS_INSTANCE;
  }

 public
  Map<String, Set<String>> getBucketOperations() { return bucketOperations; }

 public
  Map<String, Set<String>> getObjectOperations() { return objectOperations; }
}
