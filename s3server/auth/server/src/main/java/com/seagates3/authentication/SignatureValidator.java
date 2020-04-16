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
 * Original creation date: 17-Sep-2014
 */
package com.seagates3.authentication;

import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Date;
import java.util.Map;
import java.util.TimeZone;
import java.util.concurrent.TimeUnit;

import org.joda.time.DateTime;
import com.seagates3.exception.InvalidTokenException;
import com.seagates3.model.Requestor;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.generator.AuthenticationResponseGenerator;
import com.seagates3.util.DateUtil;
import com.seagates3.util.IEMUtil;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import io.netty.handler.codec.http.HttpResponseStatus;

public class SignatureValidator {

    private final Logger LOGGER = LoggerFactory.getLogger(
            SignatureValidator.class.getName());

    private final String SIGNER_PACKAGE = "com.seagates3.authentication";

    public ServerResponse validate(ClientRequestToken clientRequestToken,
            Requestor requestor) {

        AuthenticationResponseGenerator responseGenerator
                = new AuthenticationResponseGenerator();

        AWSSign awsSign = getSigner(clientRequestToken);

        // validate dates in request signature
        ServerResponse dateValidationResponse =
            validateSignatureDate(clientRequestToken, awsSign);
        if (!dateValidationResponse.getResponseStatus().equals(
                 HttpResponseStatus.OK)) {
          return dateValidationResponse;
        }

        Boolean isRequestorAuthenticated = false;
        try {
          if (awsSign != null) {
            isRequestorAuthenticated = awsSign.authenticate(
                                         clientRequestToken, requestor);
          }
        }
        catch (InvalidTokenException e) {
            return responseGenerator.invalidToken();
        }
        if (!isRequestorAuthenticated) {
            LOGGER.debug("Requestor is not authenticated.");
            return responseGenerator.signatureDoesNotMatch();
        }

        LOGGER.debug("Requestor is authenticated.");
        return responseGenerator.ok();
    }

    /*
     * Validate request Signature
     * For v4 request if request date is same as credentialscope date then its
     * valid request
     * For v2 request if difference between request time and current server time
     * is within 15 minutes then its valid request
     * else return appropriate error response
     */
   private
    ServerResponse validateSignatureDate(ClientRequestToken clientRequestToken,
                                         AWSSign awsSign) {
      AuthenticationResponseGenerator responseGenerator =
          new AuthenticationResponseGenerator();

      Map<String, String> requestHeaders =
          clientRequestToken.getRequestHeaders();
      String requestDateString;
      Date requestDate = null;
      boolean isRequestInSkewTime = false, isValidDate = false;

      // Fetch header values from request
      if (requestHeaders.containsKey("x-amz-date")) {
        requestDateString = requestHeaders.get("x-amz-date");
        // Check for ISO8601 date format as per aws v4
        if (requestDateString.endsWith("Z")) {
          requestDate = DateUtil.parseDateString(requestDateString,
                                                 "yyyyMMdd'T'HHmmss'Z'");
        } else {  // Get GMT date header
          requestDate = DateUtil.parseDateString(requestDateString,
                                                 "EEE, dd MMM yyyy HH:mm:ss");
        }
      } else {  // Get GMT date header
        requestDateString = requestHeaders.get("Date");
        requestDate = DateUtil.parseDateString(requestDateString,
                                               "EEE, dd MMM yyyy HH:mm:ss");
      }
      // Handle Invalid date or Empty date check.
      if (requestDate == null) {
        LOGGER.error("Invalid date received in signature header.");
        return responseGenerator.invalidSignatureDate();
      }

      // Handle request timestamp and s3 server timestamp difference check
      DateTime currentDateTime = DateUtil.getCurrentDateTime();
      long timeInterval =
          Math.abs(currentDateTime.getMillis() - requestDate.getTime());
      long diffInMinutes =
          TimeUnit.MINUTES.convert(timeInterval, TimeUnit.MILLISECONDS);
      if (diffInMinutes <= 15) {
        isRequestInSkewTime = true;
      }

      // Handle Signature v4 credential scope date check
      if (awsSign instanceof AWSV4Sign) {
        isValidDate = compareCredentialScopeDate(clientRequestToken.getDate(),
                                                 requestDate);
        // If request is within 15 minutes of server time and
        // request date is same as credential scope date
        // then its valid request so send OK response
        if (isValidDate && isRequestInSkewTime) {
          return responseGenerator.ok();
        }
        if (!isValidDate) {  // Handle failures of request date and canonical
                             // scope mismatch
          LOGGER.error("Request date and Credential scope date is mismatched.");
          return responseGenerator.invalidSignatureDate();
        }
      } else if (isRequestInSkewTime) {  // Handle v2 signature valid response
        // Request time and server time is within 15 minutes then return OK
        // response
        return responseGenerator.ok();
      }

      // Handle error code messages for ceph s3tests

      // If request time stamp is before epoch time then returns
      // InvalidSignatureDate
      if (isRequestDateBeforeEpochDate(requestDate)) {
        LOGGER.error("Request date timestamp received is before epoch date.");
        return responseGenerator.invalidSignatureDate();
      }
      LOGGER.error(
          "Request date timestamp received does not match with server " +
          "timestamp.");
      return responseGenerator.requestTimeTooSkewed(requestDateString,
                                                    currentDateTime.toString());
    }

    /*
     * Check if given date is before epoch date
     */
   private
    static boolean isRequestDateBeforeEpochDate(Date requestDate) {
      Calendar cal = Calendar.getInstance();
      cal.setTime(requestDate);
      if (cal.get(Calendar.YEAR) < 1970) {
        return true;
      }
      return false;
    }

    /*
     * Compare Credential scope date and request date for aws v4 request
     */
   private
    static boolean compareCredentialScopeDate(String credentailScopeString,
                                              Date requestDate) {

      Date credentialScopeDate =
          DateUtil.parseDateString(credentailScopeString, "yyyyMMdd");

      if (requestDate == null || credentialScopeDate == null) {
        return false;
      }

      // Use UTC timezone while comparing the date object
      Calendar cal1 = Calendar.getInstance(TimeZone.getTimeZone("UTC"));
      Calendar cal2 = Calendar.getInstance(TimeZone.getTimeZone("UTC"));
      cal1.setTime(requestDate);
      cal2.setTime(credentialScopeDate);

      if (cal1.get(Calendar.YEAR) == cal2.get(Calendar.YEAR)) {
        int dayDifference =
            cal1.get(Calendar.DAY_OF_YEAR) - cal2.get(Calendar.DAY_OF_YEAR);
        if (dayDifference == 0) {
          return true;
        }
      }
      return false;
    }

    /*
      * <IEM_INLINE_DOCUMENTATION>
      *     <event_code>048004001</event_code>
      *     <application>S3 Authserver</application>
      *     <submodule>Reflection</submodule>
      *     <description>Failed to get required class</description>
      *     <audience>Development</audience>
      *     <details>
      *         Class not found exception occurred.
      *         The data section of the event has following keys:
      *           time - timestamp
      *           node - node name
      *           pid - process id of Authserver
      *           file - source code filename
      *           line - line number within file where error occurred
      *           cause - cause of exception
      *     </details>
      *     <service_actions>
      *         Save authserver log files and contact development team for
      *         further investigation.
      *     </service_actions>
      * </IEM_INLINE_DOCUMENTATION>
      *
      */
    /*
     * Get the client request version and return the AWS signer object.
     */
    private AWSSign getSigner(ClientRequestToken clientRequestToken) {
        LOGGER.debug("Signature version "
                + clientRequestToken.getSignVersion().toString());

        String signVersion = clientRequestToken.getSignVersion().toString();
        String signerClassName = toSignerClassName(signVersion);

        Class<?> awsSigner;
        Object obj;

        try {
            awsSigner = Class.forName(signerClassName);
            obj = awsSigner.newInstance();

            return (AWSSign) obj;
        } catch (ClassNotFoundException | SecurityException ex) {
            IEMUtil.log(IEMUtil.Level.ERROR, IEMUtil.CLASS_NOT_FOUND_EX,
                    "Failed to get required class",
                    String.format("\"cause\": \"%s\"", ex.getCause()));
        } catch (IllegalAccessException | IllegalArgumentException | InstantiationException ex) {
            LOGGER.error("Exception: ", ex);
        }

        return null;
    }

    /*
     * Return the class name of the AWS signer.
     */
    private String toSignerClassName(String signVersion) {
        return String.format("%s.AWS%sSign", SIGNER_PACKAGE, signVersion);
    }
}
