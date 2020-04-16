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

/* This Class Represents Xml Node As
 *
 *  <Owner>
 *    <ID>123er45678</ID>
 *    <DisplayName>S3test</DisplayName>
 *  </Owner>
 */

package com.seagates3.acl;

public
class Owner {

  String canonicalId;
  String displayName;

 public
  Owner(String canonicalId, String displayName) {
    this.canonicalId = canonicalId;
    this.displayName = displayName;
  }

  void setCanonicalId(String Id) { canonicalId = Id; }

  String getCanonicalId() { return canonicalId; }

  void setDisplayName(String Name) { displayName = Name; }

  String getDisplayName() { return displayName; }
}
