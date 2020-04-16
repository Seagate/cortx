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
package com.seagates3.dao;

import com.seagates3.exception.DataAccessException;
import com.seagates3.model.User;

public interface UserDAO {

    /*
     * Get user details from the database using user name.
     */
    public User find(String accountName, String userName) throws DataAccessException;

    /*
     * Get user details from the database using user id.
     */
    public User findByUserId(String accountName, String userId) throws DataAccessException;

    /*
     * Get the details of all the users with the given path prefix from an account.
     */
    public User[] findAll(String accountName, String pathPrefix) throws DataAccessException;

    /*
     * Delete the user.
     */
    public void delete(User user) throws DataAccessException;

    /*
     * Create a new entry for the user in the data base.
     */
    public void save(User user) throws DataAccessException;

    /*
     * Modify user details.
     */
    public void update(User user, String newUserName, String newPath) throws DataAccessException;
    public
     User findByUserId(String userId) throws DataAccessException;
    public
     User findByArn(String arn) throws DataAccessException;
}
