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
 * Original creation date: 1-Nov-2015
 */
package com.seagates3.controller;

import com.seagates3.dao.DAODispatcher;
import com.seagates3.dao.DAOResource;
import com.seagates3.dao.RoleDAO;
import com.seagates3.exception.DataAccessException;
import com.seagates3.model.Requestor;
import com.seagates3.model.Role;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.generator.RoleResponseGenerator;
import com.seagates3.util.ARNUtil;
import com.seagates3.util.DateUtil;
import com.seagates3.util.KeyGenUtil;
import java.util.Map;
import org.joda.time.DateTime;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class RoleController extends AbstractController {

    RoleDAO roleDAO;
    RoleResponseGenerator responseGenerator;
    private final Logger LOGGER =
            LoggerFactory.getLogger(RoleController.class.getName());

    public RoleController(Requestor requestor,
            Map<String, String> requestBody) {
        super(requestor, requestBody);

        roleDAO = (RoleDAO) DAODispatcher.getResourceDAO(DAOResource.ROLE);
        responseGenerator = new RoleResponseGenerator();
    }

    /**
     * Create new role.
     *
     * @return ServerReponse
     */
    @Override
    public ServerResponse create() {
        Role role;
        try {
            role = roleDAO.find(requestor.getAccount(),
                    requestBody.get("RoleName"));
        } catch (DataAccessException ex) {
            return responseGenerator.internalServerError();
        }

        if (role.exists()) {
            return responseGenerator.entityAlreadyExists();
        }

        role.setRoleId(KeyGenUtil.createId());
        role.setRolePolicyDoc(requestBody.get("AssumeRolePolicyDocument"));

        if (requestBody.containsKey("path")) {
            role.setPath(requestBody.get("path"));
        } else {
            role.setPath("/");
        }

        role.setCreateDate(DateUtil.toServerResponseFormat(DateTime.now()));
        String arn = ARNUtil.createARN(requestor.getAccount().getId(), "role",
                role.getRoleId());
        role.setARN(arn);

        LOGGER.info("Creating role:" + role.getName());

        try {
            roleDAO.save(role);
        } catch (DataAccessException ex) {
            return responseGenerator.internalServerError();
        }

        return responseGenerator.generateCreateResponse(role);
    }

    /*
     * TODO
     * Check if the role has policy attached before deleting.
     */
    @Override
    public ServerResponse delete() {
        Role role;
        try {
            role = roleDAO.find(requestor.getAccount(),
                    requestBody.get("RoleName"));
        } catch (DataAccessException ex) {
            return responseGenerator.internalServerError();
        }

        if (!role.exists()) {
            return responseGenerator.noSuchEntity();
        }

        LOGGER.info("Deleting role:" + role.getName());

        try {
            roleDAO.delete(role);
        } catch (DataAccessException ex) {
            return responseGenerator.internalServerError();
        }

        return responseGenerator.generateDeleteResponse();
    }

    @Override
    public ServerResponse list() {
        String pathPrefix;

        if (requestBody.containsKey("PathPrefix")) {
            pathPrefix = requestBody.get("PathPrefix");
        } else {
            pathPrefix = "/";
        }

        LOGGER.info("Listing all roles of account: " + requestor.getAccount()
                                 + " pathPrefix: " + pathPrefix);

        Role[] roleList;
        try {
            roleList = roleDAO.findAll(requestor.getAccount(), pathPrefix);
        } catch (DataAccessException ex) {
            return responseGenerator.internalServerError();
        }

        return responseGenerator.generateListResponse(roleList);
    }
}
