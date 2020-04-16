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

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

class FaultPoint {

    private final Logger LOGGER = LoggerFactory.getLogger(FaultPoint.class.getName());
    private enum Mode { FAIL_ALWAYS, FAIL_ONCE, FAIL_N_TIMES, SKIP_FIRST_N_TIMES }
    private String failLocation;
    private Mode mode;
    private int failOrSkipCount;
    private int hitCount;

    FaultPoint(String failLocation, String mode, int failOrSkipCount) {
        this.failLocation = failLocation;
        this.mode = Mode.valueOf(mode.toUpperCase());
        this.failOrSkipCount = failOrSkipCount;
    }

    private boolean checkAndUpdateState() {
        hitCount += 1;
        if (mode == Mode.FAIL_ONCE) {
            if (hitCount > 1) {
                return false;
            }
        } else if (mode == Mode.FAIL_N_TIMES) {
            if (hitCount > failOrSkipCount) {
                return false;
            }
        } else if (mode == Mode.SKIP_FIRST_N_TIMES) {
            if (hitCount <= failOrSkipCount) {
                return false;
            }
        }

        return true;
    }

    public boolean isActive() {
        if(checkAndUpdateState()) {
            LOGGER.debug("FailLocation: " + failLocation + " HitCount: " + hitCount);
            return true;
        }

        return false;
    }
}
