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
package com.seagates3.dao.ldap;

import com.novell.ldap.LDAPConnection;
import com.novell.ldap.LDAPEntry;
import com.novell.ldap.LDAPException;
import com.novell.ldap.LDAPSearchResults;
import com.seagates3.dao.AccountDAO;
import com.seagates3.dao.DAODispatcher;
import com.seagates3.dao.DAOResource;
import com.seagates3.dao.RequestorDAO;
import com.seagates3.exception.DataAccessException;
import com.seagates3.model.AccessKey;
import com.seagates3.model.Account;
import com.seagates3.model.Requestor;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class RequestorImpl implements RequestorDAO {

    private final Logger LOGGER =
            LoggerFactory.getLogger(RequestorImpl.class.getName());

    @Override
    public Requestor find(AccessKey accessKey) throws DataAccessException {
        Requestor requestor = new Requestor();

        requestor.setAccessKey(accessKey);

        if (accessKey.getUserId() != null) {
            String filter;
            String[] attrs = {LDAPUtils.COMMON_NAME};
            LDAPSearchResults ldapResults;

            filter = String.format("%s=%s", LDAPUtils.USER_ID,
                    accessKey.getUserId());

            String baseDN = String.format("%s=%s,%s",
                    LDAPUtils.ORGANIZATIONAL_UNIT_NAME, LDAPUtils.ACCOUNT_OU,
                    LDAPUtils.BASE_DN);

            LOGGER.debug("Finding access key details of userID: "
                                            + accessKey.getUserId());
            try {
                ldapResults = LDAPUtils.search(baseDN, LDAPConnection.SCOPE_SUB,
                        filter, attrs);
            } catch (LDAPException ex) {
                LOGGER.error("Failed to find access key details of userId: "
                                           + accessKey.getUserId());
                throw new DataAccessException(
                        "Failed to find requestor details.\n" + ex);
            }

            if (ldapResults.hasMore()) {
                LDAPEntry entry;
                try {
                    entry = ldapResults.next();
                } catch (LDAPException ex) {
                    throw new DataAccessException("LDAP error\n" + ex);
                }

                requestor.setId(accessKey.getUserId());
                requestor.setName(entry.getAttribute(
                        LDAPUtils.COMMON_NAME).getStringValue());

                String accountName = getAccountName(entry.getDN());
                requestor.setAccount(getAccount(accountName));
            } else {
                LOGGER.error("Failed to find access key details of userId: "
                        + accessKey.getUserId());
                throw new DataAccessException(
                        "Failed to find the requestor who owns the "
                        + "given access key.\n");
            }
        }

        return requestor;
    }

    /**
     * Extract the account name from user distinguished name.
     *
     * @param dn
     * @return Account Name
     */
    private String getAccountName(String dn) {
        String dnRegexPattern = "[\\w\\W]+,o=(.*?),[\\w\\W]+";

        Pattern pattern = Pattern.compile(dnRegexPattern);
        Matcher matcher = pattern.matcher(dn);
        if (matcher.find()) {
            return matcher.group(1);
        }

        return "";
    }

    /**
     * Get the account details from account name.
     *
     * @param accountName Account name.
     * @return Account
     */
    private Account getAccount(String accountName) throws DataAccessException {
        AccountDAO accountDao = (AccountDAO) DAODispatcher.getResourceDAO(
                DAOResource.ACCOUNT);
        LOGGER.debug("Finding account: " + accountName);
        return accountDao.find(accountName);
    }
}
