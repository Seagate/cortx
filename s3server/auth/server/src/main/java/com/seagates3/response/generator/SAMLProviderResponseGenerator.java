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
 * Original creation date: 15-Dec-2015
 */
package com.seagates3.response.generator;

import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.model.SAMLProvider;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.formatter.xml.SAMLProviderResponseFormatter;
import com.seagates3.response.formatter.xml.XMLResponseFormatter;
import java.util.ArrayList;
import java.util.LinkedHashMap;

public class SAMLProviderResponseGenerator extends AbstractResponseGenerator {

    public ServerResponse generateCreateResponse(SAMLProvider samlProvider) {
        LinkedHashMap responseElements = new LinkedHashMap();
        String arnValue = String.format("arn:seagate:iam::%s:%s",
                samlProvider.getAccount().getName(), samlProvider.getName());
        responseElements.put("Arn", arnValue);

        return new SAMLProviderResponseFormatter().formatCreateResponse(
            "CreateSAMLProvider", null, responseElements,
            AuthServerConfig.getReqId());
    }

    public ServerResponse generateDeleteResponse() {
        return new SAMLProviderResponseFormatter().formatDeleteResponse(
                "DeleteSAMLProvider");
    }

    public ServerResponse generateUpdateResponse(String name) {
       return new SAMLProviderResponseFormatter().formatUpdateResponse(
           name, AuthServerConfig.getReqId());
    }

    public ServerResponse generateListResponse(SAMLProvider[] samlProviderList) {
        ArrayList<LinkedHashMap<String, String>> providerMembers = new ArrayList<>();
        LinkedHashMap responseElements;

        for (SAMLProvider provider : samlProviderList) {
            responseElements = new LinkedHashMap();
            String arn = String.format("arn:seagate:iam:::%s",
                    provider.getName());

            responseElements.put("Arn", arn);
            responseElements.put("ValidUntil", provider.getExpiry());
            responseElements.put("CreateDate", provider.getCreateDate());

            providerMembers.add(responseElements);
        }

        return new XMLResponseFormatter().formatListResponse(
            "ListSAMLProviders", "SAMLProviderList", providerMembers, false,
            AuthServerConfig.getReqId());
    }
}
