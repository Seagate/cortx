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
 * Original creation date: 18-Jan-2016
 */
package com.seagates3.util;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;

public class JSONUtil {

    /**
     * Serialize the class into a JSON String.
     *
     * @param obj
     * @return
     */
    public static String serializeToJson(Object obj) {
        Gson gson = new GsonBuilder().create();
        return gson.toJson(obj);
    }

    /**
     * Convert json to SAML Metadata Tokens object.
     *
     * @param jsonBody
     * @param deserializeClass
     * @return
     */
    public static Object deserializeFromJson(String jsonBody,
            Class<?> deserializeClass) {
        Gson gson = new Gson();

        return gson.fromJson(jsonBody, deserializeClass);
    }

}
