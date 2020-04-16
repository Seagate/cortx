/*
 * COPYRIGHT 2016 SEAGATE LLC
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
 * Original creation date: 20-May-2016
 */
package com.seagates3.model;

public class Group {

    private String path, name, arn, createDate, groupId;
    private Account account;
    public
     static final String AllUsersURI =
         "http://acs.amazonaws.com/groups/global/AllUsers";
    public
     static final String AuthenticatedUsersURI =
         "http://acs.amazonaws.com/groups/global/AuthenticatedUsers";
    public
     static final String LogDeliveryURI =
         "http://acs.amazonaws.com/groups/s3/LogDelivery";

    /**
     * Return the account to which the group belongs.
     *
     * @return
     */
    public Account getAccount() {
        return account;
    }

    /**
     * Return the ARN of the group.
     *
     * @return
     */
    public String getARN() {
        return arn;
    }

    /**
     * Return the create date of the account.
     *
     * @return
     */
    public String getCreateDate() {
        return createDate;
    }

    /**
     * Return the group ID.
     *
     * @return
     */
    public String getGroupId() {
        return groupId;
    }

    /**
     * Return the name of the group
     *
     * @return
     */
    public String getName() {
        return name;
    }

    /**
     * Return the path of the group.
     *
     * @return
     */
    public String getPath() {
        return path;
    }

    /**
     * Set the account to which the group belongs.
     *
     * @param account
     */
    public void setAccount(Account account) {
        this.account = account;
    }

    /**
     * Set the ARN of the group
     *
     * @param arn
     */
    public void setARN(String arn) {
        this.arn = arn;
    }

    /**
     * Set the create date of the group.
     *
     * @param createDate
     */
    public void setCreateDate(String createDate) {
        this.createDate = createDate;
    }

    /**
     * Set the ID of the group.
     *
     * @param groupId
     */
    public void setGroupId(String groupId) {
        this.groupId = groupId;
    }

    /**
     * Set the name of the group.
     *
     * @param name
     */
    public void setName(String name) {
        this.name = name;
    }

    /**
     * Set the path of the group.
     *
     * @param path
     */
    public void setPath(String path) {
        this.path = path;
    }

    /**
     * Return true if group exists.
     *
     * @return
     */
    public boolean exists() {
        return groupId != null;
    }
}
