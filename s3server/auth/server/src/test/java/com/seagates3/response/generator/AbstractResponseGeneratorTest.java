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

import com.seagates3.response.ServerResponse;
import io.netty.handler.codec.http.HttpResponseStatus;
import org.junit.Assert;
import org.junit.Test;

public
class AbstractResponseGeneratorTest extends AbstractResponseGenerator {

  @Test public void testBadRequest() {
    final String expectedResponseBody =
        "<?xml version=\"1.0\" " + "encoding=\"UTF-8\" standalone=\"no\"?>" +
        "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
        "<Error><Code>BadRequest</Code>" +
        "<Message>Bad Request. Check request headers and " +
        "body.</Message></Error>" + "<RequestId>0000</RequestId>" +
        "</ErrorResponse>";

    ServerResponse response = badRequest();
    Assert.assertEquals(expectedResponseBody, response.getResponseBody());
    Assert.assertEquals(HttpResponseStatus.BAD_REQUEST,
                        response.getResponseStatus());
  }

  @Test public void testDeleteConflict() {
    final String expectedResponseBody =
        "<?xml version=\"1.0\" " + "encoding=\"UTF-8\" standalone=\"no\"?>" +
        "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
        "<Error><Code>DeleteConflict</Code>" +
        "<Message>The request was rejected because it attempted to " +
        "delete a resource that has attached subordinate entities. " +
        "The error message describes these entities.</Message></Error>" +
        "<RequestId>0000</RequestId>" + "</ErrorResponse>";

    ServerResponse response = deleteConflict();
    Assert.assertEquals(expectedResponseBody, response.getResponseBody());
    Assert.assertEquals(HttpResponseStatus.CONFLICT,
                        response.getResponseStatus());
  }

  @Test public void testEntityAlreadyExists() {
    final String expectedResponseBody =
        "<?xml version=\"1.0\" " + "encoding=\"UTF-8\" standalone=\"no\"?>" +
        "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
        "<Error><Code>EntityAlreadyExists</Code>" +
        "<Message>The request was rejected because it attempted to " +
        "create or update a resource that already exists.</Message></Error>" +
        "<RequestId>0000</RequestId>" + "</ErrorResponse>";

    ServerResponse response = entityAlreadyExists();
    Assert.assertEquals(expectedResponseBody, response.getResponseBody());
    Assert.assertEquals(HttpResponseStatus.CONFLICT,
                        response.getResponseStatus());
  }

  @Test public void testExpiredCredential() {
    final String expectedResponseBody =
        "<?xml version=\"1.0\" " + "encoding=\"UTF-8\" standalone=\"no\"?>" +
        "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
        "<Error><Code>ExpiredCredential</Code>" +
        "<Message>The request was rejected because the credential " +
        "used to sign the request has expired.</Message></Error>" +
        "<RequestId>0000</RequestId>" + "</ErrorResponse>";

    ServerResponse response = expiredCredential();
    Assert.assertEquals(expectedResponseBody, response.getResponseBody());
    Assert.assertEquals(HttpResponseStatus.UNAUTHORIZED,
                        response.getResponseStatus());
  }

  @Test public void testInactiveAccessKey() {
    final String expectedResponseBody =
        "<?xml version=\"1.0\" " + "encoding=\"UTF-8\" standalone=\"no\"?>" +
        "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
        "<Error><Code>InactiveAccessKey</Code>" +
        "<Message>The access key used to sign the request is inactive." +
        "</Message></Error>" + "<RequestId>0000</RequestId>" +
        "</ErrorResponse>";

    ServerResponse response = inactiveAccessKey();
    Assert.assertEquals(expectedResponseBody, response.getResponseBody());
    Assert.assertEquals(HttpResponseStatus.UNAUTHORIZED,
                        response.getResponseStatus());
  }

  @Test public void testSignatureDoesNotMatch() {
    final String expectedResponseBody =
        "<?xml version=\"1.0\" " + "encoding=\"UTF-8\" standalone=\"no\"?>" +
        "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
        "<Error><Code>SignatureDoesNotMatch</Code>" +
        "<Message>The request signature we calculated does not match " +
        "the signature you provided. Check your AWS secret access key " +
        "and signing method. For more information, see REST " +
        "Authentication andSOAP Authentication for details.</Message></Error>" +
        "<RequestId>0000</RequestId>" + "</ErrorResponse>";

    ServerResponse response = signatureDoesNotMatch();
    Assert.assertEquals(expectedResponseBody, response.getResponseBody());
    Assert.assertEquals(HttpResponseStatus.UNAUTHORIZED,
                        response.getResponseStatus());
  }

  @Test public void testInternalServerError() {
    final String expectedResponseBody =
        "<?xml version=\"1.0\" " + "encoding=\"UTF-8\" standalone=\"no\"?>" +
        "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
        "<Error><Code>InternalFailure</Code>" +
        "<Message>The request processing has failed because of an " +
        "unknown error, exception or failure.</Message></Error>" +
        "<RequestId>0000</RequestId>" + "</ErrorResponse>";

    ServerResponse response = internalServerError();
    Assert.assertEquals(expectedResponseBody, response.getResponseBody());
    Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                        response.getResponseStatus());
  }

  @Test public void testInvalidAction() {
    final String expectedResponseBody =
        "<?xml version=\"1.0\" " + "encoding=\"UTF-8\" standalone=\"no\"?>" +
        "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
        "<Error><Code>InvalidAction</Code>" +
        "<Message>The action or operation requested is invalid. " +
        "Verify that the action is typed correctly.</Message></Error>" +
        "<RequestId>0000</RequestId>" + "</ErrorResponse>";

    ServerResponse response = invalidAction();
    Assert.assertEquals(expectedResponseBody, response.getResponseBody());
    Assert.assertEquals(HttpResponseStatus.BAD_REQUEST,
                        response.getResponseStatus());
  }

  @Test public void testInvalidClientTokenId() {
    final String expectedResponseBody =
        "<?xml version=\"1.0\" " + "encoding=\"UTF-8\" standalone=\"no\"?>" +
        "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
        "<Error><Code>InvalidClientTokenId</Code>" +
        "<Message>The X.509 certificate or AWS access key ID provided " +
        "does not exist in our records.</Message></Error>" +
        "<RequestId>0000</RequestId>" + "</ErrorResponse>";

    ServerResponse response = invalidClientTokenId();
    Assert.assertEquals(expectedResponseBody, response.getResponseBody());
    Assert.assertEquals(HttpResponseStatus.UNAUTHORIZED,
                        response.getResponseStatus());
  }

  @Test public void testInvalidParameterValue() {
    final String expectedResponseBody =
        "<?xml version=\"1.0\" " + "encoding=\"UTF-8\" standalone=\"no\"?>" +
        "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
        "<Error><Code>InvalidParameterValue</Code>" +
        "<Message>An invalid or out-of-range value was supplied for " +
        "the input parameter.</Message></Error>" +
        "<RequestId>0000</RequestId>" + "</ErrorResponse>";

    ServerResponse response = invalidParametervalue();
    Assert.assertEquals(expectedResponseBody, response.getResponseBody());
    Assert.assertEquals(HttpResponseStatus.BAD_REQUEST,
                        response.getResponseStatus());
  }

  @Test public void testMissingParameter() {
    final String expectedResponseBody =
        "<?xml version=\"1.0\" " + "encoding=\"UTF-8\" standalone=\"no\"?>" +
        "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
        "<Error><Code>MissingParameter</Code>" +
        "<Message>A required parameter for the specified action is " +
        "not supplied.</Message></Error>" + "<RequestId>0000</RequestId>" +
        "</ErrorResponse>";

    ServerResponse response = missingParameter();
    Assert.assertEquals(expectedResponseBody, response.getResponseBody());
    Assert.assertEquals(HttpResponseStatus.BAD_REQUEST,
                        response.getResponseStatus());
  }

  @Test public void testNoSuchEntity() {
    final String expectedResponseBody =
        "<?xml version=\"1.0\" " + "encoding=\"UTF-8\" standalone=\"no\"?>" +
        "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
        "<Error><Code>NoSuchEntity</Code>" +
        "<Message>The request was rejected because it referenced an " +
        "entity that does not exist. </Message></Error>" +
        "<RequestId>0000</RequestId>" + "</ErrorResponse>";

    ServerResponse response = noSuchEntity();
    Assert.assertEquals(expectedResponseBody, response.getResponseBody());
    Assert.assertEquals(HttpResponseStatus.UNAUTHORIZED,
                        response.getResponseStatus());
  }

  @Test public void testOKResponse() {
    final String expectedResponseBody = "Action successful";

    ServerResponse response = ok();
    Assert.assertEquals(expectedResponseBody, response.getResponseBody());
    Assert.assertEquals(HttpResponseStatus.OK, response.getResponseStatus());
  }

  @Test public void testInvalidCredentials() {
    final String expectedResponseBody =
        "<?xml version=\"1.0\" " + "encoding=\"UTF-8\" standalone=\"no\"?>" +
        "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
        "<Error><Code>InvalidCredentials</Code>" +
        "<Message>The request was rejected because the credentials " +
        "used in request are invalid.</Message></Error>" +
        "<RequestId>0000</RequestId>" + "</ErrorResponse>";

    ServerResponse response = invalidCredentials();
    Assert.assertEquals(expectedResponseBody, response.getResponseBody());
    Assert.assertEquals(HttpResponseStatus.UNAUTHORIZED,
                        response.getResponseStatus());
  }

  @Test public void testPasswordResetRequired() {
    final String expectedResponseBody =
        "<?xml version=\"1.0\" " + "encoding=\"UTF-8\" standalone=\"no\"?>" +
        "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
        "<Error><Code>PasswordResetRequired</Code>" +
        "<Message>The request was rejected because the password is not " +
        "reset" + "</Message></Error>" + "<RequestId>0000</RequestId>" +
        "</ErrorResponse>";

    ServerResponse response = passwordResetRequired();
    Assert.assertEquals(expectedResponseBody, response.getResponseBody());
    Assert.assertEquals(HttpResponseStatus.UNAUTHORIZED,
                        response.getResponseStatus());
  }

  @Test public void testMaxDurationIntervalExceeded() {
    final String expectedResponseBody =
        "<?xml version=\"1.0\" " + "encoding=\"UTF-8\" standalone=\"no\"?>" +
        "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
        "<Error><Code>MaxDurationIntervalExceeded</Code>" +
        "<Message>The request was rejected because the maximum allowed " +
        "interval duration is exceeded" + "</Message></Error>" +
        "<RequestId>0000</RequestId>" + "</ErrorResponse>";

    ServerResponse response = maxDurationIntervalExceeded();
    Assert.assertEquals(expectedResponseBody, response.getResponseBody());
    Assert.assertEquals(HttpResponseStatus.NOT_ACCEPTABLE,
                        response.getResponseStatus());
  }

  @Test public void testMinDurationIntervalViolated() {
    final String expectedResponseBody =
        "<?xml version=\"1.0\" " + "encoding=\"UTF-8\" standalone=\"no\"?>" +
        "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
        "<Error><Code>MinDurationIntervalNotMaintained</Code>" +
        "<Message>The request was rejected because the minimum required " +
        "interval duration is not maintained" + "</Message></Error>" +
        "<RequestId>0000</RequestId>" + "</ErrorResponse>";

    ServerResponse response = minDurationIntervalViolated();
    Assert.assertEquals(expectedResponseBody, response.getResponseBody());
    Assert.assertEquals(HttpResponseStatus.NOT_ACCEPTABLE,
                        response.getResponseStatus());
  }
}
