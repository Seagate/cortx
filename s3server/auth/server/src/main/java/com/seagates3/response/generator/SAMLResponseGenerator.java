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

import com.seagates3.response.ServerResponse;
import com.seagates3.response.formatter.xml.AssumeRoleWithSAMLResponseFormatter;
import io.netty.handler.codec.http.HttpResponseStatus;

public class SAMLResponseGenerator
        extends AbstractResponseGenerator {

    public ServerResponse invalidIdentityToken() {
        String errorMessage = "The web identity token that was passed could "
                + "not be validated. Get a new identity token from the "
                + "identity provider and then retry the request.";

        return new AssumeRoleWithSAMLResponseFormatter().formatErrorResponse(
                HttpResponseStatus.BAD_REQUEST, "InvalidIdentityToken",
                errorMessage);
    }

    public ServerResponse expiredToken() {
        String errorMessage = "The web identity token that was passed is "
                + "expired or is not valid. Get a new identity token from the "
                + "identity provider and then retry the request.";

        return new AssumeRoleWithSAMLResponseFormatter().formatErrorResponse(
                HttpResponseStatus.BAD_REQUEST, "ExpiredToken",
                errorMessage);
    }

    public ServerResponse idpRejectedClaim() {
        String errorMessage = "The identity provider (IdP) reported that "
                + "authentication failed. This might be because the claim is "
                + "invalid.\nIf this error is returned for the "
                + "AssumeRoleWithWebIdentity operation, it can also mean that "
                + "the claim has expired or has been explicitly revoked.";

        return new AssumeRoleWithSAMLResponseFormatter().formatErrorResponse(
                HttpResponseStatus.FORBIDDEN, "IDPRejectedClaim",
                errorMessage);
    }
}
