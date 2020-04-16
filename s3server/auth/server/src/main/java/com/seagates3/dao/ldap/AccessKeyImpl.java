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

import com.novell.ldap.LDAPAttribute;
import com.novell.ldap.LDAPAttributeSet;
import com.novell.ldap.LDAPConnection;
import com.novell.ldap.LDAPEntry;
import com.novell.ldap.LDAPException;
import com.novell.ldap.LDAPModification;
import com.novell.ldap.LDAPSearchResults;
import com.seagates3.controller.UserController;
import com.seagates3.dao.AccessKeyDAO;
import com.seagates3.exception.DataAccessException;
import com.seagates3.model.AccessKey;
import com.seagates3.model.AccessKey.AccessKeyStatus;
import com.seagates3.model.User;
import com.seagates3.util.DateUtil;
import java.util.ArrayList;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class AccessKeyImpl implements AccessKeyDAO {

    private final Logger LOGGER =
            LoggerFactory.getLogger(AccessKeyImpl.class.getName());
    /**
     * Search the access key in LDAP.
     *
     * @param accessKeyId
     * @return
     * @throws com.seagates3.exception.DataAccessException
     */
    @Override
    public AccessKey find(String accessKeyId) throws DataAccessException {
        AccessKey accessKey = new AccessKey();
        accessKey.setId(accessKeyId);

        String[] attrs = {LDAPUtils.USER_ID, LDAPUtils.SECRET_KEY,
            LDAPUtils.EXPIRY, LDAPUtils.TOKEN, LDAPUtils.STATUS,
            LDAPUtils.CREATE_TIMESTAMP, LDAPUtils.OBJECT_CLASS};

        String accessKeyBaseDN = String.format("%s=accesskeys,%s",
                LDAPUtils.ORGANIZATIONAL_UNIT_NAME, LDAPUtils.BASE_DN
        );

        String filter = String.format("%s=%s", LDAPUtils.ACCESS_KEY_ID,
                accessKeyId);

        LDAPSearchResults ldapResults;

        try {
            ldapResults = LDAPUtils.search(accessKeyBaseDN,
                    LDAPConnection.SCOPE_SUB, filter, attrs);
        } catch (LDAPException ex) {
            LOGGER.error("Failed to find Access Key.");
            throw new DataAccessException("Access key find failed.\n" + ex);
        }

        if (ldapResults.hasMore()) {
            LDAPEntry entry;
            try {
                entry = ldapResults.next();
            } catch (LDAPException ex) {
                LOGGER.error("Failed to update Access Key.");
                throw new DataAccessException("Failed to update AccessKey.\n"
                        + ex);
            }

            accessKey.setUserId(entry.getAttribute(LDAPUtils.USER_ID)
                    .getStringValue());
            accessKey.setSecretKey(entry.getAttribute(LDAPUtils.SECRET_KEY)
                    .getStringValue());
            AccessKeyStatus status = AccessKeyStatus.valueOf(
                    entry.getAttribute(LDAPUtils.STATUS).getStringValue()
                    .toUpperCase());
            accessKey.setStatus(status);

            String createTime = DateUtil.toServerResponseFormat(
                    entry.getAttribute(LDAPUtils.CREATE_TIMESTAMP)
                    .getStringValue()
            );
            accessKey.setCreateDate(createTime);

            String objectClass = entry.getAttribute(LDAPUtils.OBJECT_CLASS)
                    .getStringValue();
            if (objectClass.equalsIgnoreCase("fedaccesskey")) {
                String expiry = DateUtil.toServerResponseFormat(
                        entry.getAttribute(LDAPUtils.EXPIRY)
                        .getStringValue());

                accessKey.setExpiry(expiry);
                accessKey.setToken(entry.getAttribute(LDAPUtils.TOKEN)
                        .getStringValue()
                );
            }
        }

        return accessKey;
    }

    @Override public AccessKey findFromToken(String secretToken)
        throws DataAccessException {
      AccessKey accKey = new AccessKey();
      accKey.setToken(secretToken);

      String[] dnattrs = {LDAPUtils.USER_ID,     LDAPUtils.SECRET_KEY,
                          LDAPUtils.EXPIRY,      LDAPUtils.ACCESS_KEY_ID,
                          LDAPUtils.STATUS,      LDAPUtils.CREATE_TIMESTAMP,
                          LDAPUtils.OBJECT_CLASS};

      String accKeyBaseDN =
          String.format("%s=accesskeys,%s", LDAPUtils.ORGANIZATIONAL_UNIT_NAME,
                        LDAPUtils.BASE_DN);

      String dnfilter = String.format("%s=%s", LDAPUtils.TOKEN, secretToken);

      LDAPSearchResults ldapResultsForToken;

        try {
          ldapResultsForToken = LDAPUtils.search(
              accKeyBaseDN, LDAPConnection.SCOPE_SUB, dnfilter, dnattrs);
        } catch (LDAPException ex) {
            LOGGER.error("Failed to find Access Key.");
            throw new DataAccessException("Access key find failed.\n" + ex);
        }

        if (ldapResultsForToken.hasMore()) {
          LDAPEntry ldapEntry;
            try {
              ldapEntry = ldapResultsForToken.next();
            } catch (LDAPException ex) {
                LOGGER.error("Failed to update AccessKey.");
                throw new DataAccessException("Failed to update AccessKey.\n"
                        + ex);
            }

            accKey.setUserId(
                ldapEntry.getAttribute(LDAPUtils.USER_ID).getStringValue());
            accKey.setSecretKey(
                ldapEntry.getAttribute(LDAPUtils.SECRET_KEY).getStringValue());
            AccessKeyStatus status =
                AccessKeyStatus.valueOf(ldapEntry.getAttribute(LDAPUtils.STATUS)
                                            .getStringValue()
                                            .toUpperCase());
            accKey.setStatus(status);

            String keyCreateTime = DateUtil.toServerResponseFormat(
                ldapEntry.getAttribute(LDAPUtils.CREATE_TIMESTAMP)
                    .getStringValue());
            accKey.setCreateDate(keyCreateTime);

            String keyObjectClass =
                ldapEntry.getAttribute(LDAPUtils.OBJECT_CLASS).getStringValue();

            if (keyObjectClass.equalsIgnoreCase("fedaccesskey")) {
              String expiry = DateUtil.toServerResponseFormat(
                  ldapEntry.getAttribute(LDAPUtils.EXPIRY).getStringValue());

              accKey.setExpiry(expiry);
              accKey.setId(ldapEntry.getAttribute(LDAPUtils.ACCESS_KEY_ID)
                               .getStringValue());
            }
        }

        return accKey;
    }

    /**
     * Get the access key belonging to the user from LDAP. Federated access keys
     * belonging to the user should not be displayed.
     *
     * @param user User
     * @return AccessKey List
     * @throws com.seagates3.exception.DataAccessException
     */
    @Override
    public AccessKey[] findAll(User user) throws DataAccessException {
        ArrayList accessKeys = new ArrayList();
        AccessKey accessKey;

        String[] attrs = {LDAPUtils.ACCESS_KEY_ID, LDAPUtils.STATUS,
            LDAPUtils.CREATE_TIMESTAMP};

        String accessKeyBaseDN = String.format("%s=accesskeys,%s",
                LDAPUtils.ORGANIZATIONAL_UNIT_NAME, LDAPUtils.BASE_DN
        );

        String filter = String.format("(&(%s=%s)(%s=%s))",
                LDAPUtils.USER_ID, user.getId(), LDAPUtils.OBJECT_CLASS,
                LDAPUtils.ACCESS_KEY_OBJECT_CLASS);

        LDAPSearchResults ldapResults;
        try {
            ldapResults = LDAPUtils.search(accessKeyBaseDN,
                    LDAPConnection.SCOPE_SUB, filter, attrs);
        } catch (LDAPException ex) {
            LOGGER.error("Failed to search access keys.");
            throw new DataAccessException("Failed to search access keys" + ex);
        }

        AccessKeyStatus accessKeystatus;
        LDAPEntry entry;
        while (ldapResults.hasMore()) {
            accessKey = new AccessKey();
            try {
                entry = ldapResults.next();
            } catch (LDAPException ex) {
                LOGGER.error("Access key find failed.");
                throw new DataAccessException("Access key find failed.\n" + ex);
            }

            accessKey.setId(entry.getAttribute(LDAPUtils.ACCESS_KEY_ID).getStringValue());
            accessKeystatus = AccessKeyStatus.valueOf(
                    entry.getAttribute(LDAPUtils.STATUS).getStringValue()
                    .toUpperCase());
            accessKey.setStatus(accessKeystatus);

            String createTime = DateUtil.toServerResponseFormat(
                    entry.getAttribute(LDAPUtils.CREATE_TIMESTAMP).getStringValue());
            accessKey.setCreateDate(createTime);

            accessKeys.add(accessKey);
        }

        AccessKey[] accessKeyList = new AccessKey[accessKeys.size()];
        return (AccessKey[]) accessKeys.toArray(accessKeyList);
    }

    /**
     * Return true if the user has an access key.
     *
     * @param userId User ID.
     * @return True if user has access keys.
     * @throws com.seagates3.exception.DataAccessException
     */
    @Override
    public Boolean hasAccessKeys(String userId) throws DataAccessException {
        return getCount(userId) > 0;
    }

    /**
     * Return the no of access keys which a user has.
     *
     * @param userId User ID.
     * @return no of access keys.
     * @throws com.seagates3.exception.DataAccessException
     */
    @Override
    public int getCount(String userId) throws DataAccessException {
        String[] attrs = new String[]{LDAPUtils.ACCESS_KEY_ID};
        int count = 0;

        String filter = String.format("(&(%s=%s)(%s=%s))",
                LDAPUtils.USER_ID, userId, LDAPUtils.OBJECT_CLASS,
                LDAPUtils.ACCESS_KEY_OBJECT_CLASS);

        String accessKeyBaseDN = String.format("%s=accesskeys,%s",
                LDAPUtils.ORGANIZATIONAL_UNIT_NAME, LDAPUtils.BASE_DN);

        LDAPSearchResults ldapResults;
        try {

            ldapResults = LDAPUtils.search(accessKeyBaseDN,
                    LDAPConnection.SCOPE_SUB, filter, attrs);

            /**
             * TODO - Replace this iteration with existing getCount method if
             * available.
             */
            while (ldapResults.hasMore()) {
                count++;
                ldapResults.next();
            }

            return count;
        } catch (LDAPException ex) {
            LOGGER.error("Failed to get the count of user access keys"
                    + " for user id :" + userId);
            throw new DataAccessException("Failed to get the count of user "
                    + "access keys" + ex);
        }
    }

    /**
     * Delete the user access key.
     *
     * @param accessKey AccessKey
     * @throws com.seagates3.exception.DataAccessException
     */
    @Override
    public void delete(AccessKey accessKey) throws DataAccessException {
        String dn = String.format("%s=%s,%s=accesskeys,%s",
                LDAPUtils.ACCESS_KEY_ID, accessKey.getId(),
                LDAPUtils.ORGANIZATIONAL_UNIT_NAME, LDAPUtils.BASE_DN);
        try {
            LDAPUtils.delete(dn);
        } catch (LDAPException ex) {
            LOGGER.error("Failed to delete access key.");
            throw new DataAccessException("Failed to delete access key" + ex);
        }
    }

    /**
     * Save the access key.
     *
     * @param accessKey AccessKey
     * @throws com.seagates3.exception.DataAccessException
     */
    @Override
    public void save(AccessKey accessKey) throws DataAccessException {
        if (accessKey.getToken() != null) {
            saveFedAccessKey(accessKey);
        } else {
            saveAccessKey(accessKey);
        }
        try {
          // Added delay so that newly created keys are replicated in ldap
          Thread.sleep(500);
        }
        catch (InterruptedException e) {
          LOGGER.error("Exception occurred while saving access key", e);
          Thread.currentThread().interrupt();
        }
    }

    /**
     * Update the access key of the user.
     *
     * @param accessKey AccessKey
     * @param newStatus Updated Status
     * @throws com.seagates3.exception.DataAccessException
     */
    @Override
    public void update(AccessKey accessKey, String newStatus)
            throws DataAccessException {

        String dn = String.format("%s=%s,%s=accesskeys,%s",
                LDAPUtils.ACCESS_KEY_ID, accessKey.getId(),
                LDAPUtils.ORGANIZATIONAL_UNIT_NAME, LDAPUtils.BASE_DN
        );

        LDAPAttribute attr = new LDAPAttribute("status", newStatus);
        LDAPModification modify = new LDAPModification(LDAPModification.REPLACE,
                attr);

        try {
            LDAPUtils.modify(dn, modify);
        } catch (LDAPException ex) {
            LOGGER.error("Failed to update the access key of userId: "
                                             + accessKey.getUserId());
            throw new DataAccessException("Failed to update the access key" + ex);
        }
    }

    /**
     * Save the access key in LDAP.
     */
    private void saveAccessKey(AccessKey accessKey) throws DataAccessException {
        LDAPAttributeSet attributeSet = new LDAPAttributeSet();
        attributeSet.add(new LDAPAttribute(LDAPUtils.OBJECT_CLASS,
                LDAPUtils.ACCESS_KEY_OBJECT_CLASS));
        attributeSet.add(new LDAPAttribute(LDAPUtils.USER_ID, accessKey.getUserId()));
        attributeSet.add(new LDAPAttribute(LDAPUtils.ACCESS_KEY_ID,
                accessKey.getId()));
        attributeSet.add(new LDAPAttribute(LDAPUtils.SECRET_KEY,
                accessKey.getSecretKey()));
        attributeSet.add(new LDAPAttribute(LDAPUtils.STATUS,
                accessKey.getStatus()));

        String dn = String.format("%s=%s,%s=accesskeys,%s",
                LDAPUtils.ACCESS_KEY_ID, accessKey.getId(),
                LDAPUtils.ORGANIZATIONAL_UNIT_NAME, LDAPUtils.BASE_DN
        );

        try {
            LDAPUtils.add(new LDAPEntry(dn, attributeSet));
        } catch (LDAPException ex) {
            LOGGER.error("Failed to save access key of userId:"
                                        + accessKey.getUserId());
            throw new DataAccessException("Failed to save access key" + ex);
        }
    }

    /**
     * Save the federated access key in LDAP.
     */
    private void saveFedAccessKey(AccessKey accessKey)
            throws DataAccessException {
        String expiry = DateUtil.toLdapDate(accessKey.getExpiry());
        LDAPAttributeSet attributeSet = new LDAPAttributeSet();
        attributeSet.add(new LDAPAttribute(LDAPUtils.OBJECT_CLASS,
                LDAPUtils.FED_ACCESS_KEY_OBJECT_CLASS));
        attributeSet.add(new LDAPAttribute(LDAPUtils.USER_ID,
                accessKey.getUserId()));
        attributeSet.add(new LDAPAttribute(LDAPUtils.ACCESS_KEY_ID,
                accessKey.getId()));
        attributeSet.add(new LDAPAttribute(LDAPUtils.SECRET_KEY,
                accessKey.getSecretKey()));
        attributeSet.add(new LDAPAttribute(LDAPUtils.TOKEN,
                accessKey.getToken()));
        attributeSet.add(new LDAPAttribute(LDAPUtils.EXPIRY,
                expiry));
        attributeSet.add(new LDAPAttribute(LDAPUtils.STATUS,
                accessKey.getStatus()));

        String dn = String.format("%s=%s,%s=accesskeys,%s",
                LDAPUtils.ACCESS_KEY_ID, accessKey.getId(),
                LDAPUtils.ORGANIZATIONAL_UNIT_NAME, LDAPUtils.BASE_DN
        );

        try {
            LDAPUtils.add(new LDAPEntry(dn, attributeSet));
        } catch (LDAPException ex) {
            LOGGER.error("Failed to save federated access key.");
            throw new DataAccessException("Failed to save federated access key" + ex);
        }
    }
}
