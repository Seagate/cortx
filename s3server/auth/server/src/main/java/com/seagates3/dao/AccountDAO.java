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
package com.seagates3.dao;

import com.seagates3.exception.DataAccessException;
import com.seagates3.model.Account;

public interface AccountDAO {

    /**
     * TODO - Replace this method with an overloaded find method taking integer
     * account id as a parameter.
     *
     * @param accountID Account ID.
     * @return Account
     * @throws DataAccessException
     */
    public Account findByID(String accountID) throws DataAccessException;

    /* @param canonicalId of Account.
    *  @return Account
    *  @throws DataAccessException
    */
   public Account findByCanonicalID(String canonicalID) throws DataAccessException;
    /*
     * Get account details from the database.
     */
    public Account find(String name) throws DataAccessException;

    /*
     * Add a new entry for the account in the database.
     */
    public void save(Account account) throws DataAccessException;

    /*
     * Fetch all accounts from the database
     */
    public Account[] findAll() throws DataAccessException;

    /*
     * Delete account
     */
    public void delete(Account account) throws DataAccessException;

    /*
     * Delete ou under account
     */
    public void deleteOu(Account account, String ou) throws DataAccessException;

    public
     Account findByEmailAddress(String emailAddress) throws DataAccessException;
}
