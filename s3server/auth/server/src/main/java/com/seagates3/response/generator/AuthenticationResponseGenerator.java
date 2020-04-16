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

import com.seagates3.authentication.ClientRequestToken;
import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.model.Requestor;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.formatter.xml.AuthenticationResponseFormatter;
import java.util.LinkedHashMap;
import io.netty.handler.codec.http.HttpResponseStatus;

public class AuthenticationResponseGenerator extends AbstractResponseGenerator {

    public ServerResponse generateAuthenticatedResponse(Requestor requestor,
            ClientRequestToken requestToken) {
        LinkedHashMap responseElements = new LinkedHashMap();
        responseElements.put("UserId", requestor.getId());
        responseElements.put("UserName", requestor.getName());
        responseElements.put("AccountId", requestor.getAccount().getId());
        responseElements.put("AccountName", requestor.getAccount().getName());
        responseElements.put("SignatureSHA256", requestToken.getSignature());
        responseElements.put("CanonicalId",
                             requestor.getAccount().getCanonicalId());
        responseElements.put("Email", requestor.getAccount().getEmail());

        return (ServerResponse) new AuthenticationResponseFormatter()
            .formatAuthenticatedResponse(responseElements,
                                         AuthServerConfig.getReqId());
    }

   public
    ServerResponse requestTimeTooSkewed(String requestTime, String serverTime) {
      String errorMessage =
          "The difference between request time and current time is too large";

      return (ServerResponse) new AuthenticationResponseFormatter()
          .formatSignatureErrorResponse(HttpResponseStatus.FORBIDDEN,
                                        "RequestTimeTooSkewed", errorMessage,
                                        requestTime, serverTime, "900000",
                                        AuthServerConfig.getReqId());
    }
}
