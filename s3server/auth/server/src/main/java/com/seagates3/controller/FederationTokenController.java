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
 * Original creation date: 17-Sep-2015
 */
package com.seagates3.controller;

import com.seagates3.exception.DataAccessException;
import com.seagates3.model.AccessKey;
import com.seagates3.model.Requestor;
import com.seagates3.model.User;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.generator.FederationTokenResponseGenerator;
import com.seagates3.service.AccessKeyService;
import com.seagates3.service.UserService;
import java.util.Map;

public class FederationTokenController extends AbstractController {

    FederationTokenResponseGenerator fedTokenResponse;

    public FederationTokenController(Requestor requestor,
            Map<String, String> requestBody) {
        super(requestor, requestBody);

        fedTokenResponse = new FederationTokenResponseGenerator();
    }

    /*
     * TODO
     * Store user policy
     */
    @Override
    public ServerResponse create() throws DataAccessException {
        String userName = requestBody.get("Name");

        int duration;
        if (requestBody.containsKey("DurationSeconds")) {
            duration = Integer.parseInt(requestBody.get("DurationSeconds"));
        } else {
            duration = 43200;
        }

        User user = UserService.createFederationUser(requestor.getAccount(),
                userName);

        AccessKey accessKey = AccessKeyService.createFedAccessKey(user,
                duration);
        if (accessKey == null) {
            return fedTokenResponse.internalServerError();
        }

        return fedTokenResponse.generateCreateResponse(user, accessKey);
    }
}
