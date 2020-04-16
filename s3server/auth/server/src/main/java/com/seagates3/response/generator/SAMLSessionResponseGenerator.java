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
 * Original creation date: 26-Jan-2016
 */
package com.seagates3.response.generator;

import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.model.Requestor;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.formatter.xml.SAMLSessionResponseFormatter;
import java.util.LinkedHashMap;

public class SAMLSessionResponseGenerator extends AbstractResponseGenerator {

    public ServerResponse generateCreateResponse(Requestor requestor) {
        LinkedHashMap credElements = new LinkedHashMap();
        credElements.put("AccessKeyId", requestor.getAccesskey().getId());
        credElements.put("SessionToken", requestor.getAccesskey().getToken());
        credElements.put("SecretAccessKey", requestor.getAccesskey()
                .getSecretKey());

        LinkedHashMap userDetailsElements = new LinkedHashMap();
        userDetailsElements.put("UserId", requestor.getId());
        userDetailsElements.put("UserName", requestor.getName());
        userDetailsElements.put("AccountId", requestor.getAccount().getId());
        userDetailsElements.put("AccountName", requestor.getAccount().getName());

        return new SAMLSessionResponseFormatter().formatCreateReponse(
            credElements, userDetailsElements, AuthServerConfig.getReqId());
    }

}
