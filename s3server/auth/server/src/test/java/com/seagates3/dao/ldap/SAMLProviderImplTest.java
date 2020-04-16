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
 * Original creation date: 11-Jan-2016
 */
package com.seagates3.dao.ldap;

import com.novell.ldap.LDAPAttribute;
import com.novell.ldap.LDAPAttributeSet;
import com.novell.ldap.LDAPEntry;
import com.novell.ldap.LDAPException;
import com.novell.ldap.LDAPModification;
import com.novell.ldap.LDAPSearchResults;
import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.exception.DataAccessException;
import com.seagates3.model.Account;
import com.seagates3.model.SAMLMetadataTokens;
import com.seagates3.model.SAMLProvider;
import com.seagates3.util.JSONUtil;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;
import org.mockito.Matchers;
import org.mockito.Mockito;
import org.mockito.internal.matchers.apachecommons.ReflectionEquals;
import org.powermock.api.mockito.PowerMockito;
import org.powermock.api.mockito.mockpolicies.Slf4jMockPolicy;
import org.powermock.core.classloader.annotations.MockPolicy;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;

@RunWith(PowerMockRunner.class)
@PrepareForTest({LDAPUtils.class, SAMLProviderImpl.class,
        JSONUtil.class, AuthServerConfig.class})
@MockPolicy(Slf4jMockPolicy.class)
public class SAMLProviderImplTest {

    private final String ACCOUNT_ID = "12345";
    private final String ACCOUNT_NAME = "s3test";
    private final String PROVIDER_NAME = "s3testprovider";
    private final String ISSUER_NAME = "http://s3test/issuer";
    private final String SAML_METADATA = "SAMLPLE_XML";
    private final String SAML_METADATA_TOKENS_JSON = "{}";
    private final String UPDATE_SAML_METADATA = "UPDATE_SAML_XML";
    private final String LDAP_EXP_DATE;
    private final String EXPECTED_EXP_DATE;
    private final String LDAP_CREATE_DATE;
    private final String EXPECTED_CREATE_DATE;
    private final String BASE_DN = "ou=idp,dc=s3,dc=seagate,dc=com";
    private final String[] FIND_ATTRS = {"issuer", "exp", "samltokensjson"};

    private final String[] FIND_ALL_ATTRS = {"cn", "exp", "createtimestamp"};

    private final Account ACCOUNT;

    private final SAMLProviderImpl samlProviderImpl;
    private final LDAPSearchResults ldapResults;
    private final LDAPEntry entry;
    private final LDAPAttribute issuerAttr;
    private final LDAPAttribute expAttr;
    private final LDAPAttribute cnAttr;
    private final LDAPAttribute samlTokenJsonAttr;
    private final LDAPAttribute createTimestampAttr;

    @Rule
    public final ExpectedException exception = ExpectedException.none();

    public SAMLProviderImplTest() {
        samlProviderImpl = new SAMLProviderImpl();
        ldapResults = Mockito.mock(LDAPSearchResults.class);
        entry = Mockito.mock(LDAPEntry.class);
        issuerAttr = Mockito.mock(LDAPAttribute.class);
        expAttr = Mockito.mock(LDAPAttribute.class);
        cnAttr = Mockito.mock(LDAPAttribute.class);
        createTimestampAttr = Mockito.mock(LDAPAttribute.class);
        samlTokenJsonAttr = Mockito.mock(LDAPAttribute.class);

        LDAP_EXP_DATE = "20160129160752Z";
        EXPECTED_EXP_DATE = "2016-01-29T16:07:52.000+0000";

        LDAP_CREATE_DATE = "20160119160752Z";
        EXPECTED_CREATE_DATE = "2016-01-19T16:07:52.000+0000";

        ACCOUNT = new Account();
        ACCOUNT.setId("12345");
    }

    private void setupSAMLProviderAttr() throws Exception {
        PowerMockito.mockStatic(LDAPUtils.class);
        Mockito.when(ldapResults.next()).thenReturn(entry);

        Mockito.when(entry.getAttribute("issuer")).thenReturn(issuerAttr);
        Mockito.when(issuerAttr.getStringValue()).thenReturn(ISSUER_NAME);

        Mockito.when(entry.getAttribute("exp")).thenReturn(expAttr);
        Mockito.when(expAttr.getStringValue()).thenReturn(LDAP_EXP_DATE);

        Mockito.when(entry.getAttribute("cn")).thenReturn(cnAttr);
        Mockito.when(cnAttr.getStringValue()).thenReturn(PROVIDER_NAME);

        Mockito.when(entry.getAttribute("samltokensjson")).thenReturn(
                samlTokenJsonAttr);
        Mockito.when(samlTokenJsonAttr.getStringValue())
                .thenReturn(SAML_METADATA_TOKENS_JSON);

        Mockito.when(entry.getAttribute("createtimestamp"))
                .thenReturn(createTimestampAttr);
        Mockito.when(createTimestampAttr.getStringValue())
                .thenReturn(LDAP_CREATE_DATE);

    }

    @Before
    public void setUp() throws Exception {
        PowerMockito.mockStatic(LDAPUtils.class);
        PowerMockito.mockStatic(JSONUtil.class);
        PowerMockito.mockStatic(AuthServerConfig.class);
    }

    @Test
    public void Find_SAMLProviderSearchFailed_ThrowException()
            throws Exception {
        String filter = "(&(accountid=12345)(cn=s3testprovider))";

        PowerMockito.doThrow(new LDAPException()).when(LDAPUtils.class, "search",
                BASE_DN, 2, filter, FIND_ATTRS
        );

        exception.expect(DataAccessException.class);
        samlProviderImpl.find(ACCOUNT, PROVIDER_NAME);
    }

    @Test
    public void Find_SAMLProviderNotFound_ReturnEmptyProvider()
            throws Exception {
        SAMLProvider expectedSAMLProvider = new SAMLProvider();
        expectedSAMLProvider.setAccount(ACCOUNT);
        expectedSAMLProvider.setName(PROVIDER_NAME);

        String filter = "(&(accountid=12345)(cn=s3testprovider))";

        PowerMockito.doReturn(ldapResults).when(LDAPUtils.class, "search",
                BASE_DN, 2, filter, FIND_ATTRS
        );
        Mockito.when(ldapResults.hasMore()).thenReturn(Boolean.FALSE);

        SAMLProvider samlProvider = samlProviderImpl.find(ACCOUNT,
                PROVIDER_NAME);
        Assert.assertThat(expectedSAMLProvider, new ReflectionEquals(
                samlProvider));
    }

    @Test
    public void Find_SAMLProviderFound_ReturnProvider()
            throws Exception {
        setupSAMLProviderAttr();

        SAMLMetadataTokens samlMetadataTokens = Mockito.mock(
                SAMLMetadataTokens.class);

        SAMLProvider expectedSAMLProvider = new SAMLProvider();
        expectedSAMLProvider.setAccount(ACCOUNT);
        expectedSAMLProvider.setName(PROVIDER_NAME);
        expectedSAMLProvider.setExpiry(EXPECTED_EXP_DATE);
        expectedSAMLProvider.setIssuer(ISSUER_NAME);
        expectedSAMLProvider.setSAMLMetadataTokens(samlMetadataTokens);

        String filter = "(&(accountid=12345)(cn=s3testprovider))";

        PowerMockito.doReturn(ldapResults).when(LDAPUtils.class, "search",
                BASE_DN, 2, filter, FIND_ATTRS
        );
        Mockito.when(ldapResults.hasMore())
                .thenReturn(Boolean.TRUE)
                .thenReturn(Boolean.FALSE);
        Mockito.when(ldapResults.next()).thenReturn(entry);
        PowerMockito.doReturn(samlMetadataTokens).when(JSONUtil.class,
                "deserializeFromJson", "{}", SAMLMetadataTokens.class);

        SAMLProvider samlProvider = samlProviderImpl.find(ACCOUNT,
                PROVIDER_NAME);
        Assert.assertThat(expectedSAMLProvider, new ReflectionEquals(
                samlProvider));
    }

    @Test
    public void Save_ProviderSaveFailed_ThrowException() throws Exception {
        SAMLMetadataTokens samlMetadataTokens = Mockito.mock(
                SAMLMetadataTokens.class);

        SAMLProvider samlProvider = new SAMLProvider();
        samlProvider.setAccount(ACCOUNT);
        samlProvider.setName(PROVIDER_NAME);
        samlProvider.setSamlMetadata(SAML_METADATA);
        samlProvider.setExpiry(EXPECTED_EXP_DATE);
        samlProvider.setIssuer(ISSUER_NAME);
        samlProvider.setSAMLMetadataTokens(samlMetadataTokens);

        PowerMockito.doReturn(SAML_METADATA_TOKENS_JSON).when(JSONUtil.class,
                "serializeToJson", samlMetadataTokens);

        PowerMockito.doThrow(new LDAPException()).when(LDAPUtils.class, "add",
                Matchers.any(LDAPEntry.class)
        );

        exception.expect(DataAccessException.class);

        samlProviderImpl.save(samlProvider);
    }

    @Test
    public void Save_SAMLProviderSaved() throws Exception {
        SAMLProvider samlProvider = new SAMLProvider();
        samlProvider.setName(PROVIDER_NAME);
        samlProvider.setSamlMetadata(SAML_METADATA);
        samlProvider.setExpiry(EXPECTED_EXP_DATE);
        samlProvider.setIssuer(ISSUER_NAME);
        samlProvider.setAccount(ACCOUNT);

        String dn = "issuer=http://s3test/issuer,ou=idp,dc=s3,dc=seagate,dc=com";

        LDAPAttributeSet ldapAttributeSet = Mockito.mock(LDAPAttributeSet.class);
        LDAPAttribute ldapAttribute = Mockito.mock(LDAPAttribute.class);
        LDAPEntry accessKeyEntry = Mockito.mock(LDAPEntry.class);

        PowerMockito.whenNew(LDAPAttributeSet.class)
                .withNoArguments()
                .thenReturn(ldapAttributeSet);

        PowerMockito.whenNew(LDAPAttribute.class)
                .withAnyArguments()
                .thenReturn(ldapAttribute);

        Mockito.doReturn(true).when(ldapAttributeSet).add(ldapAttribute);
        PowerMockito.whenNew(LDAPEntry.class)
                .withParameterTypes(String.class, LDAPAttributeSet.class)
                .withArguments(dn, ldapAttributeSet)
                .thenReturn(accessKeyEntry);

        samlProviderImpl.save(samlProvider);

        PowerMockito.verifyNew(LDAPAttribute.class)
                .withArguments("objectclass", "samlprovider");
        PowerMockito.verifyNew(LDAPAttribute.class)
                .withArguments("cn", "s3testprovider");
        PowerMockito.verifyNew(LDAPAttribute.class)
                .withArguments("samlmetadataxml", "SAMLPLE_XML");
        PowerMockito.verifyNew(LDAPAttribute.class)
                .withArguments("exp", LDAP_EXP_DATE);
        PowerMockito.verifyNew(LDAPAttribute.class)
                .withArguments("issuer", "http://s3test/issuer");

        PowerMockito.verifyStatic(Mockito.times(1));
        LDAPUtils.add(accessKeyEntry);
    }

    @Test
    public void FindAll_LdapSearchFailed_ThrowException() throws Exception {
        String ldapBase = "ou=idp,dc=s3,dc=seagate,dc=com";

        String filter = "(&(objectclass=samlprovider)(accountid=12345))";

        PowerMockito.doThrow(new LDAPException()).when(LDAPUtils.class, "search",
                ldapBase, 2, filter, FIND_ALL_ATTRS
        );
        exception.expect(DataAccessException.class);

        samlProviderImpl.findAll(ACCOUNT);
    }

    @Test
    public void FindAll_ProviderDoesnotExist_ReturnEmptyProviderList()
            throws Exception {
        SAMLProvider[] expectedSamlProviderList = new SAMLProvider[0];

        String ldapBase = "ou=idp,dc=s3,dc=seagate,dc=com";

        String filter = "(&(objectclass=samlprovider)(accountid=12345))";

        PowerMockito.doReturn(ldapResults).when(LDAPUtils.class, "search",
                ldapBase, 2, filter, FIND_ALL_ATTRS
        );
        Mockito.when(ldapResults.hasMore()).thenReturn(Boolean.FALSE);

        SAMLProvider[] samlProviderList = samlProviderImpl.findAll(ACCOUNT);
        Assert.assertArrayEquals(expectedSamlProviderList, samlProviderList);
    }

    @Test
    public void FindAll_ExceptionOccuredWhileIterating_ThrowException()
            throws Exception {
        String ldapBase = "ou=idp,dc=s3,dc=seagate,dc=com";

        String filter = "(&(objectclass=samlprovider)(accountid=12345))";

        PowerMockito.doReturn(ldapResults).when(LDAPUtils.class, "search",
                ldapBase, 2, filter, FIND_ALL_ATTRS
        );
        Mockito.when(ldapResults.hasMore()).thenReturn(Boolean.TRUE);
        Mockito.doThrow(new LDAPException()).when(ldapResults).next();

        exception.expect(DataAccessException.class);
        SAMLProvider[] samlProviderList = samlProviderImpl.findAll(ACCOUNT);
    }

    @Test
    public void FindAll_ProvidersFound_ReturnProviderList()
            throws Exception {
        SAMLProvider expectedSAMLProvider = new SAMLProvider();
        expectedSAMLProvider.setAccount(ACCOUNT);
        expectedSAMLProvider.setName(PROVIDER_NAME);
        expectedSAMLProvider.setCreateDate(EXPECTED_CREATE_DATE);
        expectedSAMLProvider.setExpiry(EXPECTED_EXP_DATE);

        setupSAMLProviderAttr();

        String ldapBase = "ou=idp,dc=s3,dc=seagate,dc=com";

        String filter = "(&(objectclass=samlprovider)(accountid=12345))";

        PowerMockito.doReturn(ldapResults).when(LDAPUtils.class, "search",
                ldapBase, 2, filter, FIND_ALL_ATTRS
        );
        Mockito.when(ldapResults.hasMore())
                .thenReturn(Boolean.TRUE)
                .thenReturn(Boolean.FALSE);

        SAMLProvider[] samlProviderList = samlProviderImpl.findAll(ACCOUNT);
        Assert.assertEquals(1, samlProviderList.length);
        Assert.assertThat(expectedSAMLProvider,
                new ReflectionEquals(samlProviderList[0]));
    }

    @Test
    public void Delete_LdapDeleteFailed_ThrowException() throws Exception {
        SAMLProvider samlProvider = new SAMLProvider();
        samlProvider.setName(PROVIDER_NAME);
        samlProvider.setSamlMetadata(SAML_METADATA);
        samlProvider.setExpiry(EXPECTED_EXP_DATE);
        samlProvider.setIssuer(ISSUER_NAME);

        String dn = "issuer=http://s3test/issuer,ou=idp,dc=s3,dc=seagate,dc=com";
        PowerMockito.doThrow(new LDAPException()).when(LDAPUtils.class, "delete",
                dn
        );
        exception.expect(DataAccessException.class);

        samlProviderImpl.delete(samlProvider);
    }

    @Test
    public void Delete_Success() throws Exception {
        SAMLProvider samlProvider = new SAMLProvider();
        samlProvider.setName(PROVIDER_NAME);
        samlProvider.setSamlMetadata(SAML_METADATA);
        samlProvider.setExpiry(EXPECTED_EXP_DATE);
        samlProvider.setIssuer(ISSUER_NAME);

        String dn = "issuer=http://s3test/issuer,ou=idp,dc=s3,dc=seagate,dc=com";

        samlProviderImpl.delete(samlProvider);
        PowerMockito.verifyStatic(Mockito.times(1));
        LDAPUtils.delete(dn);
    }

    @Test
    public void Update_ModifyFailed_ThrowException() throws Exception {
        SAMLProvider samlProvider = new SAMLProvider();
        samlProvider.setName(PROVIDER_NAME);
        samlProvider.setSamlMetadata(SAML_METADATA);
        samlProvider.setExpiry(EXPECTED_EXP_DATE);
        samlProvider.setIssuer(ISSUER_NAME);

        String dn = "issuer=http://s3test/issuer,ou=idp,dc=s3,dc=seagate,dc=com";
        LDAPAttribute ldapAttribute = Mockito.mock(LDAPAttribute.class);
        LDAPModification modification = Mockito.mock(LDAPModification.class);

        PowerMockito.whenNew(LDAPAttribute.class)
                .withAnyArguments()
                .thenReturn(ldapAttribute);

        PowerMockito.whenNew(LDAPModification.class)
                .withParameterTypes(int.class, LDAPAttribute.class)
                .withArguments(LDAPModification.REPLACE, ldapAttribute)
                .thenReturn(modification);

        PowerMockito.doThrow(new LDAPException()).when(LDAPUtils.class, "modify",
                dn, modification
        );

        exception.expect(DataAccessException.class);

        samlProviderImpl.update(samlProvider, UPDATE_SAML_METADATA);
    }

    @Test
    public void Update_ProviderUpdateSuccess() throws Exception {
        SAMLProvider samlProvider = new SAMLProvider();
        samlProvider.setName(PROVIDER_NAME);
        samlProvider.setSamlMetadata(SAML_METADATA);
        samlProvider.setExpiry(EXPECTED_EXP_DATE);
        samlProvider.setIssuer(ISSUER_NAME);

        String dn = "issuer=http://s3test/issuer,ou=idp,dc=s3,dc=seagate,dc=com";
        LDAPAttribute ldapAttribute = Mockito.mock(LDAPAttribute.class);
        LDAPModification modification = Mockito.mock(LDAPModification.class);

        PowerMockito.whenNew(LDAPAttribute.class)
                .withAnyArguments()
                .thenReturn(ldapAttribute);

        PowerMockito.whenNew(LDAPModification.class)
                .withParameterTypes(int.class, LDAPAttribute.class)
                .withArguments(LDAPModification.REPLACE, ldapAttribute)
                .thenReturn(modification);

        PowerMockito.doNothing().when(LDAPUtils.class, "modify",
                dn, modification
        );

        samlProviderImpl.update(samlProvider, UPDATE_SAML_METADATA);
        PowerMockito.verifyNew(LDAPAttribute.class)
                .withArguments("samlmetadataxml", UPDATE_SAML_METADATA);
    }
}
