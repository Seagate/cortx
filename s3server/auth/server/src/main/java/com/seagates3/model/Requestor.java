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

public class Requestor {

    private String id, name;
    private Account account;
    private AccessKey accessKey;

    /*
     * Return the requestor id.
     */
    public String getId() {
        return id;
    }

    /*
     * Name of the requestor.
     */
    public String getName() {
        return name;
    }

    public AccessKey getAccesskey() {
        return accessKey;
    }

    public Account getAccount() {
        return account;
    }

    public void setAccessKey(AccessKey accessKey) {
        this.accessKey = accessKey;
    }

    public void setAccount(Account account) {
        this.account = account;
    }

    public void setId(String id) {
        this.id = id;
    }

    public void setName(String name) {
        this.name = name;
    }

    /*
     * Return if the requestor exists.
     */
    public Boolean exists() {
        return id != null;
    }

    /*
     * Return true if the user is a federated user.
     */
    public Boolean isFederatedUser() {
        return accessKey.getToken() != null;
    }
}
