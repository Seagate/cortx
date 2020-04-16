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
 * Original creation date: 21-June-2019
 */

package com.seagates3.authorization;

import java.util.HashSet;
import java.util.Set;

public
class RootPermissionAuthorizer {

 private
  static RootPermissionAuthorizer instance = null;
 private
  Set<String> actionsSet = new HashSet<>();

 private
  RootPermissionAuthorizer() {
    actionsSet.add("CreateLoginProfile");
    actionsSet.add("UpdateLoginProfile");
    actionsSet.add("GetLoginProfile");
    actionsSet.add("CreateAccountLoginProfile");
    actionsSet.add("GetAccountLoginProfile");
    actionsSet.add("UpdateAccountLoginProfile");
  }

 public
  static RootPermissionAuthorizer getInstance() {
    if (instance == null) {
      instance = new RootPermissionAuthorizer();
    }
    return instance;
  }

 public
  boolean containsAction(String action) {
    return actionsSet.contains(action) ? true : false;
  }
}
