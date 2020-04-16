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

public class User {

    public enum UserType {

        IAM_USER("iamUser"),
        IAM_FED_USER("iamFedUser"),
        ROLE_USER("roleUser");

        private final String userType;

        private UserType(final String userType) {
            this.userType = userType;
        }

        @Override
        public String toString() {
            return userType;
        }
    }

    private String name;
    private String accountName;
    private String path;
    private String id;
    private String createDate;
    private String passwordLastUsed;
    private UserType userType;
    private
     String password;
    private
     String profileCreateDate;
    private
     String pwdResetRequired = "false";
    private
     String arn;

    /**
     * TODO - Remove RoleName. User Type is sufficient to identify a role user
     * from federated user or IAM user.
     */
    private String roleName;

    public String getName() {
        return name;
    }

    public String getAccountName() {
        return accountName;
    }

    public String getPath() {
        return path;
    }

    public
     String getPassword() { return password; }

    public
     void setPassword(String password) { this.password = password; }

    public String getId() {
        return id;
    }

    public String getCreateDate() {
        return createDate;
    }

    public String getPasswordLastUsed() {
        return passwordLastUsed;
    }

    public UserType getUserType() {
        return userType;
    }

    public String getRoleName() {
        return roleName;
    }

    public
     String getProfileCreateDate() { return profileCreateDate; }

    public
     String getPwdResetRequired() { return pwdResetRequired; }

    public void setName(String name) {
        this.name = name;
    }

    public void setAccountName(String accountName) {
        this.accountName = accountName;
    }

    public void setPath(String path) {
        this.path = path;
    }

    public void setId(String id) {
        this.id = id;
    }

    public void setUserType(UserType userType) {
        this.userType = userType;
    }

    public void setUserType(String userClass) {
        this.userType = getUserTypeConstant(userClass);
    }

    public void setCreateDate(String createDate) {
        this.createDate = createDate;
    }

    public void setPasswordLastUsed(String passwordLastUsed) {
        this.passwordLastUsed = passwordLastUsed;
    }

    public void setRoleName(String roleName) {
        this.roleName = roleName;
    }

    public Boolean exists() {
        return id != null;
    }

    private UserType getUserTypeConstant(String userClass) {
        if (userClass.compareToIgnoreCase("iamuser") == 0) {
            return UserType.IAM_USER;
        }

        if (userClass.compareToIgnoreCase("iamfeduser") == 0) {
            return UserType.IAM_FED_USER;
        }

        return UserType.ROLE_USER;
    }

   public
    void setProfileCreateDate(String pCreateDate) {
      profileCreateDate = pCreateDate;
    }

   public
    void setPwdResetRequired(String pwdReset) { pwdResetRequired = pwdReset; }

   public
    String getArn() { return arn; }

   public
    void setArn(String arn) { this.arn = arn; }
}
