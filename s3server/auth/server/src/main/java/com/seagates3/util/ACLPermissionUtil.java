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
 * Original author:  Basavaraj Kirunge
 * Original creation date: 10-Apr-2019
 */

package com.seagates3.util;

import java.util.HashMap;

import io.netty.handler.codec.http.HttpMethod;

/*
 * Utility class to get ACL permission type given method and request uri
 */

public class ACLPermissionUtil {

    public static final String ACL_READ = "READ";
    public static final String ACL_READ_ACP = "READ_ACP";
    public static final String ACL_WRITE = "WRITE";
    public static final String ACL_WRITE_ACP = "WRITE_ACP";
    public static final String ACL_FULL_CONTROL = "FULL_CONTROL";

    static final String ACL_QUERY = "?acl";

    static final HashMap<HttpMethod, String> permissionMap = initPermissionMap();

    private static HashMap<HttpMethod, String> initPermissionMap() {

        HashMap<HttpMethod, String> pMap = new HashMap<>();
        pMap.put(HttpMethod.GET, ACL_READ);
        pMap.put(HttpMethod.HEAD, ACL_READ);
        pMap.put(HttpMethod.PUT, ACL_WRITE);
        pMap.put(HttpMethod.DELETE, ACL_WRITE);
        pMap.put(HttpMethod.POST, ACL_WRITE);
        return pMap;
    }

    final static HashMap<HttpMethod, String> permissionMapACP = initPermissionMapACP();

    private static HashMap<HttpMethod, String> initPermissionMapACP() {

        HashMap<HttpMethod, String> pMap = new HashMap<>();
        pMap.put(HttpMethod.GET, ACL_READ_ACP);
        pMap.put(HttpMethod.HEAD, ACL_READ_ACP);
        pMap.put(HttpMethod.PUT, ACL_WRITE_ACP);
        pMap.put(HttpMethod.DELETE, ACL_WRITE_ACP);
        pMap.put(HttpMethod.POST, ACL_WRITE_ACP);
        return pMap;
    }

    /*
     * Returns ACL Permission
     * @param method Request method
     * @param uri Request Uri
     * @return ACL permission and null in case of error
     */
   public
    static String getACLPermission(HttpMethod method, String uri,
                                   String queryParam) {

        if (uri == null || uri.isEmpty()) {
            IEMUtil.log(IEMUtil.Level.ERROR,
                    IEMUtil.INVALID_REST_URI_ERROR,
                            "URI is empty or null", null);
            return null;
        }
        if (isACLReadWrite(uri, queryParam)) {
           return permissionMapACP.get(method);
        }
        else {
            return permissionMap.get(method);
        }
    }

   public
    static boolean isACLReadWrite(String uri, String queryParam) {
      if (uri.endsWith(ACL_QUERY) || "acl".equals(queryParam)) {
            return true;
        }
        else {
            return false;
        }
    }
}


