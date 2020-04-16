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
 * Original creation date: 21-May-2016
 */
package com.seagates3.authserver;

import com.google.gson.Gson;
import com.seagates3.exception.AuthResourceNotFoundException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.UnsupportedEncodingException;
import java.util.HashMap;

/**
 * IAM APIs do not follow restful architecture. IAM requests are HTTP POST
 * requests to URL - "https://iam.seagate.com/". These requests contain a field
 * "Action" in the post body which indicates the operation to be performed. Ex -
 * Action: CreateUser. This class is written to map the IAM actions to the
 * corresponding controller and action.
 */
public class IAMResourceMapper {

    private static final String ROUTES_CONFIG_FILE = "/IAMroutes.json";

    private static HashMap<String, String> routeConfigs;

    /**
     * Read the handler mapping rules from routes.json.
     *
     * @throws java.io.UnsupportedEncodingException
     */
    public static void init() throws UnsupportedEncodingException {
        InputStream in = IAMResourceMapper.class.
                getResourceAsStream(ROUTES_CONFIG_FILE);
        InputStreamReader reader = new InputStreamReader(in, "UTF-8");

        Gson gson = new Gson();
        routeConfigs = gson.fromJson(reader, HashMap.class);
    }

    /**
     * Get the controller and action for the request URI.
     *
     * URI should be in the format /controller/action. If a match isn't found
     * for the full URI i.e "/controller/action", then check if the resource has
     * a wild card entry for the controller i.e "/controller/*".
     *
     * @param action
     * @return ResourceMap.
     * @throws com.seagates3.exception.AuthResourceNotFoundException
     */
    public static ResourceMap getResourceMap(String action)
            throws AuthResourceNotFoundException {
        String controllerAction = routeConfigs.get(action);

        if (controllerAction == null) {
            String errorMessage = "Requested operation " + action
                    + " is not supported.";
            throw new AuthResourceNotFoundException(errorMessage);
        }

        String[] tokens = controllerAction.split("#", 2);
        return new ResourceMap(tokens[0], tokens[1]);
    }
}
