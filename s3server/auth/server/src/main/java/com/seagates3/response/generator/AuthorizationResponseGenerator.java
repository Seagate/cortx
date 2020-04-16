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
 * Original creation date: 27-May-2016
 */
package com.seagates3.response.generator;

import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.model.Requestor;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.formatter.xml.AuthorizationResponseFormatter;
import java.util.LinkedHashMap;

public class AuthorizationResponseGenerator extends AbstractResponseGenerator {

  public
   ServerResponse generateAuthorizationResponse(Requestor requestor,
                                                String acpXml) {
        LinkedHashMap responseElements = new LinkedHashMap();
        if (requestor != null) {
          responseElements.put("UserId", requestor.getId());
          responseElements.put("UserName", requestor.getName());
          responseElements.put("AccountId", requestor.getAccount().getId());
          responseElements.put("AccountName", requestor.getAccount().getName());
          responseElements.put("CanonicalId",
                               requestor.getAccount().getCanonicalId());
        } else {
          responseElements.put("AllUserRequest", "true");
        }

        return (ServerResponse) new AuthorizationResponseFormatter().authorized(
            responseElements, AuthServerConfig.getReqId(), acpXml);
    }
}
