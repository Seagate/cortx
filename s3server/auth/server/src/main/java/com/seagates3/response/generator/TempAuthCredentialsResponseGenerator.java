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
 * Original creation date: 27-June-2019
 */

package com.seagates3.response.generator;

import java.util.LinkedHashMap;

import com.seagates3.model.AccessKey;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.formatter.xml.AccessKeyResponseFormatter;

public
class TempAuthCredentialsResponseGenerator extends AbstractResponseGenerator {

 public
  ServerResponse generateCreateResponse(String userName, AccessKey accessKey) {
    LinkedHashMap<String, String> responseElements = new LinkedHashMap<>();
    responseElements.put("UserName", userName);
    responseElements.put("AccessKeyId", accessKey.getId());
    responseElements.put("Status", accessKey.getStatus());
    responseElements.put("SecretAccessKey", accessKey.getSecretKey());
    responseElements.put("ExpiryTime", accessKey.getExpiry());
    responseElements.put("SessionToken", accessKey.getToken());

    return new AccessKeyResponseFormatter().formatCreateResponse(
        "GetTempAuthCredentials", "AccessKey", responseElements, "0000");
  }
}
