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
 * Original creation date: 17-Mar-2016
 */
package com.seagates3.aws.sign;

import com.seagates3.authentication.AWSV4Sign;
import com.seagates3.aws.AWSV4RequestHelper;
import com.seagates3.exception.InvalidTokenException;
import com.seagates3.authentication.ClientRequestToken;
import com.seagates3.model.Requestor;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.powermock.core.classloader.annotations.PowerMockIgnore;
import org.powermock.modules.junit4.PowerMockRunner;

@RunWith(PowerMockRunner.class)
@PowerMockIgnore({"javax.crypto.*", "javax.management.*"})
public class AWSV4SignTest {

    private final AWSV4Sign awsv4Sign;
    private final Requestor requestor;

    public AWSV4SignTest() {
        awsv4Sign = new AWSV4Sign();
        requestor = AWSV4RequestHelper.getRequestor();
    }

    @Test
    public void Authenticate_ChunkedSeedRequest_True() {
        ClientRequestToken requestToken
                = AWSV4RequestHelper.getChunkedSeedRequestClientToken();

        try {
                Assert.assertEquals(awsv4Sign.authenticate(requestToken, requestor),
                   Boolean.TRUE);
        } catch (InvalidTokenException e) {
            e.printStackTrace();
            Assert.fail("This Shouldn't have thrown exception");
        }

    }

    @Test
    public void Authenticate_ChunkedSeedRequest_False() {
        ClientRequestToken requestToken
                = AWSV4RequestHelper.getChunkedSeedRequestClientToken();

        requestToken.setHttpMethod("GET");

        try {
            Assert.assertEquals(awsv4Sign.authenticate(requestToken, requestor),
               Boolean.FALSE);
        } catch (InvalidTokenException e) {
            e.printStackTrace();
            Assert.fail("This Shouldn't have thrown exception");
        }

    }

    @Test
    public void Authenticate_ChunkedRequest_True() {
        ClientRequestToken requestToken
                = AWSV4RequestHelper.getChunkedRequestClientToken();

        try {
            Assert.assertEquals(awsv4Sign.authenticate(requestToken, requestor),
               Boolean.TRUE);
        } catch (InvalidTokenException e) {
            e.printStackTrace();
            Assert.fail("This Shouldn't have thrown exception");
        }

    }

    @Test
    public void Authenticate_RequestPathStyle_True() {
        ClientRequestToken requestToken
                = AWSV4RequestHelper.getRequestClientTokenPathStyle();

        try {
            Assert.assertEquals(awsv4Sign.authenticate(requestToken, requestor),
               Boolean.TRUE);
        } catch (InvalidTokenException e) {
            e.printStackTrace();
            Assert.fail("This Shouldn't have thrown exception");
        }

    }

    @Test
    public void Authenticate_RequestVirtualHostStyle_True() {
        ClientRequestToken requestToken
                = AWSV4RequestHelper.getRequestClientTokenVirtualHostStyle();

        try {
            Assert.assertEquals(awsv4Sign.authenticate(requestToken, requestor),
                    Boolean.TRUE);
        } catch (InvalidTokenException e) {
            e.printStackTrace();
            Assert.fail("This Shouldn't have thrown exception");
        }

    }

    @Test
    public void Authenticate_RequestSpecialQueryParams() {
        ClientRequestToken requestToken
                = AWSV4RequestHelper.getRequestClientTokenSpecialQuery();

        try {
            Assert.assertEquals(awsv4Sign.authenticate(requestToken, requestor),
                    Boolean.TRUE);
        } catch (InvalidTokenException e) {
            e.printStackTrace();
            Assert.fail("This Shouldn't have thrown exception");
        }

    }

    @Test
    public void Authenticate_RequestVirtualHostStyleHead_True() {
        ClientRequestToken requestToken
                = AWSV4RequestHelper.getFullHttpRequestClientTokenHEAD();

        Requestor requestor1 =
                AWSV4RequestHelper.getRequestorMock("AKIAJTYX36YCKQSAJT7Q",
                                "A6k2z84BqwXmee4WUUS2oWwM/tha7Wrd4Hc/8yRt");
        try {
            Assert.assertEquals(awsv4Sign.authenticate(requestToken, requestor1),
                    Boolean.TRUE);
        } catch (InvalidTokenException e) {
            e.printStackTrace();
            Assert.fail("This Shouldn't have thrown exception");
        }

    }

    @Test
    public void Authenticate_RequestVirtualHostStyle_InvalidRequest() {
        ClientRequestToken requestToken
                = AWSV4RequestHelper.getInvalidHttpRequestClientToken();

        Requestor requestor1 =
                AWSV4RequestHelper.getRequestorMock("AKIAJTYX36YCKQSAJT7Q",
                                "A6k2z84BqwXmee4WUUS2oWwM/tha7Wrd4Hc/8yRt");
        try {
            awsv4Sign.authenticate(requestToken, requestor1);
            Assert.fail("Didn't throw BadRequest Exception");
        } catch (InvalidTokenException e) {
            Assert.assertTrue(e.getMessage().contains("Signed header :"
                    + "connection is not found in Request header list"));
        }
    }

}
