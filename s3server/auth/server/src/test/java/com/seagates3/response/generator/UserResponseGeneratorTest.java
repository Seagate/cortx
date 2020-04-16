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
 * Original creation date: 18-Dec-2015
 */
package com.seagates3.response.generator;

import java.util.ArrayList;

import org.junit.Assert;
import org.junit.Test;

import com.seagates3.model.User;
import com.seagates3.response.ServerResponse;

import io.netty.handler.codec.http.HttpResponseStatus;

public class UserResponseGeneratorTest {

    @Test
    public void testCreateResponse() {
        User user = new User();
        user.setPath("/");
        user.setName("s3user");
        user.setId("123");
        user.setArn("arn:aws:iam::1:user/s3user");

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" + "<CreateUserResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<CreateUserResult>" + "<User>" + "<Path>/</Path>" +
            "<UserName>s3user</UserName>" + "<UserId>123</UserId>" +
            "<Arn>arn:aws:iam::1:user/s3user</Arn>" + "</User>" +
            "</CreateUserResult>" + "<ResponseMetadata>" +
            "<RequestId>0000</RequestId>" + "</ResponseMetadata>" +
            "</CreateUserResponse>";

        UserResponseGenerator responseGenerator = new UserResponseGenerator();
        ServerResponse response =
            responseGenerator.generateCreateResponse(user);

        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.CREATED,
                            response.getResponseStatus());
    }

    @Test
    public void testListResponse() {
        ArrayList<User> users = new ArrayList<>();

        User user1 = new User();
        user1.setPath("/");
        user1.setName("s3user1");
        user1.setId("123");
        user1.setCreateDate("2015-12-19T07:20:29.000+0530");
        user1.setArn("arn:aws:iam::000:user/s3user1");

        User user2 = new User();
        user2.setPath("/");
        user2.setName("s3user2");
        user2.setId("456");
        user2.setCreateDate("2015-12-19T07:20:29.000+0530");
        user2.setArn("arn:aws:iam::000:user/s3user2");

        users.add(user1);
        users.add(user2);

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" + "<ListUsersResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<ListUsersResult>" + "<Users>" + "<member>" +
            "<UserId>123</UserId>" + "<Path>/</Path>" +
            "<UserName>s3user1</UserName>" +
            "<Arn>arn:aws:iam::000:user/s3user1</Arn>" +
            "<CreateDate>2015-12-19T07:20:29.000+0530</CreateDate>" +
            "</member>" + "<member>" + "<UserId>456</UserId>" +
            "<Path>/</Path>" + "<UserName>s3user2</UserName>" +
            "<Arn>arn:aws:iam::000:user/s3user2</Arn>" +
            "<CreateDate>2015-12-19T07:20:29.000+0530</CreateDate>" +
            "</member>" + "</Users>" + "<IsTruncated>false</IsTruncated>" +
            "</ListUsersResult>" + "<ResponseMetadata>" +
            "<RequestId>0000</RequestId>" + "</ResponseMetadata>" +
            "</ListUsersResponse>";

        User[] userList = new User[users.size()];
        UserResponseGenerator responseGenerator = new UserResponseGenerator();
        ServerResponse response =
            responseGenerator.generateListResponse(users.toArray(userList));

        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.OK,
                            response.getResponseStatus());
    }

    @Test
    public void testDeleteResponse() {
      final String expectedResponseBody =
          "<?xml version=\"1.0\" " + "encoding=\"UTF-8\" standalone=\"no\"?>" +
          "<DeleteUserResponse " +
          "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
          "<ResponseMetadata>" + "<RequestId>0000</RequestId>" +
          "</ResponseMetadata>" + "</DeleteUserResponse>";

        UserResponseGenerator responseGenerator = new UserResponseGenerator();
        ServerResponse response = responseGenerator.generateDeleteResponse();

        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.OK,
                            response.getResponseStatus());
    }

    @Test
    public void testUpdateResponse() {
      final String expectedResponseBody =
          "<?xml version=\"1.0\" " + "encoding=\"UTF-8\" standalone=\"no\"?>" +
          "<UpdateUserResponse " +
          "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
          "<ResponseMetadata>" + "<RequestId>0000</RequestId>" +
          "</ResponseMetadata>" + "</UpdateUserResponse>";

        UserResponseGenerator responseGenerator = new UserResponseGenerator();
        ServerResponse response = responseGenerator.generateUpdateResponse();

        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.OK,
                            response.getResponseStatus());
    }
}

