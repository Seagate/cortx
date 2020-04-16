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
 * Original creation date: 19-May-2016
 */
package com.seagates3.model;

public class Policy {

    private int attachmentCount;
    private String description, path, name, policyDoc, policyId;
    private String arn, defaultVersionId, createDate, updateDate;
    private Account account;

    /**
     * Attachment count is the number of entities (users, groups, and roles)
     * that the policy is attached to.
     *
     * @return
     */
    public int getAttachmentCount() {
        return attachmentCount;
    }

    /**
     * Return the ARN of the policy.
     *
     * @return
     */
    public String getARN() {
        return arn;
    }

    /**
     * Set the account to which the policy belongs.
     *
     * @return
     */
    public Account getAccount() {
        return account;
    }

    /**
     * Return the create date.
     *
     * @return
     */
    public String getCreateDate() {
        return createDate;
    }

    /**
     * Return the default version id.
     *
     * @return
     */
    public String getDefaultVersionid() {
        return defaultVersionId;
    }

    /**
     * Return policy description.
     *
     * @return Description.
     */
    public String getDescription() {
        return description;
    }

    /**
     * Return the policy ID.
     *
     * @return
     */
    public String getPolicyId() {
        return policyId;
    }

    /**
     * Return Policy path.
     *
     * @return Path
     */
    public String getPath() {
        return path;
    }

    /**
     * Return policy name.
     *
     * @return
     */
    public String getName() {
        return name;
    }

    /**
     * Return Policy doc.
     *
     * @return
     */
    public String getPolicyDoc() {
        return policyDoc;
    }

    /**
     * Return the update date.
     *
     * @return
     */
    public String getUpdateDate() {
        return updateDate;
    }

    /**
     * Set the attachment count of the policy.
     *
     * @param count
     */
    public void setAttachmentCount(int count) {
        this.attachmentCount = count;
    }

    /**
     * Set the ARN of the policy.
     *
     * @param arn
     */
    public void setARN(String arn) {
        this.arn = arn;
    }

    /**
     * Set the account to which this policy belongs.
     *
     * @param account
     */
    public void setAccount(Account account) {
        this.account = account;
    }

    /**
     * Set the create date.
     *
     * @param createDate
     */
    public void setCreateDate(String createDate) {
        this.createDate = createDate;
    }

    /**
     * Set the default version Id of the policy.
     *
     * @param versionId
     */
    public void setDefaultVersionId(String versionId) {
        this.defaultVersionId = versionId;
    }

    /**
     * Set policy description.
     *
     * @param description
     */
    public void setDescription(String description) {
        this.description = description;
    }

    /**
     * Set policy path.
     *
     * @param path
     */
    public void setPath(String path) {
        this.path = path;
    }

    /**
     * Set policy ID.
     *
     * @param id
     */
    public void setPolicyId(String id) {
        this.policyId = id;
    }

    /**
     * Set policy name.
     *
     * @param name
     */
    public void setName(String name) {
        this.name = name;
    }

    /**
     * Set policy document.
     *
     * @param policyDoc
     */
    public void setPolicyDoc(String policyDoc) {
        this.policyDoc = policyDoc;
    }

    /**
     * Set the update date.
     *
     * @param updateDate
     */
    public void setUpdateDate(String updateDate) {
        this.updateDate = updateDate;
    }

    /**
     * Return true if policy exists.
     *
     * @return
     */
    public boolean exists() {
        return policyId != null;
    }
}
