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
 * Original author:  Sushant Mane <sushant.mane@seagate.com>
 * Original creation date: 26-Oct-2016
 */
package com.seagates3.controller;

import com.seagates3.exception.FaultPointException;
import com.seagates3.fi.FaultPoints;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.generator.FaultPointsResponseGenerator;

import java.util.Map;

public class FaultPointsController {

    private final FaultPoints faultPoints = FaultPoints.getInstance();
    public FaultPointsResponseGenerator responseGenerator = new FaultPointsResponseGenerator();

    public ServerResponse set(Map<String, String> requestBody) {
        if (!requiredParamsExist(requestBody)) {
            return responseGenerator.badRequest("Too few parameters.");
        }

        try {
            String faultPoint = requestBody.get("FaultPoint");
            String mode = requestBody.get("Mode");
            int value = Integer.parseInt(requestBody.get("Value"));
            faultPoints.setFaultPoint(faultPoint, mode, value);
        } catch (IllegalArgumentException e) {
            return responseGenerator.invalidParametervalue();
        } catch (FaultPointException e) {
            return responseGenerator.faultPointAlreadySet(e.getMessage());
        } catch (Exception e) {
            return responseGenerator.internalServerError();
        }

        return responseGenerator.setSuccessful();
    }

    public ServerResponse reset(Map<String, String> requestBody) {
        if (!requestBody.containsKey("FaultPoint")) {
            return responseGenerator.badRequest("Invalid parameters.");
        }

        String faultPoint = requestBody.get("FaultPoint");
        try {
            faultPoints.resetFaultPoint(faultPoint);
        } catch (IllegalArgumentException e) {
            return responseGenerator.invalidParametervalue(e.getMessage());
        } catch (FaultPointException e) {
            return responseGenerator.faultPointNotSet(e.getMessage());
        } catch (Exception e) {
            return responseGenerator.internalServerError();
        }

        return responseGenerator.resetSuccessful();
    }

    private boolean requiredParamsExist(Map<String, String> requestBody) {
        if (requestBody.containsKey("FaultPoint") &&
                requestBody.containsKey("Mode") &&
                requestBody.containsKey("Value")) {
            return true;
        }

        return false;
    }
}
