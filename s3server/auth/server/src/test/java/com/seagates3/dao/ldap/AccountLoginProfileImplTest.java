package com.seagates3.dao.ldap;

import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.powermock.api.mockito.PowerMockito;
import org.powermock.core.classloader.annotations.PowerMockIgnore;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;

import com.novell.ldap.LDAPAttribute;
import com.novell.ldap.LDAPException;
import com.novell.ldap.LDAPModification;
import com.seagates3.exception.DataAccessException;
import com.seagates3.model.Account;
import com.seagates3.util.KeyGenUtil;

@RunWith(PowerMockRunner.class)
    @PrepareForTest({LDAPUtils.class, AccountLoginProfileImpl.class,
                     KeyGenUtil.class})
    @PowerMockIgnore(
        {"javax.management.*"}) public class AccountLoginProfileImplTest {

  final String ACCOUNT_NAME = "s3test";
 private
  final String ACCOUNT_ID = "12345";
 private
  final Account ACCOUNT;
 private
  final AccountLoginProfileImpl accountLoginProfileImpl;

 public
  AccountLoginProfileImplTest() {
    ACCOUNT = new Account();
    ACCOUNT.setId(ACCOUNT_ID);
    ACCOUNT.setName(ACCOUNT_NAME);
    accountLoginProfileImpl = new AccountLoginProfileImpl();
  }

  @Before public void setUp() throws Exception {
    PowerMockito.mockStatic(LDAPUtils.class);
    PowerMockito.mockStatic(KeyGenUtil.class);
  }

  @Test public void save_Success() throws LDAPException, DataAccessException {
    ACCOUNT.setId(ACCOUNT_ID);
    ACCOUNT.setName(ACCOUNT_NAME);
    ACCOUNT.setPwdResetRequired("false");
    ACCOUNT.setProfileCreateDate("2019-06-16 15:38:53+00:00");
    ACCOUNT.setPassword("password");
    accountLoginProfileImpl.save(ACCOUNT);

    PowerMockito.verifyStatic(Mockito.times(1));
    LDAPUtils.modify(Mockito.anyString(), Mockito.any(ArrayList.class));
    PowerMockito.verifyNoMoreInteractions(LDAPUtils.class);
  }

  @Test(expected = DataAccessException.class) public void save_LDAPException()
      throws Exception {
    ACCOUNT.setId(ACCOUNT_ID);
    ACCOUNT.setName(ACCOUNT_NAME);
    ACCOUNT.setPwdResetRequired("false");
    ACCOUNT.setProfileCreateDate("2019-06-16 15:38:53+00:00");
    ACCOUNT.setPassword("password");
    String dn = "o=s3test,ou=accounts,dc=s3,dc=seagate,dc=com";

    ArrayList modifyList = Mockito.mock(ArrayList.class);
    LDAPAttribute ldapAttribute = Mockito.mock(LDAPAttribute.class);
    LDAPModification modification = Mockito.mock(LDAPModification.class);

    PowerMockito.whenNew(ArrayList.class).withNoArguments().thenReturn(
        modifyList);

    PowerMockito.whenNew(LDAPAttribute.class).withAnyArguments().thenReturn(
        ldapAttribute);

    PowerMockito.whenNew(LDAPModification.class)
        .withParameterTypes(int.class, LDAPAttribute.class)
        .withArguments(LDAPModification.REPLACE, ldapAttribute)
        .thenReturn(modification);

    PowerMockito.doThrow(new LDAPException())
        .when(LDAPUtils.class, "modify", dn, modifyList);

    accountLoginProfileImpl.save(ACCOUNT);
  }

  @Test(expected = DataAccessException
                       .class) public void save_NoSuchAlgorithmException()
      throws Exception {
    ACCOUNT.setId(ACCOUNT_ID);
    ACCOUNT.setName(ACCOUNT_NAME);
    ACCOUNT.setPwdResetRequired("false");
    ACCOUNT.setProfileCreateDate("2019-06-16 15:38:53+00:00");
    ACCOUNT.setPassword("password");

    PowerMockito.doThrow(new NoSuchAlgorithmException())
        .when(KeyGenUtil.class, "generateSSHA", "password");
    accountLoginProfileImpl.save(ACCOUNT);
  }
}
