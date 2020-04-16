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
 * Original creation date: 17-Sep-2015
 */
package com.seagates3.model;

public class AccessKey {

    public enum AccessKeyStatus {

        ACTIVE("Active"),
        INACTIVE("Inactive");

        private final String status;

        private AccessKeyStatus(final String text) {
            this.status = text;
        }

        @Override
        public String toString() {
            return status;
        }
    }

    private String userId;
    private String id;
    private String secretKey;
    private String createDate;
    private String token;
    private String expiry;
    private AccessKeyStatus status;

    public String getUserId() {
        return userId;
    }

    public String getId() {
        return id;
    }

    public String getSecretKey() {
        return secretKey;
    }

    public String getStatus() {
        return status.toString();
    }

    public String getCreateDate() {
        return createDate;
    }

    public String getToken() {
        return token;
    }

    public String getExpiry() {
        return expiry;
    }

    public void setUserId(String userId) {
        this.userId = userId;
    }

    public void setId(String accessKeyId) {
        this.id = accessKeyId;
    }

    public void setSecretKey(String secretKey) {
        this.secretKey = secretKey;
    }

    public void setStatus(AccessKeyStatus status) {
        this.status = status;
    }

    public void setCreateDate(String createDate) {
        this.createDate = createDate;
    }

    public void setToken(String token) {
        this.token = token;
    }

    public void setExpiry(String expiry) {
        this.expiry = expiry;
    }

    public Boolean isAccessKeyActive() {
        return status == AccessKeyStatus.ACTIVE;
    }

    public Boolean exists() {
        return userId != null;
    }
}
