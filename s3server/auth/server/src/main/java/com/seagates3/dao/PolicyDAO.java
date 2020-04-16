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
 * Original creation date: 19-May-2016
 */
package com.seagates3.dao;

import com.seagates3.exception.DataAccessException;
import com.seagates3.model.Account;
import com.seagates3.model.Policy;

public interface PolicyDAO {

    /**
     * Find Policy.
     *
     * @param account
     * @param name
     * @return
     * @throws DataAccessException
     */
    public Policy find(Account account, String name)
            throws DataAccessException;

    /**
     * Save the policy.
     *
     * @param policy
     * @throws DataAccessException
     */
    public void save(Policy policy) throws DataAccessException;
}
