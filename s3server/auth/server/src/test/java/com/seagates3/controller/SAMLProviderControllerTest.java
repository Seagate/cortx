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
 * Original creation date: 12-Jan-2015
 */
package com.seagates3.controller;

import com.seagates3.dao.DAODispatcher;
import com.seagates3.dao.DAOResource;
import com.seagates3.dao.SAMLProviderDAO;
import com.seagates3.exception.DataAccessException;
import com.seagates3.model.Account;
import com.seagates3.model.Requestor;
import com.seagates3.response.ServerResponse;
import com.seagates3.saml.SAMLUtilV2;
import io.netty.handler.codec.http.HttpResponseStatus;
import java.util.Map;
import java.util.TreeMap;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.powermock.api.mockito.PowerMockito;
import org.powermock.core.classloader.annotations.PowerMockIgnore;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;

@RunWith(PowerMockRunner.class)
@PrepareForTest({DAODispatcher.class, SAMLUtilV2.class})
@PowerMockIgnore({"javax.management.*"})
public class SAMLProviderControllerTest {

    private SAMLProviderController samlProviderController;
    private SAMLProviderDAO samlProviderDao;
    private final String ACCOUNT_NAME = "s3test";
    private final String ACCOUNT_ID = "12345";
    private final Account ACCOUNT;

    private final String SAML_METADATA = "Sample metadata";
    private final String UPDATED_SAML_METADATA = "Updated saml metadata";
    private final String SAML_PROVIDER_ARN = "arn:seagate:iam:::s3testprovider";
    private final String PROVIDER_NAME = "s3testprovider";
    private final String ISSUER_NAME = "http://s3test/issuer";
    private final String EXPIRY_DATE = "2016-01-13T10:42:30.000+0530";
    private final String CREATE_DATE = "2015-01-13T10:42:30.000+0530";

    public SAMLProviderControllerTest() {
        ACCOUNT = new Account();
        ACCOUNT.setId(ACCOUNT_ID);
        ACCOUNT.setName(ACCOUNT_NAME);
    }

    /**
     * Create SAMLProvider controller object and mock UserDAO for Create SAML
     * Provider API.
     *
     * @param path User path attribute.
     * @throws Exception
     */
    private void createSAMLProviderController_CreateAPI()
            throws Exception {
        Requestor requestor = new Requestor();
        requestor.setAccount(ACCOUNT);

        Map<String, String> requestBody = new TreeMap<>(
                String.CASE_INSENSITIVE_ORDER);
        requestBody.put("SAMLMetadataDocument", SAML_METADATA);
        requestBody.put("name", PROVIDER_NAME);

        samlProviderDao = Mockito.mock(SAMLProviderDAO.class);

        PowerMockito.doReturn(samlProviderDao).when(DAODispatcher.class,
                "getResourceDAO", DAOResource.SAML_PROVIDER
        );

        samlProviderController = new SAMLProviderController(requestor,
                requestBody);
    }

    /**
     * Create SAMLProvider controller object and mock UserDAO for Delete and
     * List SAML Provider API.
     *
     * @param path User path attribute.
     * @throws Exception
     */
    private void createSAMLProviderController_Delete_List_API()
            throws Exception {
        Requestor requestor = new Requestor();
        requestor.setAccount(ACCOUNT);

        Map<String, String> requestBody = new TreeMap<>(
                String.CASE_INSENSITIVE_ORDER);
        requestBody.put("SAMLProviderArn", SAML_PROVIDER_ARN);

        samlProviderDao = Mockito.mock(SAMLProviderDAO.class);

        PowerMockito.doReturn(samlProviderDao).when(DAODispatcher.class,
                "getResourceDAO", DAOResource.SAML_PROVIDER
        );

        samlProviderController = new SAMLProviderController(requestor,
                requestBody);
    }

    /**
     * Create SAMLProvider controller object and mock UserDAO for Update SAML
     * Provider API.
     *
     * @param path User path attribute.
     * @throws Exception
     */
    private void createSAMLProviderController_UpdateAPI()
            throws Exception {
        Requestor requestor = new Requestor();
        requestor.setAccount(ACCOUNT);

        Map<String, String> requestBody = new TreeMap<>(
                String.CASE_INSENSITIVE_ORDER);
        requestBody.put("SAMLProviderArn", SAML_PROVIDER_ARN);
        requestBody.put("SAMLMetadataDocument", UPDATED_SAML_METADATA);

        samlProviderDao = Mockito.mock(SAMLProviderDAO.class);

        PowerMockito.doReturn(samlProviderDao).when(DAODispatcher.class,
                "getResourceDAO", DAOResource.SAML_PROVIDER
        );

        samlProviderController = new SAMLProviderController(requestor,
                requestBody);
    }

    @Before
    public void setUp() throws Exception {
        PowerMockito.mockStatic(DAODispatcher.class);
        PowerMockito.mockStatic(SAMLUtilV2.class);
    }

    @Test
    public void Create_SAMLProviderSearchFailed_ReturnInternalServerError()
            throws Exception {
        createSAMLProviderController_CreateAPI();

        Mockito.when(samlProviderDao.find(ACCOUNT, "s3testprovider")).thenThrow(
                new DataAccessException("failed to search provider.\n"));

        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<Error><Code>InternalFailure</Code>"
                + "<Message>The request processing has failed because of an "
                + "unknown error, exception or failure.</Message></Error>"
                + "<RequestId>0000</RequestId>"
                + "</ErrorResponse>";

        ServerResponse response = samlProviderController.create();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                response.getResponseStatus());
    }
//
//    @Test
//    public void Create_SAMLProviderExists_ReturnEntityAlreadyExists()
//            throws Exception {
//        createSAMLProviderController_CreateAPI();
//
//        SAMLProvider samlProvider = new SAMLProvider();
//        samlProvider.setAccountId(ACCOUNT_ID);
//        samlProvider.setName(PROVIDER_NAME);
//        samlProvider.setIssuer(ISSUER_NAME);
//        samlProvider.setExpiry(EXPIRY_DATE);
//
//        Mockito.when(samlProviderDao.find("12345", "s3testprovider"))
//                .thenReturn(samlProvider);
//
//        final String expectedResponseBody = "<?xml version=\"1.0\" "
//                + "encoding=\"UTF-8\" standalone=\"no\"?>"
//                + "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
//                + "<Error><Code>EntityAlreadyExists</Code>"
//                + "<Message>The request was rejected because it attempted "
//                + "to create or update a resource that already exists.</Message></Error>"
//                + "<RequestId>0000</RequestId>"
//                + "</ErrorResponse>";
//
//        ServerResponse response = samlProviderController.create();
//
//        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
//        Assert.assertEquals(HttpResponseStatus.CONFLICT,
//                response.getResponseStatus());
//    }
//
//    @Test
//    public void Create_XMLParserExceptionWhileParsing_ReturnInvalidParameterValue()
//            throws Exception {
//        createSAMLProviderController_CreateAPI();
//
//        SAMLProvider samlProvider = new SAMLProvider();
//        samlProvider.setAccountId(ACCOUNT_ID);
//        samlProvider.setName(PROVIDER_NAME);
//
//        Mockito.when(samlProviderDao.find("12345", "s3testprovider"))
//                .thenReturn(samlProvider);
//        PowerMockito.doThrow(new XMLParserException()).when(SAMLUtil.class,
//                "createSAMLProvider", samlProvider, SAML_METADATA
//        );
//
//        final String expectedResponseBody = "<?xml version=\"1.0\" "
//                + "encoding=\"UTF-8\" standalone=\"no\"?>"
//                + "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
//                + "<Error><Code>InvalidParameterValue</Code>"
//                + "<Message>An invalid or out-of-range value was supplied for "
//                + "the input parameter.</Message></Error>"
//                + "<RequestId>0000</RequestId>"
//                + "</ErrorResponse>";
//
//        ServerResponse response = samlProviderController.create();
//
//        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
//        Assert.assertEquals(HttpResponseStatus.BAD_REQUEST,
//                response.getResponseStatus());
//    }
//
//    @Test
//    public void Create_UnmarshallingExceptionWhileParsing_ReturnInvalidParameterValue()
//            throws Exception {
//        createSAMLProviderController_CreateAPI();
//
//        SAMLProvider samlProvider = new SAMLProvider();
//        samlProvider.setAccountId(ACCOUNT_ID);
//        samlProvider.setName(PROVIDER_NAME);
//
//        Mockito.when(samlProviderDao.find("12345", "s3testprovider"))
//                .thenReturn(samlProvider);
//        PowerMockito.doThrow(new UnmarshallingException()).when(SAMLUtil.class,
//                "createSAMLProvider", samlProvider, SAML_METADATA
//        );
//
//        final String expectedResponseBody = "<?xml version=\"1.0\" "
//                + "encoding=\"UTF-8\" standalone=\"no\"?>"
//                + "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
//                + "<Error><Code>InvalidParameterValue</Code>"
//                + "<Message>An invalid or out-of-range value was supplied for "
//                + "the input parameter.</Message></Error>"
//                + "<RequestId>0000</RequestId>"
//                + "</ErrorResponse>";
//
//        ServerResponse response = samlProviderController.create();
//
//        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
//        Assert.assertEquals(HttpResponseStatus.BAD_REQUEST,
//                response.getResponseStatus());
//    }
//
//    @Test
//    public void Create_CertificateExceptionWhileParsing_ReturnInvalidParameterValue()
//            throws Exception {
//        createSAMLProviderController_CreateAPI();
//
//        SAMLProvider samlProvider = new SAMLProvider();
//        samlProvider.setAccountId(ACCOUNT_ID);
//        samlProvider.setName(PROVIDER_NAME);
//
//        Mockito.when(samlProviderDao.find("12345", "s3testprovider"))
//                .thenReturn(samlProvider);
//        PowerMockito.doThrow(new CertificateException()).when(SAMLUtil.class,
//                "createSAMLProvider", samlProvider, SAML_METADATA
//        );
//
//        final String expectedResponseBody = "<?xml version=\"1.0\" "
//                + "encoding=\"UTF-8\" standalone=\"no\"?>"
//                + "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
//                + "<Error><Code>InvalidParameterValue</Code>"
//                + "<Message>An invalid or out-of-range value was supplied for "
//                + "the input parameter.</Message></Error>"
//                + "<RequestId>0000</RequestId>"
//                + "</ErrorResponse>";
//
//        ServerResponse response = samlProviderController.create();
//
//        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
//        Assert.assertEquals(HttpResponseStatus.BAD_REQUEST,
//                response.getResponseStatus());
//    }
//
//    @Test
//    public void Create_ProviderSaveFailed_ReturnInternalServerError()
//            throws Exception {
//        createSAMLProviderController_CreateAPI();
//
//        SAMLProvider samlProvider = new SAMLProvider();
//        samlProvider.setAccountId(ACCOUNT_ID);
//        samlProvider.setName(PROVIDER_NAME);
//
//        Mockito.when(samlProviderDao.find("12345", "s3testprovider"))
//                .thenReturn(samlProvider);
//        PowerMockito.doAnswer(new Answer() {
//
//            @Override
//            public Object answer(InvocationOnMock invocation) throws Throwable {
//                Object[] args = invocation.getArguments();
//                if (args[0] instanceof SAMLProvider) {
//                    SAMLProvider samlProvider = (SAMLProvider) args[0];
//                    samlProvider.setIssuer(ISSUER_NAME);
//                    samlProvider.setExpiry(EXPIRY_DATE);
//                }
//                return null;
//            }
//
//        }).when(SAMLUtil.class, "createSAMLProvider",
//                samlProvider, SAML_METADATA
//        );
//
//        Mockito.doThrow(new DataAccessException("Failed to save provider"))
//                .when(samlProviderDao).save(samlProvider);
//
//        final String expectedResponseBody = "<?xml version=\"1.0\" "
//                + "encoding=\"UTF-8\" standalone=\"no\"?>"
//                + "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
//                + "<Error><Code>InternalFailure</Code>"
//                + "<Message>The request processing has failed because of an "
//                + "unknown error, exception or failure.</Message></Error>"
//                + "<RequestId>0000</RequestId>"
//                + "</ErrorResponse>";
//
//        ServerResponse response = samlProviderController.create();
//
//        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
//        Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
//                response.getResponseStatus());
//    }
//
//    @Test
//    public void Create_ProviderSaved_ReturnCreateResponse()
//            throws Exception {
//        createSAMLProviderController_CreateAPI();
//
//        SAMLProvider samlProvider = new SAMLProvider();
//        samlProvider.setAccountId(ACCOUNT_ID);
//        samlProvider.setName(PROVIDER_NAME);
//
//        Mockito.when(samlProviderDao.find("12345", "s3testprovider"))
//                .thenReturn(samlProvider);
//        PowerMockito.doAnswer(new Answer() {
//
//            @Override
//            public Object answer(InvocationOnMock invocation) throws Throwable {
//                Object[] args = invocation.getArguments();
//                if (args[0] instanceof SAMLProvider) {
//                    SAMLProvider samlProvider = (SAMLProvider) args[0];
//                    samlProvider.setIssuer(ISSUER_NAME);
//                    samlProvider.setExpiry(EXPIRY_DATE);
//                }
//                return null;
//            }
//
//        }).when(SAMLUtil.class, "createSAMLProvider",
//                samlProvider, SAML_METADATA
//        );
//
//        Mockito.doNothing().when(samlProviderDao).save(samlProvider);
//
//        final String expectedResponseBody = "<?xml version=\"1.0\" "
//                + "encoding=\"UTF-8\" standalone=\"no\"?>"
//                + "<CreateSAMLProviderResponse "
//                + "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
//                + "<CreateSAMLProviderResult>"
//                + "<SAMLProviderArn>arn:seagate:iam:::s3testprovider"
//                + "</SAMLProviderArn>"
//                + "</CreateSAMLProviderResult>"
//                + "<ResponseMetadata>"
//                + "<RequestId>0000</RequestId>"
//                + "</ResponseMetadata>"
//                + "</CreateSAMLProviderResponse>";
//
//        ServerResponse response = samlProviderController.create();
//
//        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
//        Assert.assertEquals(HttpResponseStatus.CREATED,
//                response.getResponseStatus());
//    }
//
//    @Test
//    public void Delete_SAMLProviderSearchFailed_ReturnInternalServerError()
//            throws Exception {
//        createSAMLProviderController_Delete_List_API();
//
//        Mockito.when(samlProviderDao.find("12345", "s3testprovider")).thenThrow(
//                new DataAccessException("failed to search provider.\n"));
//
//        final String expectedResponseBody = "<?xml version=\"1.0\" "
//                + "encoding=\"UTF-8\" standalone=\"no\"?>"
//                + "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
//                + "<Error><Code>InternalFailure</Code>"
//                + "<Message>The request processing has failed because of an "
//                + "unknown error, exception or failure.</Message></Error>"
//                + "<RequestId>0000</RequestId>"
//                + "</ErrorResponse>";
//
//        ServerResponse response = samlProviderController.delete();
//        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
//        Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
//                response.getResponseStatus());
//    }
//
//    @Test
//    public void Delete_SAMLProviderDoesNotExist_ReturnNoSuchEntity()
//            throws Exception {
//        createSAMLProviderController_Delete_List_API();
//
//        SAMLProvider samlProvider = new SAMLProvider();
//        samlProvider.setAccountId(ACCOUNT_ID);
//        samlProvider.setName(PROVIDER_NAME);
//
//        Mockito.when(samlProviderDao.find("12345", "s3testprovider"))
//                .thenReturn(samlProvider);
//
//        final String expectedResponseBody = "<?xml version=\"1.0\" "
//                + "encoding=\"UTF-8\" standalone=\"no\"?>"
//                + "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
//                + "<Error><Code>NoSuchEntity</Code>"
//                + "<Message>The request was rejected because it referenced "
//                + "an entity that does not exist. </Message></Error>"
//                + "<RequestId>0000</RequestId>"
//                + "</ErrorResponse>";
//
//        ServerResponse response = samlProviderController.delete();
//        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
//        Assert.assertEquals(HttpResponseStatus.NOT_FOUND,
//                response.getResponseStatus());
//    }
//
//    @Test
//    public void Delete_SAMLProviderDeleted_ReturnDeleteResponse()
//            throws Exception {
//        createSAMLProviderController_Delete_List_API();
//
//        SAMLProvider samlProvider = new SAMLProvider();
//        samlProvider.setAccountId(ACCOUNT_ID);
//        samlProvider.setName(PROVIDER_NAME);
//        samlProvider.setIssuer(ISSUER_NAME);
//        samlProvider.setExpiry(EXPIRY_DATE);
//
//        Mockito.when(samlProviderDao.find("12345", "s3testprovider"))
//                .thenReturn(samlProvider);
//        Mockito.doNothing().when(samlProviderDao).save(samlProvider);
//
//        final String expectedResponseBody = "<?xml version=\"1.0\" "
//                + "encoding=\"UTF-8\" standalone=\"no\"?>"
//                + "<DeleteSAMLProviderResponse "
//                + "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
//                + "<ResponseMetadata>"
//                + "<RequestId>0000</RequestId>"
//                + "</ResponseMetadata>"
//                + "</DeleteSAMLProviderResponse>";
//
//        ServerResponse response = samlProviderController.delete();
//        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
//        Assert.assertEquals(HttpResponseStatus.OK,
//                response.getResponseStatus());
//    }
//
//    @Test
//    public void List_SAMLProviderSearchFailed_ReturnInternalServerError()
//            throws Exception {
//        createSAMLProviderController_Delete_List_API();
//
//        Mockito.when(samlProviderDao.findAll("12345")).thenThrow(
//                new DataAccessException("failed to search provider.\n"));
//
//        final String expectedResponseBody = "<?xml version=\"1.0\" "
//                + "encoding=\"UTF-8\" standalone=\"no\"?>"
//                + "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
//                + "<Error><Code>InternalFailure</Code>"
//                + "<Message>The request processing has failed because of an "
//                + "unknown error, exception or failure.</Message></Error>"
//                + "<RequestId>0000</RequestId>"
//                + "</ErrorResponse>";
//
//        ServerResponse response = samlProviderController.list();
//        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
//        Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
//                response.getResponseStatus());
//    }
//
//    @Test
//    public void List_SAMLProviderSearchEmpty_ReturnSearchResponse()
//            throws Exception {
//        createSAMLProviderController_Delete_List_API();
//        SAMLProvider[] samlProviderList = new SAMLProvider[0];
//
//        Mockito.when(samlProviderDao.findAll("12345"))
//                .thenReturn(samlProviderList);
//
//        final String expectedResponseBody = "<?xml version=\"1.0\" "
//                + "encoding=\"UTF-8\" standalone=\"no\"?>"
//                + "<ListSAMLProvidersResponse "
//                + "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
//                + "<ListSAMLProvidersResult>"
//                + "<SAMLProviderList/>"
//                + "<IsTruncated>false</IsTruncated>"
//                + "</ListSAMLProvidersResult>"
//                + "<ResponseMetadata>"
//                + "<RequestId>0000</RequestId>"
//                + "</ResponseMetadata>"
//                + "</ListSAMLProvidersResponse>";
//
//        ServerResponse response = samlProviderController.list();
//        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
//        Assert.assertEquals(HttpResponseStatus.OK,
//                response.getResponseStatus());
//    }
//
//    @Test
//    public void List_SAMLProvidersFound_ReturnSearchResponse()
//            throws Exception {
//        createSAMLProviderController_Delete_List_API();
//        SAMLProvider[] samlProviderList = new SAMLProvider[1];
//
//        SAMLProvider samlProvider = new SAMLProvider();
//        samlProvider.setAccountId(ACCOUNT_ID);
//        samlProvider.setName(PROVIDER_NAME);
//        samlProvider.setIssuer(ISSUER_NAME);
//        samlProvider.setExpiry(EXPIRY_DATE);
//        samlProvider.setCreateDate(CREATE_DATE);
//
//        samlProviderList[0] = samlProvider;
//        Mockito.when(samlProviderDao.findAll("12345"))
//                .thenReturn(samlProviderList);
//
//        final String expectedResponseBody = "<?xml version=\"1.0\" "
//                + "encoding=\"UTF-8\" standalone=\"no\"?>"
//                + "<ListSAMLProvidersResponse "
//                + "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
//                + "<ListSAMLProvidersResult>"
//                + "<SAMLProviderList>"
//                + "<member>"
//                + "<Arn>arn:seagate:iam:::s3testprovider</Arn>"
//                + "<ValidUntil>2016-01-13T10:42:30.000+0530</ValidUntil>"
//                + "<CreateDate>2015-01-13T10:42:30.000+0530</CreateDate>"
//                + "</member>"
//                + "</SAMLProviderList>"
//                + "<IsTruncated>false</IsTruncated>"
//                + "</ListSAMLProvidersResult>"
//                + "<ResponseMetadata>"
//                + "<RequestId>0000</RequestId>"
//                + "</ResponseMetadata>"
//                + "</ListSAMLProvidersResponse>";
//
//        ServerResponse response = samlProviderController.list();
//        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
//        Assert.assertEquals(HttpResponseStatus.OK,
//                response.getResponseStatus());
//    }
//
//    @Test
//    public void Update_SAMLProviderSearchFailed_ReturnInternalServerError()
//            throws Exception {
//        createSAMLProviderController_UpdateAPI();
//
//        Mockito.when(samlProviderDao.find("12345", "s3testprovider")).thenThrow(
//                new DataAccessException("failed to search provider.\n"));
//
//        final String expectedResponseBody = "<?xml version=\"1.0\" "
//                + "encoding=\"UTF-8\" standalone=\"no\"?>"
//                + "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
//                + "<Error><Code>InternalFailure</Code>"
//                + "<Message>The request processing has failed because of an "
//                + "unknown error, exception or failure.</Message></Error>"
//                + "<RequestId>0000</RequestId>"
//                + "</ErrorResponse>";
//
//        ServerResponse response = samlProviderController.delete();
//        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
//        Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
//                response.getResponseStatus());
//    }
//
//    @Test
//    public void Update_SAMLProviderDoesNotExist_ReturnNoSuchEntity()
//            throws Exception {
//        createSAMLProviderController_UpdateAPI();
//
//        SAMLProvider samlProvider = new SAMLProvider();
//        samlProvider.setAccountId(ACCOUNT_ID);
//        samlProvider.setName(PROVIDER_NAME);
//
//        Mockito.when(samlProviderDao.find("12345", "s3testprovider"))
//                .thenReturn(samlProvider);
//
//        final String expectedResponseBody = "<?xml version=\"1.0\" "
//                + "encoding=\"UTF-8\" standalone=\"no\"?>"
//                + "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
//                + "<Error><Code>NoSuchEntity</Code>"
//                + "<Message>The request was rejected because it referenced "
//                + "an entity that does not exist. </Message></Error>"
//                + "<RequestId>0000</RequestId>"
//                + "</ErrorResponse>";
//
//        ServerResponse response = samlProviderController.update();
//        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
//        Assert.assertEquals(HttpResponseStatus.NOT_FOUND,
//                response.getResponseStatus());
//    }
//
//    @Test
//    public void Update_SAMLProviderUpdated_ReturnOkResponse()
//            throws Exception {
//        createSAMLProviderController_UpdateAPI();
//
//        SAMLProvider samlProvider = new SAMLProvider();
//        samlProvider.setAccountId(ACCOUNT_ID);
//        samlProvider.setName(PROVIDER_NAME);
//        samlProvider.setIssuer(ISSUER_NAME);
//        samlProvider.setExpiry(EXPIRY_DATE);
//
//        Mockito.when(samlProviderDao.find("12345", "s3testprovider"))
//                .thenReturn(samlProvider);
//
//        final String expectedResponseBody = "<?xml version=\"1.0\" "
//                + "encoding=\"UTF-8\" standalone=\"no\"?>"
//                + "<UpdateSAMLProviderResponse "
//                + "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
//                + "<UpdateSAMLProviderResult>"
//                + "<SAMLProviderArn>arn:seagate:iam:::s3testprovider"
//                + "</SAMLProviderArn>"
//                + "</UpdateSAMLProviderResult>"
//                + "<ResponseMetadata>"
//                + "<RequestId>0000</RequestId>"
//                + "</ResponseMetadata>"
//                + "</UpdateSAMLProviderResponse>";
//
//        ServerResponse response = samlProviderController.update();
//        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
//        Assert.assertEquals(HttpResponseStatus.OK,
//                response.getResponseStatus());
//    }
}
