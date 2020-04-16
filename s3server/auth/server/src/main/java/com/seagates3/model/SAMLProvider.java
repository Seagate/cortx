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
 * Original creation date: 14-Oct-2015
 */
package com.seagates3.model;

public class SAMLProvider {

    private Account account;
    private String name;
    private String samlMetadata;
    private String expiry;
    private String issuer;
    private String createDate;
    private SAMLMetadataTokens samlMetadataTokens;

    public Account getAccount() {
        return account;
    }

    public String getCreateDate() {
        return createDate;
    }

    public String getName() {
        return name;
    }

    public String getSamlMetadata() {
        return samlMetadata;
    }

    public String getExpiry() {
        return expiry;
    }

    public String getIssuer() {
        return issuer;
    }

    public SAMLMetadataTokens getSAMLMetadataTokens() {
        return samlMetadataTokens;
    }

    public void setAccount(Account account) {
        this.account = account;
    }

    public void setCreateDate(String createDate) {
        this.createDate = createDate;
    }

    public void setName(String name) {
        this.name = name;
    }

    public void setSamlMetadata(String samlMetadata) {
        this.samlMetadata = samlMetadata;
    }

    public void setExpiry(String expiry) {
        this.expiry = expiry;
    }

    public void setIssuer(String issuer) {
        this.issuer = issuer;
    }

    public void setSAMLMetadataTokens(SAMLMetadataTokens samlTokens) {
        this.samlMetadataTokens = samlTokens;
    }

    public Boolean exists() {
        return !(issuer == null);
    }
}
