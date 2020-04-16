/*
 * COPYRIGHT 2019 SEAGATE LLC
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
 * Original author:  Shalaka Dharap
 * Original creation date: 13-December-2019
 */

package com.seagates3.policy;

import java.util.Map;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.seagates3.model.Requestor;
import com.seagates3.response.ServerResponse;

public
abstract class PolicyAuthorizer extends PolicyAuthorizerConstants {

 protected
  final Logger LOGGER =
      LoggerFactory.getLogger(PolicyAuthorizer.class.getName());

 public
  abstract ServerResponse
      authorizePolicy(Requestor requestor, Map<String, String> requestBody);
}
