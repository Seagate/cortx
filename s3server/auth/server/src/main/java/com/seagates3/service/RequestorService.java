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
 * Original creation date: 26-May-2016
 */
package com.seagates3.service;

import com.seagates3.dao.AccessKeyDAO;
import com.seagates3.dao.DAODispatcher;
import com.seagates3.dao.DAOResource;
import com.seagates3.dao.RequestorDAO;
import com.seagates3.exception.DataAccessException;
import com.seagates3.exception.InternalServerException;
import com.seagates3.exception.InvalidAccessKeyException;
import com.seagates3.exception.InvalidRequestorException;
import com.seagates3.model.AccessKey;
import com.seagates3.authentication.ClientRequestToken;
import com.seagates3.model.Requestor;
import com.seagates3.perf.S3Perf;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.generator.ResponseGenerator;
import com.seagates3.util.DateUtil;
import org.joda.time.DateTime;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class RequestorService {

    private static final Logger LOGGER = LoggerFactory.getLogger(
            RequestorService.class.getName());

    private static final S3Perf perf = new S3Perf();

    private static final ResponseGenerator responseGenerator
            = new ResponseGenerator();

    public
     static Requestor getRequestor(ClientRequestToken clientRequestToken)
         throws InvalidAccessKeyException,
         InternalServerException, InvalidRequestorException {

        ServerResponse serverResponse;
        AccessKey accessKey;

        AccessKeyDAO accessKeyDAO = (AccessKeyDAO) DAODispatcher
                .getResourceDAO(DAOResource.ACCESS_KEY);

        try {
            perf.startClock();

            accessKey = accessKeyDAO.find(clientRequestToken.getAccessKeyId());

            perf.endClock();
            perf.printTime("Fetch access key");
        } catch (DataAccessException ex) {
            LOGGER.error("Error occured while searching for requestor's "
                    + "access key id.\n" + ex.getMessage());
            serverResponse = responseGenerator.internalServerError();
            throw new InternalServerException(serverResponse);
        }

        validateAccessKey(accessKey);
        LOGGER.debug("Access key is valid.\n");

        RequestorDAO requestorDAO = (RequestorDAO) DAODispatcher
                .getResourceDAO(DAOResource.REQUESTOR);

        Requestor requestor;
        try {
            perf.startClock();

            requestor = requestorDAO.find(accessKey);

            perf.endClock();
            perf.printTime("Fetch requestor");
        } catch (DataAccessException ex) {
            serverResponse = responseGenerator.internalServerError();
            throw new InternalServerException(serverResponse);
        }

        validateRequestor(requestor, clientRequestToken);
        return requestor;
    }

    /**
     * Validate access Key.
     *
     * Check if access key exists. if access key is active.
     */
    private static Boolean validateAccessKey(AccessKey accessKey)
            throws InvalidAccessKeyException {
        ServerResponse serverResponse;
        /*
         * Return exit message if access key doesnt exist.
         */
        if (!accessKey.exists()) {
            LOGGER.debug("Access key doesn't exist.");
            serverResponse = responseGenerator.invalidAccessKey();
            throw new InvalidAccessKeyException(serverResponse);
        }

        if (!accessKey.isAccessKeyActive()) {
            LOGGER.debug("Access key in not active.");
            serverResponse = responseGenerator.inactiveAccessKey();
            throw new InvalidAccessKeyException(serverResponse);
        }

        return true;
    }

    /**
     * Validate the requestor.
     *
     * Check if requestor exists. if the requestor is a federated user, check if
     * the access key is valid.
     */
    private static Boolean validateRequestor(Requestor requestor,
            ClientRequestToken clientRequestToken)
            throws InvalidRequestorException {
        /*
         * Return internal server error if requestor doesn't exist.
         *
         * Ideally, an access key will be associated with a requestor.
         * It is a server error if the access key doesn't belong to a user.
         */

        /**
         * TODO - AWS supports an error message called "TokenRefreshRequired".
         * Use case - If the creds gets expired in the middle of a huge file
         * upload, this token can be issued.
         *
         */
        ServerResponse serverResponse;

        if (!requestor.exists()) {
            LOGGER.debug("Requestor doesn't exist.");
            serverResponse = responseGenerator.internalServerError();
            throw new InvalidRequestorException(serverResponse);
        }

        AccessKey accessKey = requestor.getAccesskey();
        if (requestor.isFederatedUser()) {
            LOGGER.debug("Requestor is using federated credentials.");
            String sessionToken = clientRequestToken.getRequestHeaders()
                    .get("X-Amz-Security-Token");
            if (!accessKey.getToken().equals(sessionToken)) {
                LOGGER.debug("Invalid clientt token ID.");
                serverResponse = responseGenerator.invalidClientTokenId();
                throw new InvalidRequestorException(serverResponse);
            }

            DateTime currentDate = DateUtil.getCurrentDateTime();
            DateTime expiryDate = DateUtil.toDateTime(accessKey.getExpiry());

            if (currentDate.isAfter(expiryDate)) {
                LOGGER.debug("Federated credentials have expired.");
                serverResponse = responseGenerator.expiredCredential();
                throw new InvalidRequestorException(serverResponse);
            }
        }

        return true;
    }
}


