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
import com.seagates3.model.AccessKey;
import com.seagates3.model.User;

public interface AccessKeyDAO {

    /*
     * Get the access key details from the database.
     */
    public AccessKey find(String accessKeyId) throws DataAccessException;

    public AccessKey findFromToken(String accessKeyId) throws DataAccessException;

    /*
     * Get all the access keys belonging to the user.
     */
    public AccessKey[] findAll(User user) throws DataAccessException;

    /*
     * Return true if the user has an access key.
     */
    public Boolean hasAccessKeys(String userId) throws DataAccessException;

    /*
     * Return the no of access keys which a user has.
     * AWS allows a maximum of 2 access keys per user.
     */
    public int getCount(String userId) throws DataAccessException;

    /*
     * Delete the access key.
     */
    public void delete(AccessKey accessKey) throws DataAccessException;

    /*
     * Create a new entry in the data base for the access key.
     */
    public void save(AccessKey accessKey) throws DataAccessException;

    /*
     * modify the access key details.
     */
    public void update(AccessKey accessKey, String newStatus) throws DataAccessException;
}
