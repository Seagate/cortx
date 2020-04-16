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

/*
 * TODO
 * Rename the enum name
 */
public
 enum DAOResource {
   ACCESS_KEY("AccessKey"),
   ACCOUNT("Account"),
   FED_USER("FedUser"),
   GROUP("Group"),
   POLICY("Policy"),
   REQUESTEE("Requestee"),
   REQUESTOR("Requestor"),
   ROLE("Role"),
   SAML_PROVIDER("SAMLProvider"),
   USER("User"),
   USER_LOGIN_PROFILE("UserLoginProfile"),
   ACCOUNT_LOGIN_PROFILE("AccountLoginProfile");
   private final String className;
   private DAOResource(final String className) {this.className = className;}

    @Override
    public String toString() {
        return className;
    }
}
