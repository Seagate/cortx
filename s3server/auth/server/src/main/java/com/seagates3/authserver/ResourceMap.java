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
 * Original creation date: 21-May-2016
 */
package com.seagates3.authserver;

import org.apache.commons.lang.StringUtils;

public class ResourceMap {

    private final String controllerName;
    private final String action;

    private final String VALIDATOR_PACKAGE = "com.seagates3.parameter.validator";
    private final String CONTROLLER_PACKAGE = "com.seagates3.controller";

    public ResourceMap(String controllerName, String action) {
        this.controllerName = controllerName;
        this.action = action;
    }

    /**
     * Return the full class path of the controller.
     *
     * @return
     */
    public String getControllerClass() {
        return String.format("%s.%sController", CONTROLLER_PACKAGE,
                controllerName);
    }

    /**
     * Return the full name of the validator class.
     *
     * @return
     */
    public String getParamValidatorClass() {
        return String.format("%s.%sParameterValidator", VALIDATOR_PACKAGE,
                controllerName);
    }

    /**
     * Return the Controller Action to be invoked.
     *
     * @return Action.
     */
    public String getControllerAction() {
        return action;
    }

    /**
     * Return the Controller Action to be invoked.
     *
     * @return Action.
     */
    public String getParamValidatorMethod() {
        return String.format("isValid%sParams", StringUtils.capitalize(action));
    }

}
