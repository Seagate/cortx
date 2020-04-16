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
 * Original creation date: 31-Oct-2015
 */
package com.seagates3.dao.ldap;

import com.novell.ldap.LDAPAttribute;
import com.novell.ldap.LDAPAttributeSet;
import com.novell.ldap.LDAPConnection;
import com.novell.ldap.LDAPEntry;
import com.novell.ldap.LDAPException;
import com.novell.ldap.LDAPSearchResults;
import com.seagates3.dao.RoleDAO;
import com.seagates3.exception.DataAccessException;
import com.seagates3.model.Account;
import com.seagates3.model.Role;
import com.seagates3.util.DateUtil;
import java.util.ArrayList;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class RoleImpl implements RoleDAO {

    private final Logger LOGGER =
            LoggerFactory.getLogger(RoleImpl.class.getName());
    /**
     * Get the role from LDAP.
     *
     * Search for the role under
     * ou=roles,o=<account name>,ou=accounts,dc=s3,dc=seagate,dc=com
     *
     * @param account User Account details.
     * @param roleName Role name.
     * @return Role
     * @throws com.seagates3.exception.DataAccessException
     */
    @Override
    public Role find(Account account, String roleName) throws DataAccessException {
        Role role = new Role();
        role.setAccount(account);
        role.setName(roleName);

        String[] attrs = {LDAPUtils.ROLE_POLICY_DOC, LDAPUtils.ROLE_ID,
            LDAPUtils.PATH, LDAPUtils.CREATE_TIMESTAMP};

        String ldapBase = String.format("%s=%s,%s=%s,%s=%s,%s",
                LDAPUtils.ORGANIZATIONAL_UNIT_NAME, LDAPUtils.ROLE_OU,
                LDAPUtils.ORGANIZATIONAL_NAME, account.getName(),
                LDAPUtils.ORGANIZATIONAL_UNIT_NAME, LDAPUtils.ACCOUNT_OU,
                LDAPUtils.BASE_DN
        );
        String filter = String.format("(%s=%s)", LDAPUtils.ROLE_NAME, roleName);

        LDAPSearchResults ldapResults;

        LOGGER.debug("Searching role dn: " + ldapBase);

        try {
            ldapResults = LDAPUtils.search(ldapBase,
                    LDAPConnection.SCOPE_SUB, filter, attrs);
        } catch (LDAPException ex) {
            LOGGER.error("Failed to find the role: " + role.getName());
            throw new DataAccessException("Failed to find the role.\n" + ex);
        }

        if (ldapResults.hasMore()) {
            try {
                LDAPEntry entry = ldapResults.next();
                role.setRoleId(entry.getAttribute(
                        LDAPUtils.ROLE_ID).getStringValue());
                role.setPath(entry.getAttribute(
                        LDAPUtils.PATH).getStringValue());
                role.setRolePolicyDoc(entry.getAttribute(
                        LDAPUtils.ROLE_POLICY_DOC).getStringValue());

                String createTimeStamp = entry.getAttribute(
                        LDAPUtils.CREATE_TIMESTAMP).getStringValue();
                String createTime = DateUtil.toServerResponseFormat(
                        createTimeStamp);
                role.setCreateDate(createTime);
            } catch (LDAPException ex) {
                LOGGER.error("Failed to find details of role: " + role.getName());
                throw new DataAccessException("Failed to find role details.\n"
                        + ex);
            }
        }

        return role;
    }

    /**
     * Get all the roles with path prefix from LDAP.
     *
     * @param account User Account details.
     * @param pathPrefix Path prefix of roles.
     * @return Roles.
     * @throws com.seagates3.exception.DataAccessException
     */
    @Override
    public Role[] findAll(Account account, String pathPrefix)
            throws DataAccessException {
        ArrayList roles = new ArrayList();
        Role role;

        String[] attrs = {LDAPUtils.ROLE_NAME, LDAPUtils.ROLE_POLICY_DOC,
            LDAPUtils.ROLE_ID, LDAPUtils.PATH, LDAPUtils.CREATE_TIMESTAMP};

        String ldapBase = String.format("%s=%s,%s=%s,%s=%s,%s",
                LDAPUtils.ORGANIZATIONAL_UNIT_NAME, LDAPUtils.ROLE_OU,
                LDAPUtils.ORGANIZATIONAL_NAME, account.getName(),
                LDAPUtils.ORGANIZATIONAL_UNIT_NAME, LDAPUtils.ACCOUNT_OU,
                LDAPUtils.BASE_DN);
        String filter = String.format("(&(%s=%s*)(%s=%s))", LDAPUtils.PATH,
                pathPrefix, LDAPUtils.OBJECT_CLASS, LDAPUtils.ROLE_OBJECT_CLASS
        );

        LOGGER.debug("Searching roles dn: " + ldapBase);

        LDAPSearchResults ldapResults;
        try {
            ldapResults = LDAPUtils.search(ldapBase,
                    LDAPConnection.SCOPE_SUB, filter, attrs);
        } catch (LDAPException ex) {
            LOGGER.error("Failed to find all roles of account:"
                    + account.getName() + " pathPrefix: " + pathPrefix);
            throw new DataAccessException("Failed to find all roles.\n" + ex);
        }

        while (ldapResults.hasMore()) {
            role = new Role();
            LDAPEntry entry;
            try {
                entry = ldapResults.next();
            } catch (LDAPException ex) {
                LOGGER.error("Error fetching data from ldap.");
                throw new DataAccessException("Ldap failure.\n" + ex);
            }
            role.setAccount(account);
            role.setName(entry.getAttribute(LDAPUtils.ROLE_NAME)
                    .getStringValue());
            role.setRoleId(entry.getAttribute(
                    LDAPUtils.ROLE_ID).getStringValue());
            role.setPath(entry.getAttribute(LDAPUtils.PATH).getStringValue());
            role.setRolePolicyDoc(entry.getAttribute(LDAPUtils.ROLE_POLICY_DOC)
                    .getStringValue());

            String createTime = DateUtil.toServerResponseFormat(
                    entry.getAttribute(LDAPUtils.CREATE_TIMESTAMP)
                    .getStringValue());
            role.setCreateDate(createTime);

            roles.add(role);
        }

        Role[] roleList = new Role[roles.size()];
        return (Role[]) roles.toArray(roleList);
    }

    /**
     * Delete the role from LDAP.
     *
     * @param role Role.
     * @throws com.seagates3.exception.DataAccessException
     */
    @Override
    public void delete(Role role) throws DataAccessException {
        String dn = String.format("%s=%s,%s=%s,%s=%s,%s=%s,%s",
                LDAPUtils.ROLE_NAME, role.getName(),
                LDAPUtils.ORGANIZATIONAL_UNIT_NAME, LDAPUtils.ROLE_OU,
                LDAPUtils.ORGANIZATIONAL_NAME, role.getAccount().getName(),
                LDAPUtils.ORGANIZATIONAL_UNIT_NAME, LDAPUtils.ACCOUNT_OU,
                LDAPUtils.BASE_DN
        );

        LOGGER.debug("Deleting role: " + role.getName());

        try {
            LDAPUtils.delete(dn);
        } catch (LDAPException ex) {
            LOGGER.error("Failed to delete the role: " + role.getName());
            throw new DataAccessException("Failed to delete the role.\n" + ex);
        }
    }

    /**
     * Create a new entry for the role in LDAP.
     *
     * @param role Role
     * @throws com.seagates3.exception.DataAccessException
     */
    @Override
    public void save(Role role) throws DataAccessException {
        LDAPAttributeSet attributeSet = new LDAPAttributeSet();
        attributeSet.add(new LDAPAttribute(LDAPUtils.OBJECT_CLASS,
                LDAPUtils.ROLE_OBJECT_CLASS));
        attributeSet.add(new LDAPAttribute(LDAPUtils.ROLE_NAME, role.getName()));
        attributeSet.add(new LDAPAttribute(LDAPUtils.ROLE_ID, role.getRoleId()));
        attributeSet.add(new LDAPAttribute(LDAPUtils.ROLE_POLICY_DOC,
                role.getRolePolicyDoc()));
        attributeSet.add(new LDAPAttribute(LDAPUtils.PATH, role.getPath()));

        String dn = String.format("%s=%s,%s=%s,%s=%s,%s=%s,%s",
                LDAPUtils.ROLE_NAME, role.getName(),
                LDAPUtils.ORGANIZATIONAL_UNIT_NAME, LDAPUtils.ROLE_OU,
                LDAPUtils.ORGANIZATIONAL_NAME, role.getAccount().getName(),
                LDAPUtils.ORGANIZATIONAL_UNIT_NAME, LDAPUtils.ACCOUNT_OU,
                LDAPUtils.BASE_DN);

        LOGGER.debug("Creating role: " + role.getName());

        try {
            LDAPUtils.add(new LDAPEntry(dn, attributeSet));
        } catch (LDAPException ex) {
            LOGGER.error("Failed to create the role: " + role.getName());
            throw new DataAccessException("Failed to create role.\n" + ex);
        }
    }
}
