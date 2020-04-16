/*
 * COPYRIGHT 2015 SEAGATE LLC
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
 * Original author:  Arjun Hariharan <arjun.hariharan@seagate.com>
 * Original creation date: 12-Dec-2015
 */
package com.seagates3.response.generator;

import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.model.AccessKey;
import com.seagates3.model.Account;
import com.seagates3.model.User;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.formatter.xml.XMLResponseFormatter;
import io.netty.handler.codec.http.HttpResponseStatus;
import java.util.ArrayList;
import java.util.LinkedHashMap;

public class AccountResponseGenerator extends AbstractResponseGenerator {

    public ServerResponse generateCreateResponse(Account account, User root,
            AccessKey rootAccessKey) {
        LinkedHashMap responseElements = new LinkedHashMap();
        responseElements.put("AccountId", account.getId());
        responseElements.put("CanonicalId", account.getCanonicalId());
        responseElements.put("AccountName", account.getName());
        responseElements.put("RootUserName", root.getName());
        responseElements.put("AccessKeyId", rootAccessKey.getId());
        responseElements.put("RootSecretKeyId", rootAccessKey.getSecretKey());
        responseElements.put("Status", rootAccessKey.getStatus());

        return (ServerResponse) new XMLResponseFormatter().formatCreateResponse(
            "CreateAccount", "Account", responseElements,
            AuthServerConfig.getReqId());
    }

    public ServerResponse generateListResponse(Object[] responseObjects) {
        Account[] accounts = (Account[]) responseObjects;

        ArrayList<LinkedHashMap<String, String>> accountMembers = new ArrayList<>();
        LinkedHashMap responseElements;

        for (Account account : accounts) {
            responseElements = new LinkedHashMap();
            responseElements.put("AccountName", account.getName());
            responseElements.put("AccountId", account.getId());
            responseElements.put("CanonicalId", account.getCanonicalId());
            responseElements.put("Email", account.getEmail());
            accountMembers.add(responseElements);
        }

        return new XMLResponseFormatter().formatListResponse(
            "ListAccounts", "Accounts", accountMembers, false,
            AuthServerConfig.getReqId());
    }

    public ServerResponse generateDeleteResponse() {
        return new XMLResponseFormatter().formatDeleteResponse("DeleteAccount");
    }

    public ServerResponse generateResetAccountAccessKeyResponse(Account account, User root,
            AccessKey rootAccessKey) {
        LinkedHashMap responseElements = new LinkedHashMap();
        responseElements.put("AccountId", account.getId());
        responseElements.put("CanonicalId", account.getCanonicalId());
        responseElements.put("AccountName", account.getName());
        responseElements.put("RootUserName", root.getName());
        responseElements.put("AccessKeyId", rootAccessKey.getId());
        responseElements.put("RootSecretKeyId", rootAccessKey.getSecretKey());
        responseElements.put("Status", rootAccessKey.getStatus());

        return (ServerResponse) new XMLResponseFormatter()
            .formatResetAccountAccessKeyResponse("ResetAccountAccessKey",
                                                 "Account", responseElements,
                                                 AuthServerConfig.getReqId());
    }

    @Override
    public ServerResponse entityAlreadyExists() {
        String errorMessage = "The request was rejected because it attempted "
                + "to create an account that already exists.";

        return formatResponse(HttpResponseStatus.CONFLICT,
                "EntityAlreadyExists", errorMessage);
    }

   public
    ServerResponse emailAlreadyExists() {
      String errorMessage = "The request was rejected because " +
                            "account with this email already exists.";

      return formatResponse(HttpResponseStatus.CONFLICT, "EmailAlreadyExists",
                            errorMessage);
    }
}
