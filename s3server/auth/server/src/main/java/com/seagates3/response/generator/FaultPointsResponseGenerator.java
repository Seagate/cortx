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
 * Original author:  Sushant Mane <sushant.mane@seagate.com>
 * Original creation date: 26-Oct-2016
 */
package com.seagates3.response.generator;

import com.seagates3.response.ServerResponse;
import io.netty.handler.codec.http.HttpResponseStatus;

public class FaultPointsResponseGenerator extends AbstractResponseGenerator {

    public ServerResponse badRequest(String errorMessage) {
        return formatResponse(HttpResponseStatus.BAD_REQUEST,
                "BadRequest", errorMessage);
    }

    public ServerResponse invalidParametervalue(String errorMessage) {
        return formatResponse(HttpResponseStatus.BAD_REQUEST,
                "InvalidParameterValue", errorMessage);
    }

    public ServerResponse faultPointAlreadySet(String errorMessage) {

        return formatResponse(HttpResponseStatus.CONFLICT,
                "FaultPointAlreadySet", errorMessage);
    }

    public ServerResponse faultPointNotSet(String errorMessage) {

        return formatResponse(HttpResponseStatus.CONFLICT,
                "FaultPointNotSet", errorMessage);
    }

    public ServerResponse setSuccessful() {
        return new ServerResponse(HttpResponseStatus.CREATED,
                "Fault point set successfully.");
    }

    public ServerResponse resetSuccessful() {
        return new ServerResponse(HttpResponseStatus.OK,
                "Fault point deleted successfully.");
    }
}
