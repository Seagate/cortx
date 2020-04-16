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
 * Original author: Basavaraj Kirunge basavaraj.kirunge@seagate.com>
 * Original creation date: 20-Jun-2019
 */

package com.seagates3.response.generator;

import java.util.ArrayList;
import java.util.LinkedHashMap;

import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.model.Account;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.formatter.xml.XMLResponseFormatter;
/**
 *
 * TODO : Remove this comment once below code tested end to end
 *
 */
public
class AccountLoginProfileResponseGenerator extends AbstractResponseGenerator {

  /**
     * This method will generate 'GetLoginProfile' response
     *
     * @param Account
     * @return LoginProfile XML of the requested account
  */
 public
  ServerResponse generateGetResponse(Account account) {
    LinkedHashMap<String, String> responseElements = new LinkedHashMap<>();
    ArrayList<LinkedHashMap<String, String>> accountAttr = new ArrayList<>();
    responseElements.put("AccountName", account.getName());
    responseElements.put("CreateDate", account.getProfileCreateDate());
    responseElements.put("PasswordResetRequired",
                         account.getPwdResetRequired().toLowerCase());
    accountAttr.add(responseElements);
    return new XMLResponseFormatter().formatGetResponse(
        "GetAccountLoginProfile", "LoginProfile", accountAttr,
        AuthServerConfig.getReqId());
  }

  /**
     * This method will generate 'CreateAccountLoginProfile' response
     *
     * @param Account
     * @return LoginProfile XML of the requested account
  */

 public
  ServerResponse generateCreateResponse(Account account) {
    LinkedHashMap<String, String> responseElements =
        new LinkedHashMap<String, String>();
    responseElements.put("AccountName", account.getName());
    responseElements.put("PasswordResetRequired",
                         account.getPwdResetRequired().toLowerCase());
    responseElements.put("CreateDate", account.getProfileCreateDate());
    return new XMLResponseFormatter().formatCreateResponse(
        "CreateAccountLoginProfile", "LoginProfile", responseElements,
        AuthServerConfig.getReqId());
  }

  /**
   * This method will generate 'CreateAccountLoginProfile' response
   *
   * @param Account
   * @return LoginProfile XML of the requested account
 */

 public
  ServerResponse generateUpdateResponse() {
    return new XMLResponseFormatter().formatUpdateResponse(
        "UpdateAccountLoginProfile");
  }
}
