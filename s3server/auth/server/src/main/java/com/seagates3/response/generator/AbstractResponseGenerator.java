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
 * Original creation date: 15-Dec-2015
 */
package com.seagates3.response.generator;

import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.formatter.xml.XMLResponseFormatter;

import io.netty.handler.codec.http.HttpResponseStatus;

/**
 * Implement all the commonly used response messages in this class.
 */
public abstract class AbstractResponseGenerator {

    public ServerResponse badRequest() {
        String errorMessage = "Bad Request. Check request headers and body.";

        return formatResponse(HttpResponseStatus.BAD_REQUEST, "BadRequest",
                              errorMessage);
    }

    public ServerResponse invalidToken() {
       String errorMessage =
           "The provided token is malformed or" + " otherwise invalid.";

       return formatResponse(HttpResponseStatus.BAD_REQUEST, "InvalidToken",
                             errorMessage);
    }

    public ServerResponse deleteConflict() {
        String errorMessage = "The request was rejected because it attempted "
                + "to delete a resource that has attached subordinate entities. "
                + "The error message describes these entities.";

        return formatResponse(HttpResponseStatus.CONFLICT, "DeleteConflict",
                errorMessage);
    }

    public ServerResponse entityAlreadyExists() {
        String errorMessage = "The request was rejected because it attempted "
                + "to create or update a resource that already exists.";

        return formatResponse(HttpResponseStatus.CONFLICT,
                "EntityAlreadyExists", errorMessage);
    }

    public ServerResponse expiredCredential() {
        String errorMessage = "The request was rejected because the credential "
                + "used to sign the request has expired.";

        return formatResponse(HttpResponseStatus.UNAUTHORIZED,
                "ExpiredCredential", errorMessage);
    }

    public ServerResponse inactiveAccessKey() {
        String errorMessage = "The access key used to sign the request is inactive.";
        return formatResponse(HttpResponseStatus.UNAUTHORIZED,
                "InactiveAccessKey", errorMessage);
    }

    public ServerResponse invalidAccessKey() {
        String errorMessage = "The AWS access key Id you provided does not "
                + "exist in our records.";
        return formatResponse(HttpResponseStatus.UNAUTHORIZED,
                "InvalidAccessKeyId", errorMessage);
    }

    public ServerResponse invalidUser() {
        String errorMessage = "User is not authorized to perform invoked action. ";
        return formatResponse(HttpResponseStatus.UNAUTHORIZED, "InvalidUser",
                              errorMessage);
    }

    public ServerResponse invalidLdapUserId() {
        String errorMessage = "The Ldap user id you provided does not exist.";
        return formatResponse(HttpResponseStatus.UNAUTHORIZED,
                "InvalidLdapUserId", errorMessage);
    }

    public ServerResponse signatureDoesNotMatch() {
        String errorMessage = "The request signature we calculated does not "
                + "match the signature you provided. Check your AWS secret "
                + "access key and signing method. For more information, see "
                + "REST Authentication andSOAP Authentication for details.";
        return formatResponse(HttpResponseStatus.UNAUTHORIZED,
                "SignatureDoesNotMatch", errorMessage);
    }

    public ServerResponse unauthorizedOperation() {
        String errorMessage = "You are not authorized to perform this operation."
                + " Check your IAM policies, and ensure that you are using the "
                + "correct access keys. ";
        return formatResponse(HttpResponseStatus.UNAUTHORIZED,
                "UnauthorizedOperation", errorMessage);
    }

    public ServerResponse internalServerError() {
        String errorMessage = "The request processing has failed because of an "
                + "unknown error, exception or failure.";

        return formatResponse(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                "InternalFailure", errorMessage);
    }

   public
    ServerResponse AccessDenied() {
      String errorMessage = "Access Denied.";

      return formatResponse(HttpResponseStatus.FORBIDDEN, "AccessDenied",
                            errorMessage);
    }

    public ServerResponse invalidAction() {
        String errorMessage = "The action or operation requested is "
                + "invalid. Verify that the action is typed correctly.";

        return formatResponse(HttpResponseStatus.BAD_REQUEST, "InvalidAction",
                errorMessage);
    }

    public ServerResponse invalidClientTokenId() {
        String errorMessage = "The X.509 certificate or AWS access key ID "
                + "provided does not exist in our records.";

        return formatResponse(HttpResponseStatus.UNAUTHORIZED,
                "InvalidClientTokenId", errorMessage);
    }

    public ServerResponse invalidParametervalue() {
        String errorMessage = "An invalid or out-of-range value was "
                + "supplied for the input parameter.";

        return formatResponse(HttpResponseStatus.BAD_REQUEST,
                "InvalidParameterValue", errorMessage);
    }

    public ServerResponse invalidParametervalue(String errorMessage) {
        return formatResponse(HttpResponseStatus.BAD_REQUEST,
                "InvalidParameterValue", errorMessage);
    }

    public ServerResponse missingParameter() {
        String errorMessage = "A required parameter for the specified action "
                + "is not supplied.";

        return formatResponse(HttpResponseStatus.BAD_REQUEST,
                              "MissingParameter", errorMessage);
    }

    public ServerResponse noSuchEntity() {
        String errorMessage = "The request was rejected because it referenced an "
                + "entity that does not exist. ";

        return formatResponse(HttpResponseStatus.UNAUTHORIZED, "NoSuchEntity",
                errorMessage);
    }

    public ServerResponse operationNotSupported() {
        String errorMessage = "The requested operation is not supported.";

        return formatResponse(HttpResponseStatus.UNAUTHORIZED,
                "OperationNotSupported", errorMessage);
    }

    public ServerResponse operationNotSupported(String errorMessage) {
        return formatResponse(HttpResponseStatus.UNAUTHORIZED,
                "OperationNotSupported", errorMessage);
    }

    public ServerResponse accountNotEmpty() {
        String errorMessage = "Account cannot be deleted as it owns some resources.";

        return formatResponse(HttpResponseStatus.CONFLICT, "AccountNotEmpty",
                              errorMessage);
    }

    public ServerResponse invalidArgument() {
        String errorMessage = "Invalid Argument";
        return formatResponse(HttpResponseStatus.BAD_REQUEST,
                "InvalidArgument", errorMessage);
    }

   public
    ServerResponse invalidID() {
      String errorMessage = "InvalidID";
      return formatResponse(HttpResponseStatus.BAD_REQUEST, "InvalidID",
                            errorMessage);
    }

   public
    ServerResponse invalidID(String errorMessage) {

      return formatResponse(HttpResponseStatus.BAD_REQUEST, "InvalidID",
                            errorMessage);
    }

   public
    ServerResponse invalidACL() {
      String errorMessage =
          "The ACL you provided was not " +
          "well-formed or did not validate against our published schema";
      return formatResponse(HttpResponseStatus.BAD_REQUEST, "InvalidACL",
                            errorMessage);
    }

   public
    ServerResponse grantListSizeViolation() {
      String errorMessage =
          "Number of ACL grants exceed the pre-defined limit of - " +
          AuthServerConfig.MAX_GRANT_SIZE;
      return formatResponse(HttpResponseStatus.BAD_REQUEST, "InvalidACL",
                            errorMessage);
    }

   public
    ServerResponse invalidPassword() {
      String errorMessage =
          "Either the new password or the old password was incorrect";
      return formatResponse(HttpResponseStatus.BAD_REQUEST, "InvalidPassword",
                            errorMessage);
    }

   public
    ServerResponse passwordPolicyVoilation() {
      String errorMessage =
          "Password does not conform to the account password policy";
      return formatResponse(HttpResponseStatus.BAD_REQUEST,
                            "PasswordPolicyVoilation", errorMessage);
    }

   public
    ServerResponse invalidUserType() {
      String errorMessage = "Only IAM user can change their own password";
      return formatResponse(HttpResponseStatus.UNAUTHORIZED, "InvalidUserType",
                            errorMessage);
    }

   public
    ServerResponse invalidUserType(String operation) {
      String errorMessage = "Cannot " + operation.toLowerCase() +
                            " account login profile" + " with " + operation +
                            "UserLoginProfile";
      return formatResponse(HttpResponseStatus.UNAUTHORIZED, "InvalidUserType",
                            errorMessage);
    }

    /**
     * Use this method for internal purpose.
     *
     * @return
     */
    public ServerResponse ok() {
        String errorMessage = "Action successful";
        return new ServerResponse(HttpResponseStatus.OK, errorMessage);
    }

    /**
     * TODO - Identify the return type format i.e XML or JSON and call the
     * respective formatter.
     *
     * @param httpResponseStatus
     * @param responseCode
     * @param responseBody
     * @return
     */
   protected
    ServerResponse formatResponse(HttpResponseStatus httpResponseStatus,
                                  String responseCode, String responseBody) {
      return new XMLResponseFormatter().formatErrorResponse(
          httpResponseStatus, responseCode, responseBody);
    }

   public
    ServerResponse invalidCredentials() {
      String errorMessage =
          "The request was rejected because the credentials " +
          "used in request are invalid.";
      return formatResponse(HttpResponseStatus.UNAUTHORIZED,
                            "InvalidCredentials", errorMessage);
    }

   public
    ServerResponse passwordResetRequired() {
      String errorMessage =
          "The request was rejected because the password " + "is not reset";

      return formatResponse(HttpResponseStatus.UNAUTHORIZED,
                            "PasswordResetRequired", errorMessage);
    }

   public
    ServerResponse maxDurationIntervalExceeded() {
      String errorMessage =
          "The request was rejected because the maximum allowed interval " +
          "duration " + "is exceeded";
      return formatResponse(HttpResponseStatus.NOT_ACCEPTABLE,
                            "MaxDurationIntervalExceeded", errorMessage);
    }

   public
    ServerResponse minDurationIntervalViolated() {
      String errorMessage =
          "The request was rejected because the minimum required interval " +
          "duration " + "is not maintained";
      return formatResponse(HttpResponseStatus.NOT_ACCEPTABLE,
                            "MinDurationIntervalNotMaintained", errorMessage);
    }

   public
    ServerResponse noSuchEntity(String errorMessage) {
      return formatResponse(HttpResponseStatus.UNAUTHORIZED, "NoSuchEntity",
                            errorMessage);
    }

   public
    ServerResponse unresolvableGrantByEmailAddress() {
      String errorMessage =
          "The email address you provided does not match any account on " +
          "record.";
      return formatResponse(HttpResponseStatus.BAD_REQUEST,
                            "UnresolvableGrantByEmailAddress", errorMessage);
    }

   public
    ServerResponse invalidArgument(String errorMessage) {
      return formatResponse(HttpResponseStatus.BAD_REQUEST, "InvalidArgument",
                            errorMessage);
    }

   public
    ServerResponse invalidRequest(String errorMessage) {
      return formatResponse(HttpResponseStatus.BAD_REQUEST, "InvalidRequest",
                            errorMessage);
    }

   public
    ServerResponse unexpectedContent(String errorMessage) {
      return formatResponse(HttpResponseStatus.BAD_REQUEST, "UnexpectedContent",
                            errorMessage);
    }

   public
    ServerResponse methodNotAllowed(String errorMessage) {
      return formatResponse(HttpResponseStatus.METHOD_NOT_ALLOWED,
                            "MethodNotAllowed", errorMessage);
    }

   public
    ServerResponse invalidSignatureDate() {
      String errorMessage =
          "AWS authentication requires a valid Date or x-amz-date header";

      return formatResponse(HttpResponseStatus.FORBIDDEN, "AccessDenied",
                            errorMessage);
    }
 }
