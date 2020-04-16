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
 * Original creation date: 12-Jan-2017
 */
package com.seagates3.service;

import com.seagates3.authentication.ClientRequestToken;
import com.seagates3.dao.AccessKeyDAO;
import com.seagates3.dao.DAODispatcher;
import com.seagates3.dao.DAOResource;
import com.seagates3.dao.RequestorDAO;
import com.seagates3.exception.DataAccessException;
import com.seagates3.exception.InternalServerException;
import com.seagates3.exception.InvalidAccessKeyException;
import com.seagates3.exception.InvalidRequestorException;
import com.seagates3.model.AccessKey;
import com.seagates3.model.Requestor;
import com.seagates3.perf.S3Perf;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.powermock.api.mockito.mockpolicies.Slf4jMockPolicy;
import org.powermock.core.classloader.annotations.MockPolicy;
import org.powermock.core.classloader.annotations.PowerMockIgnore;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;
import org.powermock.reflect.internal.WhiteboxImpl;
import org.slf4j.LoggerFactory;

import java.util.Map;

import static org.hamcrest.core.StringContains.containsString;
import static org.junit.Assert.*;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.powermock.api.mockito.PowerMockito.mockStatic;
import static org.powermock.api.mockito.PowerMockito.*;

@RunWith(PowerMockRunner.class)
    @PrepareForTest({RequestorService.class, DAODispatcher.class,
                     LoggerFactory.class,    S3Perf.class})
    @PowerMockIgnore("javax.management.*")
    @MockPolicy(Slf4jMockPolicy.class) public class RequestorServiceTest {

    private AccessKeyDAO accessKeyDAO;
    private ClientRequestToken clientRequestToken;
    private Requestor requestor;
    private RequestorDAO requestorDAO;
    private S3Perf s3Perf;
    private AccessKey accessKey;

    private final String accessKeyID = "AKIAJTYX36YCKQSAJT7Q";

    @Before
    public void setUp() throws Exception {
        accessKeyDAO = mock(AccessKeyDAO.class);
        mockStatic(DAODispatcher.class);
        when(DAODispatcher.getResourceDAO(DAOResource.ACCESS_KEY))
            .thenReturn(accessKeyDAO);

        clientRequestToken = mock(ClientRequestToken.class);
        doReturn(accessKeyID).when(clientRequestToken).getAccessKeyId();

        s3Perf = mock(S3Perf.class);
        whenNew(S3Perf.class).withNoArguments().thenReturn(s3Perf);
        accessKey = mock(AccessKey.class);
        requestorDAO = mock(RequestorDAO.class);
        requestor = mock(Requestor.class);
    }

    @Test
    public void getRequestorTest() throws Exception {
        when(accessKeyDAO.find(accessKeyID)).thenReturn(accessKey);
        spy(RequestorService.class);
        doReturn(Boolean.TRUE)
            .when(RequestorService.class, "validateAccessKey", accessKey);
        when(DAODispatcher.getResourceDAO(DAOResource.REQUESTOR))
            .thenReturn(requestorDAO);
        when(requestorDAO.find(accessKey)).thenReturn(requestor);
        doReturn(Boolean.TRUE).when(RequestorService.class, "validateRequestor",
                                    requestor, clientRequestToken);

        Requestor result = RequestorService.getRequestor(clientRequestToken);

        assertEquals(requestor, result);
        verify(accessKeyDAO).find(accessKeyID);
        verify(requestorDAO).find(accessKey);
    }

    @Test(
        expected =
            InternalServerException
                .class) public void getRequestorTest_RequestorFindShouldThrowException()
        throws Exception {
        when(accessKeyDAO.find(accessKeyID)).thenReturn(accessKey);
        spy(RequestorService.class);
        doReturn(Boolean.TRUE)
            .when(RequestorService.class, "validateAccessKey", accessKey);
        when(DAODispatcher.getResourceDAO(DAOResource.REQUESTOR))
            .thenReturn(requestorDAO);
        when(requestorDAO.find(accessKey)).thenThrow(DataAccessException.class);

        RequestorService.getRequestor(clientRequestToken);
    }

    @Test(
        expected =
            InternalServerException
                .class) public void getRequestorTest_AccessKeyFindShouldThrowException()
        throws Exception {
      when(accessKeyDAO.find(accessKeyID)).thenThrow(DataAccessException.class);

      RequestorService.getRequestor(clientRequestToken);
    }

    @Test
    public void validateAccessKeyTest() throws Exception {
        when(accessKey.exists()).thenReturn(Boolean.TRUE);
        when(accessKey.isAccessKeyActive()).thenReturn(Boolean.TRUE);

        Boolean result = WhiteboxImpl.invokeMethod(
            RequestorService.class, "validateAccessKey", accessKey);

        assertTrue(result);
    }

    @Test public void validateAccessKeyTest_AccessKeyDoesNotExist()
        throws Exception {
        try {
          WhiteboxImpl.invokeMethod(RequestorService.class, "validateAccessKey",
                                    accessKey);
            fail("Should throw InvalidAccessKeyException");
        } catch (InvalidAccessKeyException e) {
            assertThat(e.getMessage(), containsString("InvalidAccessKeyId"));
        }

        verify(accessKey, times(0)).isAccessKeyActive();
    }

    @Test
    public void validateAccessKeyTest_InactiveAccessKey() throws Exception {
        when(accessKey.exists()).thenReturn(Boolean.TRUE);

        try {
          WhiteboxImpl.invokeMethod(RequestorService.class, "validateAccessKey",
                                    accessKey);
            fail("Should throw InvalidAccessKeyException");
        } catch (InvalidAccessKeyException e) {
            assertThat(e.getMessage(), containsString("InactiveAccessKey"));
        }

        verify(accessKey, times(1)).exists();
        verify(accessKey, times(1)).isAccessKeyActive();
    }

    @Test
    public void validateRequestorTest() throws Exception {
        when(requestor.exists()).thenReturn(Boolean.TRUE);
        when(requestor.isFederatedUser()).thenReturn(Boolean.FALSE);
        when(requestor.getAccesskey()).thenReturn(accessKey);

        Boolean result = WhiteboxImpl.invokeMethod(
            RequestorService.class, "validateRequestor", requestor,
            clientRequestToken);

        assertTrue(result);
    }

    @Test
    public void validateRequestorTest_RequestorDoesntExist() throws Exception {
        try {
          WhiteboxImpl.invokeMethod(RequestorService.class, "validateRequestor",
                                    requestor, clientRequestToken);
            fail("Should throw exception if requestor doesn't exist.");
        } catch (InvalidRequestorException e) {
            assertThat(e.getMessage(), containsString("InternalFailure"));
        }

        verify(requestor, times(0)).isFederatedUser();
    }

    @Test public void validateRequestorTest_RequstorIsFederatedUser()
        throws Exception {
        when(requestor.exists()).thenReturn(Boolean.TRUE);
        when(requestor.getAccesskey()).thenReturn(accessKey);
        when(requestor.isFederatedUser()).thenReturn(Boolean.TRUE);
        Map headers = mock(Map.class);
        when(clientRequestToken.getRequestHeaders()).thenReturn(headers);
        when(headers.get("X-Amz-Security-Token"))
            .thenReturn("AAAHVXNlclRrbgfOpSykBAXO7g");
        when(accessKey.getToken()).thenReturn("AAAHVXNlclRrbgfOpSykBAXO7g");
        when(accessKey.getExpiry()).thenReturn("9999-01-10T08:02:47.806-05:00");

        Boolean result = WhiteboxImpl.invokeMethod(
            RequestorService.class, "validateRequestor", requestor,
            clientRequestToken);

        assertTrue(result);
    }

    @Test public void
    validateRequestorTest_RequstorIsFedUser_InvalidClientTokenID()
        throws Exception {
        when(requestor.exists()).thenReturn(Boolean.TRUE);
        when(requestor.getAccesskey()).thenReturn(accessKey);
        when(requestor.isFederatedUser()).thenReturn(Boolean.TRUE);
        Map headers = mock(Map.class);
        when(clientRequestToken.getRequestHeaders()).thenReturn(headers);
        when(headers.get("X-Amz-Security-Token"))
            .thenReturn("AAAHVXNlclRrbgfOpSykBAXO7g");
        when(accessKey.getToken()).thenReturn("INVALID-TOKEN");

        try {
          WhiteboxImpl.invokeMethod(RequestorService.class, "validateRequestor",
                                    requestor, clientRequestToken);
            fail("Should throw InvalidRequestorException if client token id is invalid.");
        } catch (InvalidRequestorException e) {
            assertThat(e.getMessage(), containsString("InvalidClientTokenId"));
        }
    }

    @Test public void
    validateRequestorTest_RequstorIsFedUser_ExpiredCredential()
        throws Exception {
        when(requestor.exists()).thenReturn(Boolean.TRUE);
        when(requestor.getAccesskey()).thenReturn(accessKey);
        when(requestor.isFederatedUser()).thenReturn(Boolean.TRUE);
        Map headers = mock(Map.class);
        when(clientRequestToken.getRequestHeaders()).thenReturn(headers);
        when(headers.get("X-Amz-Security-Token"))
            .thenReturn("AAAHVXNlclRrbgfOpSykBAXO7g");
        when(accessKey.getToken()).thenReturn("AAAHVXNlclRrbgfOpSykBAXO7g");
        when(accessKey.getExpiry()).thenReturn("1970-01-10T08:02:47.806-05:00");

        try {
          WhiteboxImpl.invokeMethod(RequestorService.class, "validateRequestor",
                                    requestor, clientRequestToken);
            fail("Should throw InvalidRequestorException if federated credentials have expired");
        }
        catch (InvalidRequestorException e) {
            assertThat(e.getMessage(), containsString("ExpiredCredential"));
        }
    }
}


