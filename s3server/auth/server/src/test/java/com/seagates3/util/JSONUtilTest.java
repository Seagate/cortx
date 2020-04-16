/*
 * COPYRIGHT 2017 SEAGATE LLC
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
 * Original creation date: 02-Jan-2017
 */
package com.seagates3.util;

import com.google.gson.JsonSyntaxException;
import com.seagates3.model.SAMLMetadataTokens;
import org.junit.Test;

import static org.junit.Assert.*;

public class JSONUtilTest {

    @Test
    public void serializeToJsonTest() {
        assertNotNull(JSONUtil.serializeToJson(new Object()));
    }

    @Test
    public void deserializeFromJsonTest() {
        String jsonBody = "{username: seagate}";

        assertNotNull(JSONUtil.deserializeFromJson(jsonBody, SAMLMetadataTokens.class));
    }

    @Test
    public void deserializeFromJsonTest_ShouldReturnNullIfJsonBodyIsEmpty() {
        String jsonBody = "";
        assertNull(JSONUtil.deserializeFromJson(jsonBody, SAMLMetadataTokens.class));
    }

    @Test(expected = JsonSyntaxException.class)
    public void deserializeFromJsonTest_InvalidSyntaxException() {
        JSONUtil.deserializeFromJson("InvalidJSONBody", SAMLMetadataTokens.class);
    }
}
