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
 * Original author:  Ajinkya Dhumal
 * Original creation date: 10-November-2019
 */

package com.seagates3.policy;

public
class PrincipalArnParser extends ArnParser {

 public
  PrincipalArnParser() {
    /**
     *  ARN format - arn:aws:iam::<accountID>:<user>
     *  e.g. arn:aws:iam::KO87b1p0TKWa184S6xrINQ:user/u1
     *       arn:aws:iam::KO87b1p0TKWa184S6xrINQ:root
     *       arn:aws:iam::123456789012:user/user-name
     *       arn:aws:iam::123456789012:role/role-name
     */
    this.regEx = "arn:aws:iam::[a-zA-Z0-9~@#$^*\\\\/_.:-]+" +
                 ":[a-zA-Z0-9~@#$^*\\\\/_.:-]+";
  }
}