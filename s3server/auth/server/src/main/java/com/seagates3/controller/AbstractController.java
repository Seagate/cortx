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
package com.seagates3.controller;

import com.seagates3.exception.DataAccessException;
import com.seagates3.model.Requestor;
import com.seagates3.response.ServerResponse;
import java.util.Map;

public abstract class AbstractController {

    final Requestor requestor;
    final Map<String, String> requestBody;

    public AbstractController(Requestor requestor, Map<String, String> requestBody) {
        this.requestor = requestor;
        this.requestBody = requestBody;
    }

    public ServerResponse create() throws DataAccessException {
        return null;
    }

    public ServerResponse delete() throws DataAccessException {
        return null;
    }

    public ServerResponse list() throws DataAccessException {
        return null;
    }

    public ServerResponse update() throws DataAccessException {
        return null;
    }

    public
     ServerResponse changepassword() throws DataAccessException { return null; }
}
