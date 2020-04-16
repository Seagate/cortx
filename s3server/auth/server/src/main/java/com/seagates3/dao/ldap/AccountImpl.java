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

import java.util.ArrayList;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.novell.ldap.LDAPAttribute;
import com.novell.ldap.LDAPAttributeSet;
import com.novell.ldap.LDAPConnection;
import com.novell.ldap.LDAPEntry;
import com.novell.ldap.LDAPException;
import com.novell.ldap.LDAPSearchResults;
import com.seagates3.dao.AccountDAO;
import com.seagates3.exception.DataAccessException;
import com.seagates3.fi.FaultPoints;
import com.seagates3.model.Account;

public class AccountImpl implements AccountDAO {

  private
   final Logger LOGGER = LoggerFactory.getLogger(AccountImpl.class.getName());

    @Override
    public Account findByID(String accountID) throws DataAccessException {
        Account account = new Account();

        String[] attrs = {LDAPUtils.ORGANIZATIONAL_NAME,
                          LDAPUtils.CANONICAL_ID};
        String filter = String.format("(&(%s=%s)(%s=%s))",
                LDAPUtils.ACCOUNT_ID, accountID, LDAPUtils.OBJECT_CLASS,
                LDAPUtils.ACCOUNT_OBJECT_CLASS);

        LOGGER.debug("Searching account id: " + accountID);

        LDAPSearchResults ldapResults;
        try {
          ldapResults = LDAPUtils.search(
              LDAPUtils.BASE_DN, LDAPConnection.SCOPE_SUB, filter, attrs);
        } catch (LDAPException ex) {
            LOGGER.error("Failed to search account details."
                    + "of account id: " + accountID);
            throw new DataAccessException("failed to search account.\n" + ex);
        }

        if (ldapResults.hasMore()) {
            try {
                LDAPEntry entry = ldapResults.next();
                account.setId(accountID);
                account.setName(
                    entry.getAttribute(LDAPUtils.ORGANIZATIONAL_NAME)
                        .getStringValue());
                account.setCanonicalId(
                    entry.getAttribute(LDAPUtils.CANONICAL_ID)
                        .getStringValue());
            } catch (LDAPException ex) {
                LOGGER.error("Failed to find account details."
                        + "of account id: " + accountID);
                throw new DataAccessException(
                    "Failed to find account details.\n" + ex);
            }
        }

        return account;
    }

    @Override public Account findByCanonicalID(String canonicalID)
        throws DataAccessException {
        Account account = new Account();

        String[] attrs = {LDAPUtils.ORGANIZATIONAL_NAME, LDAPUtils.ACCOUNT_ID};
        String filter = String.format("(&(%s=%s)(%s=%s))",
                LDAPUtils.CANONICAL_ID, canonicalID, LDAPUtils.OBJECT_CLASS,
                LDAPUtils.ACCOUNT_OBJECT_CLASS);

        LOGGER.debug("Searching canonical id: " + canonicalID);

        LDAPSearchResults ldapResults;
        try {
          ldapResults = LDAPUtils.search(
              LDAPUtils.BASE_DN, LDAPConnection.SCOPE_SUB, filter, attrs);
        } catch (LDAPException ex) {
          LOGGER.error("Failed to search account " + "of canonical Id " +
                       canonicalID);
            throw new DataAccessException("failed to search account.\n" + ex);
        }

        if (ldapResults.hasMore()) {
            try {
                LDAPEntry entry = ldapResults.next();
                account.setCanonicalId(canonicalID);
                account.setName(
                    entry.getAttribute(LDAPUtils.ORGANIZATIONAL_NAME)
                        .getStringValue());
                account.setId(
                    entry.getAttribute(LDAPUtils.ACCOUNT_ID).getStringValue());
            } catch (LDAPException ex) {
                LOGGER.error("Failed to find account details."
                        + "of canonical id: " + canonicalID);
                throw new DataAccessException(
                    "Failed to find account details.\n" + ex);
            }
        }

        return account;
    }

    /**
     * Fetch account details from LDAP.
     *
     * @param name Account name
     * @return Account
     * @throws com.seagates3.exception.DataAccessException
     */
    @Override
    public Account find(String name) throws DataAccessException {
        Account account = new Account();
        account.setName(name);

        String[] attrs = {
            LDAPUtils.ACCOUNT_ID,          LDAPUtils.CANONICAL_ID,
            LDAPUtils.PASSWORD,            LDAPUtils.PASSWORD_RESET_REQUIRED,
            LDAPUtils.PROFILE_CREATE_DATE, LDAPUtils.EMAIL};
        String filter = String.format("(&(%s=%s)(%s=%s))",
                LDAPUtils.ORGANIZATIONAL_NAME, name, LDAPUtils.OBJECT_CLASS,
                LDAPUtils.ACCOUNT_OBJECT_CLASS);

        LDAPSearchResults ldapResults;
        LOGGER.debug("Searching account: " + name + " filter: " + filter);
        try {
          ldapResults = LDAPUtils.search(
              LDAPUtils.BASE_DN, LDAPConnection.SCOPE_SUB, filter, attrs);
        } catch (LDAPException ex) {
            LOGGER.error("Failed to search account: " + name);
            throw new DataAccessException("failed to search account.\n" + ex);
        }

        if (ldapResults != null && ldapResults.hasMore()) {
            try {
              if (FaultPoints.fiEnabled() &&
                  FaultPoints.getInstance().isFaultPointActive(
                      "LDAP_GET_ATTR_FAIL")) {
                    throw new LDAPException();
                }

                LDAPEntry entry = ldapResults.next();
                account.setId(
                    entry.getAttribute(LDAPUtils.ACCOUNT_ID).getStringValue());
                account.setCanonicalId(
                    entry.getAttribute(LDAPUtils.CANONICAL_ID)
                        .getStringValue());
                account.setEmail(
                    entry.getAttribute(LDAPUtils.EMAIL).getStringValue());

                try {
                  account.setPassword(
                      entry.getAttribute(LDAPUtils.PASSWORD).getStringValue());
                }
                catch (Exception e) {
                  LOGGER.debug("Password value not found in ldap");
                }
                try {
                  account.setPwdResetRequired(
                      entry.getAttribute(LDAPUtils.PASSWORD_RESET_REQUIRED)
                          .getStringValue());
                }
                catch (Exception e) {
                  LOGGER.debug("pwdReset required value not found in ldap");
                }
                try {
                  account.setProfileCreateDate(
                      (entry.getAttribute(LDAPUtils.PROFILE_CREATE_DATE)
                           .getStringValue()));
                }
                catch (Exception e) {
                  LOGGER.debug("profileCreateDate value not found in ldap");
                }
            } catch (LDAPException ex) {
                LOGGER.error("Failed to find details of account: " + name);
                throw new DataAccessException(
                    "Failed to find account details.\n" + ex);
            }
        }
        return account;
    }

    /*
     * fetch all accounts from database
     */
    public Account[] findAll() throws DataAccessException {
       ArrayList<Account> accounts = new ArrayList<Account>();
        Account account;
        LDAPSearchResults ldapResults;
        /*
         * search base: the starting point for search example:
         * 'ou=accounts,dc=s3,dc=seagate,dc=com'
         */
        String baseDn = String.format("%s=%s,%s",
                LDAPUtils.ORGANIZATIONAL_UNIT_NAME, LDAPUtils.ACCOUNT_OU,
                LDAPUtils.BASE_DN);
        /*
         * search filter: '(objectClass=account)'
         */
        String accountFilter = String.format("(%s=%s)", LDAPUtils.OBJECT_CLASS,
                                             LDAPUtils.ACCOUNT_OBJECT_CLASS);
        String[] attr = {LDAPUtils.ORGANIZATIONAL_NAME, LDAPUtils.ACCOUNT_ID,
                         LDAPUtils.EMAIL,               LDAPUtils.CANONICAL_ID};

        LOGGER.debug("Searching baseDn: " + baseDn + " account filter: " +
                     accountFilter);

        try {
            ldapResults = LDAPUtils.search(baseDn, LDAPConnection.SCOPE_SUB,
                    accountFilter, attr);
        } catch (LDAPException ex) {
            LOGGER.error("Failed to fetch accounts.");
            throw new DataAccessException("Failed to fetch accounts.\n" + ex);
        }
        while (ldapResults.hasMore()) {
            LDAPEntry ldapEntry;
            account = new Account();
            try {
              if (FaultPoints.fiEnabled() &&
                  FaultPoints.getInstance().isFaultPointActive(
                      "LDAP_GET_ATTR_FAIL")) {
                    throw new LDAPException();
                }

                ldapEntry = ldapResults.next();
            } catch (LDAPException ldapException) {
                LOGGER.error("Failed to read ldapEntry.");
                throw new DataAccessException("Failed to read ldapEntry.\n" +
                                              ldapException);
            }
            account.setName(
                ldapEntry.getAttribute(LDAPUtils.ORGANIZATIONAL_NAME)
                    .getStringValue());
            account.setId(
                ldapEntry.getAttribute(LDAPUtils.ACCOUNT_ID).getStringValue());
            account.setEmail(
                ldapEntry.getAttribute(LDAPUtils.EMAIL).getStringValue());
            account.setCanonicalId(
                ldapEntry.getAttribute(LDAPUtils.CANONICAL_ID)
                    .getStringValue());
            accounts.add(account);
        }
        Account[] accountList = new Account[accounts.size()];

        return (Account[]) accounts.toArray(accountList);
    }

    /**
     * Create a new entry under ou=accounts in openldap. Example dn: o=<account
     * name>,ou=accounts,dc=s3,dc=seagate,dc=com
     *
     * Create sub entries ou=user and ou=roles under the new account. dn:
     * ou=users,o=<account name>,ou=accounts,dc=s3,dc=seagate,dc=com dn:
     * ou=roles,o=<account name>,ou=accounts,dc=s3,dc=seagate,dc=com
     *
     * @param account Account
     * @throws com.seagates3.exception.DataAccessException
     */
    @Override
    public void save(Account account) throws DataAccessException {
        String dn = String.format("%s=%s,%s=accounts,%s",
                LDAPUtils.ORGANIZATIONAL_NAME, account.getName(),
                LDAPUtils.ORGANIZATIONAL_UNIT_NAME, LDAPUtils.BASE_DN);

        LDAPAttributeSet attributeSet = new LDAPAttributeSet();
        attributeSet.add(new LDAPAttribute(LDAPUtils.OBJECT_CLASS, "Account"));
        attributeSet.add(
            new LDAPAttribute(LDAPUtils.ACCOUNT_ID, account.getId()));
        attributeSet.add(new LDAPAttribute(LDAPUtils.ORGANIZATIONAL_NAME,
                account.getName()));
        attributeSet.add(
            new LDAPAttribute(LDAPUtils.EMAIL, account.getEmail()));
        attributeSet.add(new LDAPAttribute(LDAPUtils.CANONICAL_ID,
                account.getCanonicalId()));

        LOGGER.debug("Saving account dn: " + dn);

        try {
            LDAPUtils.add(new LDAPEntry(dn, attributeSet));
        } catch (LDAPException ex) {
            LOGGER.error("Failed to add new account: " + account.getName());
            throw new DataAccessException("failed to add new account.\n" + ex);
        }

        createUserOU(account.getName());
        createRoleOU(account.getName());
        createGroupsOU(account.getName());
        createPolicyOU(account.getName());
    }

    /**
     * Delete account The dn format: o=<account
     * name>,ou=accounts,dc=s3,dc=seagate,dc=com
     */
    @Override
    public void delete(Account account) throws DataAccessException {
        String dn = String.format("%s=%s,%s=accounts,%s",
                LDAPUtils.ORGANIZATIONAL_NAME, account.getName(),
                LDAPUtils.ORGANIZATIONAL_UNIT_NAME, LDAPUtils.BASE_DN);

        LOGGER.debug("Deleting account dn: " + dn);

        try {
            LDAPUtils.delete(dn);
        } catch (LDAPException ex) {
            LOGGER.error("Failed to delete account: " + account.getName());
            throw new DataAccessException("Failed to delete account.\n" + ex);
        }
    }

    /**
     * Delete given ou from LDAP.
     *
     * The dn format: ou=<ou>,o=<account
     *name>,ou=accounts,dc=s3,dc=seagate,dc=com
     */
   public
    void deleteOu(Account account, String ou) throws DataAccessException {
      String dn = String.format(
          "%s=%s,%s=%s,%s=%s,%s", LDAPUtils.ORGANIZATIONAL_UNIT_NAME, ou,
          LDAPUtils.ORGANIZATIONAL_NAME, account.getName(),
          LDAPUtils.ORGANIZATIONAL_UNIT_NAME, LDAPUtils.ACCOUNT_OU,
          LDAPUtils.BASE_DN);

        LOGGER.debug("Deleting account dn: " + dn);

        try {
            LDAPUtils.delete(dn);
        } catch (LDAPException ex) {
            LOGGER.error("Failed to delete dn: " + dn);
            throw new DataAccessException("Failed to delete " + ou + " ou.\n" +
                                          ex);
        }
    }

    /**
     * Create sub entry ou=users for the account. The dn should be in the
     * following
     * format dn: ou=users,o=<account name>,ou=accounts,dc=s3,dc=seagate,dc=com
     */
    private void createUserOU(String accountName) throws DataAccessException {
       String dn = String.format(
           "%s=%s,%s=%s,%s=%s,%s", LDAPUtils.ORGANIZATIONAL_UNIT_NAME,
           LDAPUtils.USER_OU, LDAPUtils.ORGANIZATIONAL_NAME, accountName,
           LDAPUtils.ORGANIZATIONAL_UNIT_NAME, LDAPUtils.ACCOUNT_OU,
           LDAPUtils.BASE_DN);

        LDAPAttributeSet attributeSet = new LDAPAttributeSet();
        attributeSet.add(new LDAPAttribute(LDAPUtils.OBJECT_CLASS,
                LDAPUtils.ORGANIZATIONAL_UNIT_CLASS));
        attributeSet.add(new LDAPAttribute(LDAPUtils.ORGANIZATIONAL_UNIT_NAME,
                LDAPUtils.USER_OU));

        LOGGER.debug("Creating user dn: " + dn);

        try {
            LDAPUtils.add(new LDAPEntry(dn, attributeSet));
        } catch (LDAPException ex) {
          LOGGER.error("Failed to create dn: " + dn);
            throw new DataAccessException("failed to create user ou.\n" + ex);
        }
    }

    /**
     * Create sub entry ou=roles for the account. The dn should be in the
     * following
     * format dn: ou=roles,o=<account name>,ou=accounts,dc=s3,dc=seagate,dc=com
     */
    private void createRoleOU(String accountName) throws DataAccessException {
       String dn = String.format(
           "%s=%s,%s=%s,%s=%s,%s", LDAPUtils.ORGANIZATIONAL_UNIT_NAME,
           LDAPUtils.ROLE_OU, LDAPUtils.ORGANIZATIONAL_NAME, accountName,
           LDAPUtils.ORGANIZATIONAL_UNIT_NAME, LDAPUtils.ACCOUNT_OU,
           LDAPUtils.BASE_DN);

        LDAPAttributeSet attributeSet = new LDAPAttributeSet();
        attributeSet.add(new LDAPAttribute(LDAPUtils.OBJECT_CLASS,
                LDAPUtils.ORGANIZATIONAL_UNIT_CLASS));
        attributeSet.add(new LDAPAttribute("ou", "roles"));

        LOGGER.debug("Creating role dn: " + dn);

        try {
            LDAPUtils.add(new LDAPEntry(dn, attributeSet));
        } catch (LDAPException ex) {
            LOGGER.error("Failed to create role dn: " + dn);
            throw new DataAccessException("failed to create role ou.\n" + ex);
        }
    }

    /**
     * Create sub entry ou=policies for the account. The dn should be in the
     * following format dn: ou=policies,o=<account
     * name>,ou=accounts,dc=s3,dc=seagate,dc=com
     */
   private
    void createPolicyOU(String accountName) throws DataAccessException {
      String dn = String.format(
          "%s=%s,%s=%s,%s=%s,%s", LDAPUtils.ORGANIZATIONAL_UNIT_NAME,
          LDAPUtils.POLICY_OU, LDAPUtils.ORGANIZATIONAL_NAME, accountName,
          LDAPUtils.ORGANIZATIONAL_UNIT_NAME, LDAPUtils.ACCOUNT_OU,
          LDAPUtils.BASE_DN);

        LDAPAttributeSet attributeSet = new LDAPAttributeSet();
        attributeSet.add(new LDAPAttribute(LDAPUtils.OBJECT_CLASS,
                LDAPUtils.ORGANIZATIONAL_UNIT_CLASS));
        attributeSet.add(new LDAPAttribute(LDAPUtils.ORGANIZATIONAL_UNIT_NAME,
                LDAPUtils.POLICY_OU));

        LOGGER.debug("Creating Policy dn: " + dn);

        try {
            LDAPUtils.add(new LDAPEntry(dn, attributeSet));
        } catch (LDAPException ex) {
            LOGGER.error("Failed to create policy dn: " + dn);
            throw new DataAccessException("failed to create policy ou.\n" + ex);
        }
    }

    /**
     * Create sub entry ou=groups for the account. The dn should be in the
     * following
     * format dn: ou=groups,o=<account name>,ou=accounts,dc=s3,dc=seagate,dc=com
     */
   private
    void createGroupsOU(String accountName) throws DataAccessException {
      String dn = String.format(
          "%s=%s,%s=%s,%s=%s,%s", LDAPUtils.ORGANIZATIONAL_UNIT_NAME,
          LDAPUtils.GROUP_OU, LDAPUtils.ORGANIZATIONAL_NAME, accountName,
          LDAPUtils.ORGANIZATIONAL_UNIT_NAME, LDAPUtils.ACCOUNT_OU,
          LDAPUtils.BASE_DN);

        LOGGER.debug("Creating Groups dn: " + dn);

        LDAPAttributeSet attributeSet = new LDAPAttributeSet();
        attributeSet.add(new LDAPAttribute(LDAPUtils.OBJECT_CLASS,
                LDAPUtils.ORGANIZATIONAL_UNIT_CLASS));
        attributeSet.add(new LDAPAttribute(LDAPUtils.ORGANIZATIONAL_UNIT_NAME,
                LDAPUtils.GROUP_OU));

        try {
            LDAPUtils.add(new LDAPEntry(dn, attributeSet));
        } catch (LDAPException ex) {
            LOGGER.error("Failed to create groups dn: " + dn);
            throw new DataAccessException("failed to create groups ou.\n" + ex);
        }
    }

    @Override public Account findByEmailAddress(String emailAddress)
        throws DataAccessException {
      Account account = new Account();
      account.setEmail(emailAddress);

      String[] attrs = {LDAPUtils.ORGANIZATIONAL_NAME, LDAPUtils.ACCOUNT_ID,
                        LDAPUtils.CANONICAL_ID};
      String filter =
          String.format("(&(%s=%s)(%s=%s))", LDAPUtils.EMAIL, emailAddress,
                        LDAPUtils.OBJECT_CLASS, LDAPUtils.ACCOUNT_OBJECT_CLASS);

      LOGGER.debug("Searching email address: " + emailAddress);

      LDAPSearchResults ldapResults;
        try {
          ldapResults = LDAPUtils.search(
              LDAPUtils.BASE_DN, LDAPConnection.SCOPE_SUB, filter, attrs);
        }
        catch (LDAPException ex) {
          LOGGER.error("Failed to search account " + "of email address " +
                       emailAddress);
          throw new DataAccessException("failed to search account.\n" + ex);
        }

        if (ldapResults.hasMore()) {
          try {
            LDAPEntry entry = ldapResults.next();
            account.setName(entry.getAttribute(LDAPUtils.ORGANIZATIONAL_NAME)
                                .getStringValue());
            account.setId(
                entry.getAttribute(LDAPUtils.ACCOUNT_ID).getStringValue());
            account.setCanonicalId(
                entry.getAttribute(LDAPUtils.CANONICAL_ID).getStringValue());
          }
          catch (LDAPException ex) {
            LOGGER.error("Failed to find account details." +
                         "with email address: " + emailAddress);
            throw new DataAccessException("Failed to find account details.\n" +
                                          ex);
          }
        }

        return account;
    }
}


