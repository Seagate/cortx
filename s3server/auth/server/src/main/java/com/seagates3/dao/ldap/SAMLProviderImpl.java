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
 * Original creation date: 15-Oct-2015
 */
package com.seagates3.dao.ldap;

import com.novell.ldap.LDAPAttribute;
import com.novell.ldap.LDAPAttributeSet;
import com.novell.ldap.LDAPConnection;
import com.novell.ldap.LDAPEntry;
import com.novell.ldap.LDAPException;
import com.novell.ldap.LDAPModification;
import com.novell.ldap.LDAPSearchResults;
import com.seagates3.dao.AccountDAO;
import com.seagates3.dao.DAODispatcher;
import com.seagates3.dao.DAOResource;
import com.seagates3.dao.SAMLProviderDAO;
import com.seagates3.exception.DataAccessException;
import com.seagates3.model.Account;
import com.seagates3.model.SAMLMetadataTokens;
import com.seagates3.model.SAMLProvider;
import com.seagates3.util.DateUtil;
import com.seagates3.util.JSONUtil;
import java.security.cert.Certificate;
import java.security.cert.CertificateEncodingException;
import java.util.ArrayList;

public class SAMLProviderImpl implements SAMLProviderDAO {

    @Override
    public SAMLProvider find(String issuer)
            throws DataAccessException {
        SAMLProvider samlProvider = new SAMLProvider();

        String[] attrs = {LDAPUtils.ACCOUNT_ID, LDAPUtils.COMMON_NAME,
            LDAPUtils.EXPIRY, LDAPUtils.SAML_TOKENS_JSON};

        String ldapBase = String.format("%s=%s,%s",
                LDAPUtils.ORGANIZATIONAL_UNIT_NAME, LDAPUtils.IDP_OU,
                LDAPUtils.BASE_DN);

        String filter = String.format("%s=%s", LDAPUtils.ISSUER,
                issuer);

        LDAPSearchResults ldapResults;
        try {
            ldapResults = LDAPUtils.search(ldapBase,
                    LDAPConnection.SCOPE_SUB, filter, attrs);
        } catch (LDAPException ex) {
            throw new DataAccessException("Failed to find idp.\n" + ex);
        }

        LDAPEntry entry;
        if (ldapResults.hasMore()) {
            try {
                entry = ldapResults.next();
            } catch (LDAPException ex) {
                throw new DataAccessException("Failed to find idp.\n" + ex);
            }

            samlProvider.setIssuer(issuer);
            samlProvider.setName(entry.getAttribute(LDAPUtils.COMMON_NAME)
                    .getStringValue());

            String expiry = DateUtil.toServerResponseFormat(
                    entry.getAttribute(LDAPUtils.EXPIRY).getStringValue());
            samlProvider.setExpiry(expiry);

            String samlMetadataJson = entry.getAttribute(
                    LDAPUtils.SAML_TOKENS_JSON).getStringValue();
            SAMLMetadataTokens samlMetadataTokens
                    = (SAMLMetadataTokens) JSONUtil.deserializeFromJson(
                            samlMetadataJson, SAMLMetadataTokens.class);

            samlProvider.setSAMLMetadataTokens(samlMetadataTokens);

            String accountID = entry.getAttribute(LDAPUtils.ACCOUNT_ID)
                    .getStringValue();
            samlProvider.setAccount(getAccount(accountID));
        }

        return samlProvider;
    }

    /**
     * Get the IDP descriptor from LDAP.
     *
     * @param account Account
     * @param providerName Identity Provider Name.
     * @return SAMLProvider
     * @throws com.seagates3.exception.DataAccessException
     */
    @Override
    public SAMLProvider find(Account account, String providerName)
            throws DataAccessException {
        SAMLProvider samlProvider = new SAMLProvider();
        samlProvider.setAccount(account);
        samlProvider.setName(providerName);

        String[] attrs = {LDAPUtils.ISSUER, LDAPUtils.EXPIRY,
            LDAPUtils.SAML_TOKENS_JSON};

        String ldapBase = String.format("%s=%s,%s",
                LDAPUtils.ORGANIZATIONAL_UNIT_NAME, LDAPUtils.IDP_OU,
                LDAPUtils.BASE_DN);

        String filter = String.format("(&(%s=%s)(%s=%s))", LDAPUtils.ACCOUNT_ID,
                account.getId(), LDAPUtils.COMMON_NAME, providerName);

        LDAPSearchResults ldapResults;
        try {
            ldapResults = LDAPUtils.search(ldapBase,
                    LDAPConnection.SCOPE_SUB, filter, attrs);
        } catch (LDAPException ex) {
            throw new DataAccessException("Failed to find idp.\n" + ex);
        }

        LDAPEntry entry;
        if (ldapResults.hasMore()) {
            try {
                entry = ldapResults.next();
            } catch (LDAPException ex) {
                throw new DataAccessException("Failed to find idp.\n" + ex);
            }
            samlProvider.setIssuer(entry.getAttribute(LDAPUtils.ISSUER)
                    .getStringValue());

            String expiry = DateUtil.toServerResponseFormat(
                    entry.getAttribute(LDAPUtils.EXPIRY).getStringValue());
            samlProvider.setExpiry(expiry);

            String samlMetadataJson = entry.getAttribute(
                    LDAPUtils.SAML_TOKENS_JSON).getStringValue();
            SAMLMetadataTokens samlMetadataTokens
                    = (SAMLMetadataTokens) JSONUtil.deserializeFromJson(
                            samlMetadataJson, SAMLMetadataTokens.class);

            samlProvider.setSAMLMetadataTokens(samlMetadataTokens);
        }

        return samlProvider;
    }

    /**
     * Get all the SAML Providers of an account.
     *
     * @param account
     * @return
     * @throws com.seagates3.exception.DataAccessException
     */
    @Override
    public SAMLProvider[] findAll(Account account) throws DataAccessException {
        ArrayList samlProviders = new ArrayList();
        SAMLProvider samlProvider;

        String[] attrs = {LDAPUtils.COMMON_NAME, LDAPUtils.EXPIRY,
            LDAPUtils.CREATE_TIMESTAMP};

        String ldapBase = String.format("%s=%s,%s",
                LDAPUtils.ORGANIZATIONAL_UNIT_NAME, LDAPUtils.IDP_OU,
                LDAPUtils.BASE_DN
        );

        String filter = String.format("(&(%s=%s)(%s=%s))",
                LDAPUtils.OBJECT_CLASS, LDAPUtils.SAML_PROVIDER_OBJECT_CLASS,
                LDAPUtils.ACCOUNT_ID, account.getId());

        LDAPSearchResults ldapResults;
        try {
            ldapResults = LDAPUtils.search(ldapBase,
                    LDAPConnection.SCOPE_SUB, filter, attrs);
        } catch (LDAPException ex) {
            throw new DataAccessException("Failed to find IDPs.\n" + ex);
        }

        while (ldapResults.hasMore()) {
            samlProvider = new SAMLProvider();
            LDAPEntry entry;
            try {
                entry = ldapResults.next();
            } catch (LDAPException ex) {
                throw new DataAccessException("Ldap failure.\n" + ex);
            }

            samlProvider.setAccount(account);
            samlProvider.setName(entry.getAttribute(LDAPUtils.COMMON_NAME)
                    .getStringValue());
            String createTime = DateUtil.toServerResponseFormat(
                    entry.getAttribute(LDAPUtils.CREATE_TIMESTAMP)
                    .getStringValue());
            samlProvider.setCreateDate(createTime);

            String expiry = DateUtil.toServerResponseFormat(
                    entry.getAttribute(LDAPUtils.EXPIRY).getStringValue());
            samlProvider.setExpiry(expiry);

            samlProviders.add(samlProvider);
        }

        SAMLProvider[] samlProviderList = new SAMLProvider[samlProviders.size()];
        return (SAMLProvider[]) samlProviders.toArray(samlProviderList);
    }

    /*
     * Return true if the key exists for the idp.
     */
    @Override
    public Boolean keyExists(String accountName, String name, Certificate cert)
            throws DataAccessException {
        try {
            String[] attrs = new String[]{"samlkeyuse"};

            String filter;
            try {
                filter = String.format("cacertificate;binary",
                        cert.getEncoded());
            } catch (CertificateEncodingException ex) {
                throw new DataAccessException("Failed to search "
                        + "the certificate." + ex);
            }

            LDAPSearchResults ldapResults;
            ldapResults = LDAPUtils.search(LDAPUtils.getBaseDN(),
                    LDAPConnection.SCOPE_SUB, filter, attrs);

            if (ldapResults.hasMore()) {
                return true;
            }
        } catch (LDAPException ex) {
            throw new DataAccessException("Failed to get the count of user access keys" + ex);
        }
        return false;
    }

    /**
     * Save the new identity provider.
     *
     * @param samlProvider SAMLProvider
     * @throws com.seagates3.exception.DataAccessException
     */
    @Override
    public void save(SAMLProvider samlProvider) throws DataAccessException {
        LDAPAttributeSet attributeSet = new LDAPAttributeSet();
        attributeSet.add(new LDAPAttribute(LDAPUtils.OBJECT_CLASS,
                LDAPUtils.SAML_PROVIDER_OBJECT_CLASS));
        attributeSet.add(new LDAPAttribute(LDAPUtils.ACCOUNT_ID,
                samlProvider.getAccount().getId()));
        attributeSet.add(new LDAPAttribute(LDAPUtils.COMMON_NAME,
                samlProvider.getName()));
        attributeSet.add(new LDAPAttribute(LDAPUtils.SAML_METADATA_XML,
                samlProvider.getSamlMetadata()));

        String expiry = DateUtil.toLdapDate(samlProvider.getExpiry());
        attributeSet.add(new LDAPAttribute(LDAPUtils.EXPIRY,
                expiry));
        attributeSet.add(new LDAPAttribute(LDAPUtils.ISSUER,
                samlProvider.getIssuer()));

        String samlTokensJson = JSONUtil.serializeToJson(
                samlProvider.getSAMLMetadataTokens());
        attributeSet.add(new LDAPAttribute(LDAPUtils.SAML_TOKENS_JSON,
                samlTokensJson));

        String dn = String.format("%s=%s,%s=%s,%s", LDAPUtils.ISSUER,
                samlProvider.getIssuer(), LDAPUtils.ORGANIZATIONAL_UNIT_NAME,
                LDAPUtils.IDP_OU,
                LDAPUtils.BASE_DN
        );

        try {
            LDAPUtils.add(new LDAPEntry(dn, attributeSet));
        } catch (LDAPException ex) {
            throw new DataAccessException("Failed to create new idp." + ex);
        }
    }

    /**
     * Delete a SAML provider.
     *
     * @param samlProvider SAML Provider.
     * @throws DataAccessException
     */
    @Override
    public void delete(SAMLProvider samlProvider) throws DataAccessException {
        String dn = String.format("%s=%s,%s=%s,%s", LDAPUtils.ISSUER,
                samlProvider.getIssuer(), LDAPUtils.ORGANIZATIONAL_UNIT_NAME,
                LDAPUtils.IDP_OU, LDAPUtils.BASE_DN
        );

        try {
            LDAPUtils.delete(dn);
        } catch (LDAPException ex) {
            throw new DataAccessException("Failed to delete the provider.\n" + ex);
        }
    }

    @Override
    public void update(SAMLProvider samlProvider, String newSamlMetadata)
            throws DataAccessException {
        LDAPAttribute attr;

        String dn = String.format("%s=%s,%s=%s,%s", LDAPUtils.ISSUER,
                samlProvider.getIssuer(), LDAPUtils.ORGANIZATIONAL_UNIT_NAME,
                LDAPUtils.IDP_OU, LDAPUtils.BASE_DN
        );

        attr = new LDAPAttribute(LDAPUtils.SAML_METADATA_XML, newSamlMetadata
        );

        try {
            LDAPUtils.modify(dn, new LDAPModification(
                    LDAPModification.REPLACE, attr));
        } catch (LDAPException ex) {
            throw new DataAccessException("Failed to modify the user details.\n" + ex);
        }
    }

    private Account getAccount(String accountId) throws DataAccessException {
        AccountDAO accountDAO = (AccountDAO) DAODispatcher.getResourceDAO(
                DAOResource.ACCOUNT);
        return accountDAO.findByID(accountId);
    }
}
