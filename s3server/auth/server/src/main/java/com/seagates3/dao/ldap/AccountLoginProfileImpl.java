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
 * Original author: Basavaraj Kirunge <basavaraj.kirunge@seagate.com>
 * Original creation date: 20-Jun-2019
 */
package com.seagates3.dao.ldap;

import com.novell.ldap.LDAPAttribute;
import com.novell.ldap.LDAPException;
import com.novell.ldap.LDAPModification;
import com.seagates3.dao.AccountLoginProfileDAO;
import com.seagates3.exception.DataAccessException;
import com.seagates3.model.Account;
import com.seagates3.util.KeyGenUtil;

import java.util.ArrayList;
import java.security.NoSuchAlgorithmException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
/**
 *
 * TODO : Remove this comment once below code tested end to end
 *
 */
public
class AccountLoginProfileImpl implements AccountLoginProfileDAO {

 private
  final Logger LOGGER =
      LoggerFactory.getLogger(UserLoginProfileImpl.class.getName());

 public
  void save(Account account) throws DataAccessException {

    ArrayList<LDAPModification> modList = new ArrayList<LDAPModification>();
    LDAPAttribute attr;
    String password = null;

    String dn =
        String.format("%s=%s,%s=accounts,%s", LDAPUtils.ORGANIZATIONAL_NAME,
                      account.getName(), LDAPUtils.ORGANIZATIONAL_UNIT_NAME,
                      LDAPUtils.BASE_DN);
    try {
      password = KeyGenUtil.generateSSHA(account.getPassword());
    }
    catch (NoSuchAlgorithmException ex) {
      throw new DataAccessException("Failed to modify the account details.\n" +
                                    ex);
    }
    if (password != null) {
      attr = new LDAPAttribute(LDAPUtils.PASSWORD, account.getPassword());
      modList.add(new LDAPModification(LDAPModification.REPLACE, attr));
    }

    LDAPAttribute attrDate = new LDAPAttribute(LDAPUtils.PROFILE_CREATE_DATE,
                                               account.getProfileCreateDate());
    modList.add(new LDAPModification(LDAPModification.REPLACE, attrDate));

    LDAPAttribute attrReset = new LDAPAttribute(
        LDAPUtils.PASSWORD_RESET_REQUIRED, account.getPwdResetRequired());
    modList.add(new LDAPModification(LDAPModification.REPLACE, attrReset));

    try {
      LOGGER.info("Modifying dn " + dn);
      LDAPUtils.modify(dn, modList);
    }
    catch (LDAPException ex) {
      LOGGER.error("Failed to modify the details of account: " +
                   account.getName());
      throw new DataAccessException("Failed to modify the account" +
                                    " details.\n" + ex);
    }
  }
}
