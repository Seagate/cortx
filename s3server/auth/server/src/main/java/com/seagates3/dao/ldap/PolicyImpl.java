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
 * Original creation date: 19-May-2016
 */
package com.seagates3.dao.ldap;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.novell.ldap.LDAPAttribute;
import com.novell.ldap.LDAPAttributeSet;
import com.novell.ldap.LDAPConnection;
import com.novell.ldap.LDAPEntry;
import com.novell.ldap.LDAPException;
import com.novell.ldap.LDAPSearchResults;
import com.seagates3.dao.PolicyDAO;
import com.seagates3.exception.DataAccessException;
import com.seagates3.model.Account;
import com.seagates3.model.Policy;
import com.seagates3.util.DateUtil;

public class PolicyImpl implements PolicyDAO {

    private final Logger LOGGER =
            LoggerFactory.getLogger(PolicyImpl.class.getName());
    /**
     * Find the policy.
     *
     * @param account
     * @param policyName
     * @return
     * @throws DataAccessException
     */
    @Override
    public Policy find(Account account, String policyName)
            throws DataAccessException {
        Policy policy = new Policy();
        policy.setAccount(account);
        policy.setName(policyName);

        String[] attrs = {LDAPUtils.POLICY_ID, LDAPUtils.PATH,
            LDAPUtils.CREATE_TIMESTAMP, LDAPUtils.MODIFY_TIMESTAMP,
            LDAPUtils.DEFAULT_VERSION_ID, LDAPUtils.POLICY_DOC
        };

        String ldapBase = String.format("%s=%s,%s=%s,%s=%s,%s",
                LDAPUtils.ORGANIZATIONAL_UNIT_NAME, LDAPUtils.POLICY_OU,
                LDAPUtils.ORGANIZATIONAL_NAME, account.getName(),
                LDAPUtils.ORGANIZATIONAL_UNIT_NAME, LDAPUtils.ACCOUNT_OU,
                LDAPUtils.BASE_DN
        );
        String filter = String.format("(%s=%s)", LDAPUtils.POLICY_NAME,
                policyName);

        LDAPSearchResults ldapResults;

        LOGGER.debug("Searching policy dn: " + ldapBase + " filter: "
                                                            + filter);

        try {
            ldapResults = LDAPUtils.search(ldapBase,
                    LDAPConnection.SCOPE_SUB, filter, attrs);
        } catch (LDAPException ex) {
            LOGGER.error("Failed to find the policy: " + policy.getName()
                                                  + " filter: " + filter);
            throw new DataAccessException("Failed to find the policy.\n" + ex);
        }

        if (ldapResults.hasMore()) {
            try {
                LDAPEntry entry = ldapResults.next();
                policy.setPolicyId(entry.getAttribute(LDAPUtils.POLICY_ID).
                        getStringValue());
                policy.setPath(entry.getAttribute(LDAPUtils.PATH).
                        getStringValue());
                policy.setDefaultVersionId(entry.getAttribute(
                        LDAPUtils.DEFAULT_VERSION_ID).getStringValue());
                policy.setPolicyDoc(entry.getAttribute(
                        LDAPUtils.POLICY_DOC).getStringValue());

                String createTimeStamp = entry.getAttribute(
                        LDAPUtils.CREATE_TIMESTAMP).getStringValue();
                String createTime = DateUtil.toServerResponseFormat(
                        createTimeStamp);
                policy.setCreateDate(createTime);

                String modifyTimeStamp = entry.getAttribute(
                        LDAPUtils.MODIFY_TIMESTAMP).getStringValue();
                String modifiedTime = DateUtil.toServerResponseFormat(
                        modifyTimeStamp);
                policy.setUpdateDate(modifiedTime);
            } catch (LDAPException ex) {
                LOGGER.error("Failed to find details of policy: "
                                            + policy.getName());
                throw new DataAccessException("Failed to find policy details. \n" + ex);
            }
        }

        return policy;
    }

    /**
     * Save the policy.
     *
     * @param policy
     * @throws DataAccessException
     */
    @Override
    public void save(Policy policy) throws DataAccessException {
        LDAPAttributeSet attributeSet = new LDAPAttributeSet();

        attributeSet.add(new LDAPAttribute(LDAPUtils.OBJECT_CLASS,
                LDAPUtils.POLICY_OBJECT_CLASS));
        attributeSet.add(new LDAPAttribute(LDAPUtils.POLICY_NAME,
                policy.getName()));
        attributeSet.add(new LDAPAttribute(LDAPUtils.POLICY_DOC,
                policy.getPolicyDoc()));
        attributeSet.add(new LDAPAttribute(LDAPUtils.PATH, policy.getPath()));
        attributeSet.add(new LDAPAttribute(LDAPUtils.DEFAULT_VERSION_ID,
                policy.getDefaultVersionid()));
        attributeSet.add(new LDAPAttribute(LDAPUtils.POLICY_ID,
                policy.getPolicyId()));

        if (policy.getDescription() != null) {
            attributeSet.add(new LDAPAttribute(LDAPUtils.DESCRIPTION,
                    policy.getDescription()));
        }

        String dn = String.format("%s=%s,%s=%s,%s=%s,%s=%s,%s",
                LDAPUtils.POLICY_NAME, policy.getName(),
                LDAPUtils.ORGANIZATIONAL_UNIT_NAME, LDAPUtils.POLICY_OU,
                LDAPUtils.ORGANIZATIONAL_NAME, policy.getAccount().getName(),
                LDAPUtils.ORGANIZATIONAL_UNIT_NAME, LDAPUtils.ACCOUNT_OU,
                LDAPUtils.BASE_DN);

        LOGGER.debug("Saving Policy dn: " + dn);

        try {
            LDAPUtils.add(new LDAPEntry(dn, attributeSet));
        } catch (LDAPException ex) {
            LOGGER.error("Failed to create policy: " + policy.getName());
            throw new DataAccessException("Failed to create policy.\n" + ex);
        }
    }
}
