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
package com.seagates3.controller;

import com.seagates3.dao.DAODispatcher;
import com.seagates3.dao.DAOResource;
import com.seagates3.dao.GroupDAO;
import com.seagates3.exception.DataAccessException;
import com.seagates3.model.Group;
import com.seagates3.model.Requestor;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.generator.GroupResponseGenerator;
import com.seagates3.util.ARNUtil;
import com.seagates3.util.DateUtil;
import com.seagates3.util.KeyGenUtil;
import java.util.Map;
import org.joda.time.DateTime;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class GroupController extends AbstractController {

    GroupDAO groupDAO;
    GroupResponseGenerator responseGenerator;
    private final Logger LOGGER =
            LoggerFactory.getLogger(GroupController.class.getName());

    public GroupController(Requestor requestor,
            Map<String, String> requestBody) {
        super(requestor, requestBody);

        groupDAO = (GroupDAO) DAODispatcher.getResourceDAO(DAOResource.GROUP);
        responseGenerator = new GroupResponseGenerator();
    }

    /**
     * Create new role.
     *
     * @return ServerReponse
     */
    @Override
    public ServerResponse create() {
        Group group;
        try {
            group = groupDAO.find(requestor.getAccount(),
                    requestBody.get("GroupName"));
        } catch (DataAccessException ex) {
            return responseGenerator.internalServerError();
        }

        if (group.exists()) {
            return responseGenerator.entityAlreadyExists();
        }

        group.setGroupId(KeyGenUtil.createId());
        String arn = ARNUtil.createARN(requestor.getAccount().getId(), "group",
                group.getGroupId());
        group.setARN(arn);

        if (requestBody.containsKey("path")) {
            group.setPath(requestBody.get("path"));
        } else {
            group.setPath("/");
        }

        String currentTime = DateUtil.toServerResponseFormat(DateTime.now());
        group.setCreateDate(currentTime);

        LOGGER.info("Creating group: " + group.getName());

        try {
            groupDAO.save(group);
        } catch (DataAccessException ex) {
            return responseGenerator.internalServerError();
        }

        return responseGenerator.generateCreateResponse(group);
    }

}
