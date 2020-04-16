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

public class Account {

    private String id;
    private String name;
    private String canonicalId;
    private String email;
    private
     String password;
    private
     String profileCreateDate;
    private
     String pwdResetRequired;

    public String getId() {
        return id;
    }

    public String getName() {
        return name;
    }

    public String getCanonicalId() {
        return canonicalId;
    }

    public String getEmail() {
        return email;
    }

    public void setId(String id) {
        this.id = id;
    }

    public void setName(String name) {
        this.name = name;
    }

    public void setCanonicalId(String canonicalId) {
        this.canonicalId = canonicalId;
    }

    public void setEmail(String email) {
        this.email = email;
    }

    public Boolean exists() {
        return id != null;
    }

    public
     String getPassword() { return password; }

    public
     void setPassword(String pwd) { password = pwd; }

    public
     void setProfileCreateDate(String createDate) {
       profileCreateDate = createDate;
     }

    public
     String getProfileCreateDate() { return profileCreateDate; }

    public
     void setPwdResetRequired(String pwdReset) { pwdResetRequired = pwdReset; }

    public
     String getPwdResetRequired() { return pwdResetRequired; }
}
