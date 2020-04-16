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
 * Original author:  Abhilekh Mustapure <abhilekh.mustapure@seagate.com>
 * Original creation date: 04-Apr-2019
 */

/*
 * This Class Represents Xml Node As
 *  <Grant>
 *    <Grantee xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
 *         xsi:type="CanonicalUser">
 *      <ID>Int</ID>
 *      <DisplayName>String</DisplayName>
 *    </Grantee>
 *    <Permission>String</Permission>
 *  </Grant>
 */

package com.seagates3.acl;
public
class Grant {

  String permission;
  Grantee grantee;

 public
  Grant(Grantee grantee, String permission) {
    this.grantee = grantee;
    this.permission = permission;
  }

  void setGrantee(Grantee gran_tee) { grantee = gran_tee; }

  Grantee getGrantee() { return grantee; }

  void setPermission(String permi_ssion) { permission = permi_ssion; }

  String getPermission() { return permission; }
}
