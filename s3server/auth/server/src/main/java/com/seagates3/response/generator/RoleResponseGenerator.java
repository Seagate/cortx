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
 * Original creation date: 13-Dec-2015
 */
package com.seagates3.response.generator;

import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.model.Role;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.formatter.xml.XMLResponseFormatter;
import java.util.ArrayList;
import java.util.LinkedHashMap;

public class RoleResponseGenerator extends AbstractResponseGenerator {

    public ServerResponse generateCreateResponse(Role role) {
        LinkedHashMap responseElements = new LinkedHashMap();
        responseElements.put("Path", role.getPath());
        responseElements.put("Arn", role.getARN());
        responseElements.put("RoleName", role.getName());
        responseElements.put("AssumeRolePolicyDocument", role.getRolePolicyDoc());
        responseElements.put("CreateDate", role.getCreateDate());
        responseElements.put("RoleId", role.getRoleId());

        return new XMLResponseFormatter().formatCreateResponse(
            "CreateRole", "Role", responseElements,
            AuthServerConfig.getReqId());
    }

    public ServerResponse generateDeleteResponse() {
        return new XMLResponseFormatter().formatDeleteResponse("DeleteRole");
    }

    public ServerResponse generateListResponse(Role[] roleList) {
        ArrayList<LinkedHashMap<String, String>> roleMemebers = new ArrayList<>();
        LinkedHashMap responseElements;

        for (Role role : roleList) {
            responseElements = new LinkedHashMap();
            responseElements.put("Path", role.getPath());

            String arn = String.format("arn:seagate:iam::%s:%s",
                    role.getAccount().getName(), role.getName());
            responseElements.put("Arn", arn);

            responseElements.put("RoleName", role.getName());
            responseElements.put("AssumeRolePolicyDocument", role.getRolePolicyDoc());
            responseElements.put("CreateDate", role.getCreateDate());
            responseElements.put("RoleId", role.getName());

            roleMemebers.add(responseElements);
        }

        return (ServerResponse) new XMLResponseFormatter().formatListResponse(
            "ListRoles", "Roles", roleMemebers, false,
            AuthServerConfig.getReqId());
    }
}
