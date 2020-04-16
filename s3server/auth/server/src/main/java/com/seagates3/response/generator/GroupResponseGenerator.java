/*
 * COPYRIGHT 2016 SEAGATE LLC
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
 * Original creation date: 20-May-2016
 */
package com.seagates3.response.generator;

import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.model.Group;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.formatter.xml.XMLResponseFormatter;
import java.util.LinkedHashMap;

public class GroupResponseGenerator extends AbstractResponseGenerator {

    public ServerResponse generateCreateResponse(Group group) {
        LinkedHashMap responseElements = new LinkedHashMap();
        responseElements.put("Path", group.getPath());
        responseElements.put("Arn", group.getARN());
        responseElements.put("GroupId", group.getGroupId());
        responseElements.put("GroupName", group.getName());
        responseElements.put("CreateDate", group.getCreateDate());

        return new XMLResponseFormatter().formatCreateResponse(
            "CreateGroup", "Group", responseElements,
            AuthServerConfig.getReqId());
    }

}
