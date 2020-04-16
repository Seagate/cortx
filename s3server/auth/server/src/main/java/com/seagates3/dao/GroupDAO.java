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
 * Original creation date: 20-May-2016
 */
package com.seagates3.dao;

import com.seagates3.exception.DataAccessException;
import com.seagates3.model.Account;
import com.seagates3.model.Group;

public interface GroupDAO {

    /**
     * Find Policy.
     *
     * @param account
     * @param name
     * @return
     * @throws DataAccessException
     */
    public Group find(Account account, String groupName)
            throws DataAccessException;

    /**
     * Save the Group.
     *
     * @param group
     * @throws DataAccessException
     */
    public void save(Group group) throws DataAccessException;

     /**
      * Find group by path.
      *
      * @param path
      * @return
      * @throws DataAccessException
      */
    public
     Group findByPath(String path) throws DataAccessException;

     /**
      * Find group by path and account.
      *
      * @param account
      * @param path
      * @return
      * @throws DataAccessException
      */
    public
     Group findByPathAndAccount(Account account,
                                String path) throws DataAccessException;
}
