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
 * Original creation date: 22-Jan-2016
 */
package com.seagates3.service;

import java.util.Date;

import com.seagates3.dao.AccessKeyDAO;
import com.seagates3.dao.DAODispatcher;
import com.seagates3.dao.DAOResource;
import com.seagates3.exception.DataAccessException;
import com.seagates3.model.AccessKey;
import com.seagates3.model.User;
import com.seagates3.util.DateUtil;
import com.seagates3.util.KeyGenUtil;

public class AccessKeyService {

    public static AccessKey createFedAccessKey(User user, long timeToExpire)
            throws DataAccessException {
        AccessKeyDAO accessKeyDAO = (AccessKeyDAO) DAODispatcher.getResourceDAO(
                DAOResource.ACCESS_KEY);
        AccessKey accessKey = new AccessKey();
        accessKey.setUserId(user.getId());
        accessKey.setId(KeyGenUtil.createUserAccessKeyId());
        accessKey.setSecretKey(KeyGenUtil.generateSecretKey());
        accessKey.setToken(KeyGenUtil.generateSecretKey());
        accessKey.setStatus(AccessKey.AccessKeyStatus.ACTIVE);

        long currentTime = DateUtil.getCurrentTime();
        Date expiryDate = new Date(currentTime + (timeToExpire * 1000));
        accessKey.setExpiry(DateUtil.toServerResponseFormat(expiryDate));

        accessKeyDAO.save(accessKey);

        return accessKey;
    }
}

