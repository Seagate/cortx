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
 * Original creation date: 19-May-2016
 */
package com.seagates3.response.generator;

import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.model.Policy;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.formatter.xml.XMLResponseFormatter;

import io.netty.handler.codec.http.HttpResponseStatus;

import java.util.LinkedHashMap;

public class PolicyResponseGenerator extends AbstractResponseGenerator {

    public ServerResponse generateCreateResponse(Policy policy) {
       LinkedHashMap<String, String> responseElements =
           new LinkedHashMap<String, String>();
        responseElements.put("PolicyName", policy.getName());
        responseElements.put("DefaultVersionId", policy.getDefaultVersionid());
        responseElements.put("PolicyId", policy.getPolicyId());
        responseElements.put("Path", policy.getPath());
        responseElements.put("Arn", policy.getARN());
        responseElements.put("AttachmentCount",
                String.valueOf(policy.getAttachmentCount()));
        responseElements.put("CreateDate", policy.getCreateDate());
        responseElements.put("UpdateDate", policy.getUpdateDate());

        return new XMLResponseFormatter().formatCreateResponse(
            "CreatePolicy", "Policy", responseElements,
            AuthServerConfig.getReqId());
    }

   public
    ServerResponse malformedPolicy(String errorMessage) {
      return formatResponse(HttpResponseStatus.BAD_REQUEST, "MalformedPolicy",
                            errorMessage);
    }

   public
    ServerResponse noSuchPolicy() {
      String errorMessage = "The specified policy does not exist.";
      return formatResponse(HttpResponseStatus.NOT_FOUND, "NoSuchPolicy",
                            errorMessage);
    }

   public
    ServerResponse invalidPolicyDocument() {
      String errorMessage =
          "The content of the form does not meet the conditions specified in " +
          "the " + "policy document.";
      return formatResponse(HttpResponseStatus.BAD_REQUEST,
                            "InvalidPolicyDocument", errorMessage);
    }
}
