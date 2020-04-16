/*
 * COPYRIGHT 2018 SEAGATE LLC
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
 * Original author: Preeti Kamble <preeti.kamble@seagate.com>
 * Original creation date: 10-July-2018
 */

package com.seagates3.authorization;

import com.seagates3.authorization.IAMApiAuthorizer;
import com.seagates3.exception.InvalidUserException;
import com.seagates3.model.Account;
import com.seagates3.model.Requestor;
import org.junit.rules.ExpectedException;
import org.junit.Rule;
import org.junit.Test;
import static org.junit.Assert.*;
import java.util.Map;
import java.util.TreeMap;

public class IAMApiAuthorizerTest {

    /*
     *  Set up for tests
     */
    @Rule
    public ExpectedException exception = ExpectedException.none();

    private Map<String, String> requestBody = new TreeMap<>();
    private Requestor requestor = new Requestor();

    /*
     * Test for the scenario:
     *
     * When root user's access key and secret key are given,
     * validateIfUserCanPerformAction() function should return true.
     */
    @Test
    public void validateIfUserCanPerformAction_Positive_Test() {

        IAMApiAuthorizer iamApiAuthorizer  = new IAMApiAuthorizer();
        requestor.setName("root");
        requestBody.put("UserName", "usr2");;

        assertEquals(true,
                iamApiAuthorizer.validateIfUserCanPerformAction(requestor,
                requestBody));
    }

    /*
     * Test for the scenario:
     *
     * When access key and secret key belong to non root user(i.e. usr1) and
     * attempting to perform operation on another user(i.e. usr2),
     * validateIfUserCanPerformAction() function should return false.
     */
    @Test
    public void validateIfUserCanPerformAction_Negative_Test() {

        IAMApiAuthorizer iamApiAuthorizer  = new IAMApiAuthorizer();
        requestor.setName("usr1");
        requestBody.put("UserName", "usr2");

        assertEquals(false,
                iamApiAuthorizer.validateIfUserCanPerformAction(requestor,
                requestBody));
    }

    /*
     * Test for the scenario:
     *
     * When access key and secret key belong to non root user(i.e. usr1) and
     * attempting to perform operation on another user(i.e. usr2),
     * authorize() function should throw InvalidUserException.
     *
     */
    @Test
    public void authorize_ThrowsInvalidUserExceptionTest() throws
        InvalidUserException{

        IAMApiAuthorizer iamApiAuthorizer  = new IAMApiAuthorizer();
        requestor.setName("usr1");
        requestBody.put("UserName", "usr2");

        exception.expect(InvalidUserException.class);
        iamApiAuthorizer.authorize(requestor, requestBody);
    }

    /*
     * Test for the scenario:
     *
     * When root user's access key and secret key are given,
     * authorize() function should pass.
     */
    @Test
    public void authorize_DoesNotThrowInvalidUserExceptionTest1() throws
        InvalidUserException{

        IAMApiAuthorizer iamApiAuthorizer  = new IAMApiAuthorizer();
        requestor.setName("root");
        requestBody.put("UserName", "usr2");

        iamApiAuthorizer.authorize(requestor, requestBody);
    }

    /*
     * Test for the scenario:
     *
     * When non user's access key and secret key are given and
     * attempting to perform operation on self,
     * authorize() function should pass.
     */
    @Test
    public void authorize_DoesNotThrowInvalidUserExceptionTest2() throws
        InvalidUserException{

        IAMApiAuthorizer iamApiAuthorizer  = new IAMApiAuthorizer();
        requestor.setName("usr2");
        requestBody.put("UserName", "usr2");

        iamApiAuthorizer.authorize(requestor, requestBody);
    }

    /**
    * Below test will check invalid user authorization
    * @throws InvalidUserException
    */
    @Test public void authorizeRootUserExceptionTest()
        throws InvalidUserException {
      IAMApiAuthorizer iamApiAuthorizer = new IAMApiAuthorizer();
      requestor.setName("usr1");
      requestBody.put("UserName", "user1");
      exception.expect(InvalidUserException.class);
      iamApiAuthorizer.authorizeRootUser(requestor, requestBody);
    }

    /**
    * Below test will check different user authorization
    * @throws InvalidUserException
    */
    @Test public void authorizeRootUserDifferentUserTest()
        throws InvalidUserException {
      IAMApiAuthorizer iamApiAuthorizer = new IAMApiAuthorizer();
      Account account = new Account();
      account.setName("user1");
      requestor.setAccount(account);
      requestor.setName("root");
      requestBody.put("AccountName", "user2");
      exception.expect(InvalidUserException.class);
      iamApiAuthorizer.authorizeRootUser(requestor, requestBody);
    }
    /**
    * Below test will check non root user authorization
    * @throws InvalidUserException
    */
    @Test public void authorizeRootUserNonRootUserTest()
        throws InvalidUserException {
      IAMApiAuthorizer iamApiAuthorizer = new IAMApiAuthorizer();
      Account account = new Account();
      account.setName("user1");
      requestor.setAccount(account);
      requestor.setName("user1");
      requestBody.put("UserName", "user2");
      exception.expect(InvalidUserException.class);
      iamApiAuthorizer.authorizeRootUser(requestor, requestBody);
    }
}
