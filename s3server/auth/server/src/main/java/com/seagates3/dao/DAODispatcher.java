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
 * Original creation date: 17-Sep-2014
 */
package com.seagates3.dao;

import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.authserver.SSLContextProvider;
import com.seagates3.dao.ldap.LdapConnectionManager;
import com.seagates3.exception.ServerInitialisationException;
import org.apache.logging.log4j.LogManager;
import org.apache.logging.log4j.Logger;

public class DAODispatcher {

    private static final String DAO_PACKAGE = "com.seagates3.dao";
    private static DAOProvider provider;
    private static final Logger LOGGER
            = LogManager.getLogger(SSLContextProvider.class.getName());

    public static void init() throws ServerInitialisationException {
        String dataSource = AuthServerConfig.getDataSource();
        DAODispatcher.provider = DAOProvider.valueOf(dataSource.toUpperCase());

        if ("LDAP".equals(dataSource.toUpperCase())) {
            LdapConnectionManager.initLdap();
            LOGGER.info("Initialized LDAP");
        }
    }

    public static Object getResourceDAO(DAOResource daoResource) {
        Class<?> validator;
        Object obj;
        String resourceDAOName = getResourceDAOName(daoResource.toString());

        try {
            validator = Class.forName(resourceDAOName);
            obj = validator.newInstance();
        } catch (ClassNotFoundException | SecurityException ex) {
            obj = null;
        } catch (IllegalAccessException | IllegalArgumentException | InstantiationException ex) {
            obj = null;
        }

        return obj;
    }

    private static String getResourceDAOName(String resourceName) {
        String resourceDAOName = resourceName + "Impl";

        return String.format("%s.%s.%s", DAO_PACKAGE,
                provider.toString().toLowerCase(), resourceDAOName);
    }
}
