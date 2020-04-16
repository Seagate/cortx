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
 * Original creation date: 15-Oct-2015
 */
package com.seagates3.dao;

import com.seagates3.exception.DataAccessException;
import com.seagates3.model.Account;
import com.seagates3.model.SAMLProvider;
import java.security.cert.Certificate;

public interface SAMLProviderDAO {

    /**
     * Get the SAML provider from issuer name.
     *
     * @param issuer Issuer name.
     * @return SAMLProvider
     * @throws DataAccessException
     */
    public SAMLProvider find(String issuer) throws DataAccessException;

    /*
     * Get user details from the database.
     */
    public SAMLProvider find(Account account, String name) throws DataAccessException;

    /*
     * Get the list of all the saml providers.
     */
    public SAMLProvider[] findAll(Account account) throws DataAccessException;

    /*
     * Return true if the key exists for the idp.
     */
    public Boolean keyExists(String accountId, String name, Certificate cert)
            throws DataAccessException;

    /*
     * Create a new entry for the saml provider in the data base.
     */
    public void save(SAMLProvider samlProvider) throws DataAccessException;

    /*
     * Delete the saml provider.
     */
    public void delete(SAMLProvider samlProvider) throws DataAccessException;

    /*
     * Modify saml provider metadata.
     */
    public void update(SAMLProvider samlProvider, String newSamlMetadata)
            throws DataAccessException;
}
