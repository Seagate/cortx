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
 * Original author: Sushant Mane <sushant.mane@seagate.com>
 * Original creation date: 26-Oct-2016
 */
package com.seagates3.fi;

import com.seagates3.exception.FaultPointException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.HashMap;
import java.util.Map;

/*
 * This class is not thread safe.
 * Instance is created during startup phase in main method.
 */
public class FaultPoints {

    private final Logger LOGGER = LoggerFactory.getLogger(FaultPoints.class.getName());
    private Map<String, FaultPoint> faults = new HashMap<>();
    private static FaultPoints instance;

    private FaultPoints() {}

    public static void init() {
        instance = new FaultPoints();
    }

    public static FaultPoints getInstance() {
        return instance;
    }

    public static boolean fiEnabled() {
        if (instance == null) {
            return false;
        }

        return true;
    }

    public boolean isFaultPointSet(String failLocation) {
        return faults.containsKey(failLocation);
    }

    public boolean isFaultPointActive(String failLocation) {
        if (isFaultPointSet(failLocation)) {
            return faults.get(failLocation).isActive();
        }

        return false;
    }

    public void setFaultPoint(String failLocation, String mode, int value)
            throws FaultPointException {
        if (failLocation == null || failLocation.isEmpty()) {
            LOGGER.debug("Fault point can't be empty or null");
            throw new IllegalArgumentException("Invalid fault point");
        }
        if (faults.containsKey(failLocation)) {
            LOGGER.debug("Fault point " + failLocation + " already set");
            throw new FaultPointException("Fault point " + failLocation + " already set");
        }

        FaultPoint faultPoint = new FaultPoint(failLocation, mode, value);
        faults.put(failLocation, faultPoint);
        LOGGER.debug("Fault point: " + failLocation + " is set successfully.");
    }

    public void resetFaultPoint(String failLocation) throws FaultPointException {
        if (failLocation == null || failLocation.isEmpty()) {
            LOGGER.debug("Fault point can't be empty or null");
            throw new IllegalArgumentException("Invalid fault point");
        }
        if (!faults.containsKey(failLocation)) {
            LOGGER.debug("Fault point " + failLocation + " is not set");
            throw new FaultPointException("Fault point " + failLocation + " is not set");
        }

        faults.remove(failLocation);
        LOGGER.debug("Fault point: " + failLocation + " is removed successfully");
    }
}
