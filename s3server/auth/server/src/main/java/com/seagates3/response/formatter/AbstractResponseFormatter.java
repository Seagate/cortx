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
package com.seagates3.response.formatter;

import java.util.ArrayList;
import java.util.LinkedHashMap;

import com.seagates3.response.ServerResponse;

import io.netty.handler.codec.http.HttpResponseStatus;

public
abstract class AbstractResponseFormatter {

 protected
  final String IAM_XMLNS = "https://iam.seagate.com/doc/2010-05-08/";

 public
  abstract ServerResponse
      formatCreateResponse(String operation, String returnObject,
                           LinkedHashMap<String, String> responseElements,
                           String requestId);

 public
  abstract ServerResponse formatDeleteResponse(String operation);

 public
  abstract ServerResponse formatUpdateResponse(String operation);

 public
  abstract ServerResponse formatListResponse(
      String operation, String returnObject,
      ArrayList<LinkedHashMap<String, String>> responseElements,
      Boolean isTruncated, String requestId);

 public
  abstract ServerResponse
      formatErrorResponse(HttpResponseStatus httpResponseStatus, String code,
                          String message);

 public
  abstract ServerResponse formatGetResponse(
      String operation, String returnObject,
      ArrayList<LinkedHashMap<String, String>> responseElements,
      String requestId);

 public
  abstract ServerResponse formatChangePasswordResponse(String operation);
}
