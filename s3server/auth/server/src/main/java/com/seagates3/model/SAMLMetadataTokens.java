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
 * Original creation date: 12-Jan-2016
 */
package com.seagates3.model;

import java.util.ArrayList;
import java.util.Map;

public class SAMLMetadataTokens {

    /**
     * Attributes are key pair values in the following format.
     * <name, friendly name>
     */
    private Map<String, String> attributes;

    /**
     * samlKeyDescriptors is a hash map consisting of certificates used for
     * encryption and signing.
     *
     * <<encryption, certificates[]>, <signing, certificates[]>>
     */
    private Map<String, ArrayList<String>> samlKeyDescriptors;

    /**
     * Single sign out service is a key pair value in the following format.
     * <<POST, url>, <Redirect, url>>
     */
    private Map<String, String> singleSignOutService;

    /**
     * Single sign in service is a key pair value in the following format.
     * <<POST, url>, <Redirect, url>>
     */
    private Map<String, String> singleSignInService;

    private String[] nameIdFormats;

    public Map<String, ArrayList<String>> getSAMLKeyDescriptors() {
        return samlKeyDescriptors;
    }

    public Map<String, String> getSingleSignInService() {
        return singleSignInService;
    }

    public Map<String, String> getSingleSignOutService() {
        return singleSignOutService;
    }

    public String[] getNameIdFormats() {
        return nameIdFormats;
    }

    public Map<String, String> getAttributes() {
        return attributes;
    }

    public void setSAMLKeyDescriptors(
            Map<String, ArrayList<String>> keyDescriptors) {
        this.samlKeyDescriptors = keyDescriptors;
    }

    public void setSingleSignInService(Map<String, String> service) {
        this.singleSignInService = service;
    }

    public void setSingleSignOutService(Map<String, String> service) {
        this.singleSignOutService = service;
    }

    public void setNameIdFormats(String[] nameIdFormats) {
        this.nameIdFormats = nameIdFormats;
    }

    public void setAttributes(Map<String, String> attributes) {
        this.attributes = attributes;
    }
}
