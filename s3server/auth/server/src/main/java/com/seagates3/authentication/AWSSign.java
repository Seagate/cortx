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
 * Original creation date: 22-Oct-2015
 */
package com.seagates3.authentication;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

import com.seagates3.exception.InvalidTokenException;
import com.seagates3.model.Requestor;

public interface AWSSign {
    /**
     * Map the AWS signing algorithm against the corresponding hashing function.
     */
    public static final Map<String, String> AWSHashFunction =
            Collections.unmodifiableMap(
                    new HashMap<String, String>() {
                        {
                            put("AWS4-HMAC-SHA256", "hashSHA256");
                        }
                    });

    /*
     * Authenticate the request using AWS algorithm.
     */
    public Boolean authenticate(ClientRequestToken clientRequestToken,
                       Requestor requestor) throws InvalidTokenException;
}
