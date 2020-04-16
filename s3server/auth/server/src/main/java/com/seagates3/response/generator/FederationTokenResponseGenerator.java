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
 * Original creation date: 16-Dec-2015
 */
package com.seagates3.response.generator;

import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.model.AccessKey;
import com.seagates3.model.User;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.formatter.xml.FederationTokenResponseFormatter;
import java.util.LinkedHashMap;

public class FederationTokenResponseGenerator extends AbstractResponseGenerator {

    public ServerResponse generateCreateResponse(User user, AccessKey accessKey) {
        LinkedHashMap<String, String> credentials = new LinkedHashMap<>();
        credentials.put("SessionToken", accessKey.getToken());
        credentials.put("SecretAccessKey", accessKey.getSecretKey());
        credentials.put("Expiration", accessKey.getExpiry());
        credentials.put("AccessKeyId", accessKey.getId());

        LinkedHashMap<String, String> federatedUser = new LinkedHashMap<>();

        String arnValue = String.format("arn:aws:sts::1:federated-user/%s", user.getName());
        federatedUser.put("Arn", arnValue);

        String fedUseIdVal = String.format("%s:%s", user.getId(), user.getName());
        federatedUser.put("FederatedUserId", fedUseIdVal);

        return new FederationTokenResponseFormatter().formatCreateResponse(
            credentials, federatedUser, "6", AuthServerConfig.getReqId());
    }
}
