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

package com.seagates3.model;

public class FedUser {
    private String name;
    private String accountName;
    private String id;
    private String createDate;

    public String getName() {
        return name;
    }

    public String getAccountName() {
        return accountName;
    }

    public String getId() {
        return id;
    }

    public String getCreateDate() {
        return createDate;
    }

    public void setName(String name) {
        this.name = name;
    }

    public void setAccountName(String accountName) {
        this.accountName = accountName;
    }

    public void setId(String id) {
        this.id = id;
    }

    public void setCreateDate(String createDate) {
        this.createDate = createDate;
    }

    public Boolean exists() {
        return id != null;
    }
}
