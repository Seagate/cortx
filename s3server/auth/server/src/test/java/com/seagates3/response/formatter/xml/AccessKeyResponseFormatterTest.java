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
 * Original creation date: 19-Dec-2015
 */
package com.seagates3.response.formatter.xml;

import com.seagates3.response.ServerResponse;
import io.netty.handler.codec.http.HttpResponseStatus;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;

public class AccessKeyResponseFormatterTest {

    @Rule
    public final ExpectedException exception = ExpectedException.none();

    @Test
    public void testFormatListResponse() {
        ArrayList<LinkedHashMap<String, String>> accessKeyMembers = new ArrayList<>();
        LinkedHashMap responseElement1 = new LinkedHashMap();
        responseElement1.put("UserName", "s3user1");
        responseElement1.put("AccessKeyId", "AKIA1213");
        responseElement1.put("Status", "Active");
        responseElement1.put("CreateDate", "2015-12-19T07:20:29.000+0530");

        accessKeyMembers.add(responseElement1);

        LinkedHashMap responseElement2 = new LinkedHashMap();
        responseElement2.put("UserName", "s3user1");
        responseElement2.put("AccessKeyId", "AKIA4567");
        responseElement2.put("Status", "Active");
        responseElement2.put("CreateDate", "2015-12-18T07:20:29.000+0530");

        accessKeyMembers.add(responseElement2);

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" +
            "<ListAccessKeysResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<ListAccessKeysResult>" + "<UserName>s3user1</UserName>" +
            "<AccessKeyMetadata>" + "<member>" +
            "<UserName>s3user1</UserName>" +
            "<AccessKeyId>AKIA1213</AccessKeyId>" + "<Status>Active</Status>" +
            "<CreateDate>2015-12-19T07:20:29.000+0530</CreateDate>" +
            "</member>" + "<member>" + "<UserName>s3user1</UserName>" +
            "<AccessKeyId>AKIA4567</AccessKeyId>" + "<Status>Active</Status>" +
            "<CreateDate>2015-12-18T07:20:29.000+0530</CreateDate>" +
            "</member>" + "</AccessKeyMetadata>" +
            "<IsTruncated>false</IsTruncated>" + "</ListAccessKeysResult>" +
            "<ResponseMetadata>" + "<RequestId>0000</RequestId>" +
            "</ResponseMetadata>" + "</ListAccessKeysResponse>";

        ServerResponse response = new AccessKeyResponseFormatter().formatListResponse(
                "s3user1", accessKeyMembers, false, "0000");

        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.OK, response.getResponseStatus());
    }

    @Test
    public void testFormatListResponseException() {
        ArrayList< LinkedHashMap< String, String>> responseElements = new ArrayList<>();

        exception.expect(UnsupportedOperationException.class);
        new AccessKeyResponseFormatter().formatListResponse("ListAccessKeys",
                "Accesskey", responseElements, false, "0000");
    }
}
