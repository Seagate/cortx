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
 * Original author: Abhilekh Mustapure <abhilekh.mustapure@seagate.com>
 * Original creation date: 22-May-2019
 */
package com.seagates3.response.generator;

import java.util.ArrayList;
import java.util.LinkedHashMap;

import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.model.User;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.formatter.xml.XMLResponseFormatter;

public
class UserLoginProfileResponseGenerator extends AbstractResponseGenerator {

 public
  ServerResponse generateCreateResponse(User user) {
    LinkedHashMap responseElements = new LinkedHashMap();
    responseElements.put("UserName", user.getName());
    responseElements.put("PasswordResetRequired",
                         user.getPwdResetRequired().toLowerCase());
    responseElements.put("CreateDate", user.getProfileCreateDate());
    return new XMLResponseFormatter().formatCreateResponse(
        "CreateLoginProfile", "LoginProfile", responseElements,
        AuthServerConfig.getReqId());
  }

  /**
   * Below method will generate 'GetLoginProfile' response
   *
   * @param user
   * @return LoginProfile (username and password) of the requested user
   */
 public
  ServerResponse generateGetResponse(User user) {
    LinkedHashMap<String, String> responseElements = new LinkedHashMap<>();
    ArrayList<LinkedHashMap<String, String>> userMembers = new ArrayList<>();
    responseElements.put("UserName", user.getName());
    responseElements.put("CreateDate", user.getProfileCreateDate());
    responseElements.put("PasswordResetRequired",
                         user.getPwdResetRequired().toLowerCase());
    userMembers.add(responseElements);
    return new XMLResponseFormatter().formatGetResponse(
        "GetLoginProfile", "LoginProfile", userMembers,
        AuthServerConfig.getReqId());
  }

  /**
   * Below method will generate 'UpdateLoginProfile' response
   *
   * @param user
   * @return
   */
 public
  ServerResponse generateUpdateResponse() {
    return new XMLResponseFormatter().formatUpdateResponse(
        "UpdateLoginProfile");
  }

  /**
  * Below method will generate 'ChangePassword' response
  * @return
  */
 public
  ServerResponse generateChangePasswordResponse() {
    return new XMLResponseFormatter().formatChangePasswordResponse(
        "ChangePassword");
  }
}
