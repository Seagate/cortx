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
 * Original creation date: 31-Oct-2015
 */
package com.seagates3.dao;

import com.seagates3.exception.DataAccessException;
import com.seagates3.model.Account;
import com.seagates3.model.Role;

public interface RoleDAO {

    /**
     * Get role from the database.
     *
     * @param account
     * @param roleName
     * @return
     * @throws com.seagates3.exception.DataAccessException
     */
    public Role find(Account account, String roleName)
            throws DataAccessException;

    /**
     * Get the details of all the roles with the given path prefix from an
     * account.
     *
     * @param account
     * @param pathPrefix
     * @return
     * @throws com.seagates3.exception.DataAccessException
     */
    public Role[] findAll(Account account, String pathPrefix)
            throws DataAccessException;

    /*
     * Delete the role.
     */
    public void delete(Role role) throws DataAccessException;

    /*
     * Create a new role.
     */
    public void save(Role role) throws DataAccessException;

}
