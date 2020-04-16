/*
 * COPYRIGHT 2019 SEAGATE LLC
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
 * Original author:  Ajinkya Dhumal <ajinkya.dhumal@seagate.com>
 * Original creation date: 08-Jul-2019
 */

package com.seagates3.acl;

import static org.junit.Assert.assertEquals;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

import javax.xml.parsers.ParserConfigurationException;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;
import org.xml.sax.SAXException;

import com.seagates3.exception.BadRequestException;
import com.seagates3.exception.DataAccessException;
import com.seagates3.exception.GrantListFullException;
import com.seagates3.model.Account;
import com.seagates3.model.Requestor;
import com.seagates3.util.BinaryUtil;

public
class ACLAuthorizerTest {

  static Requestor requestor = new Requestor();
  static Map<String, String> requestBody = new HashMap<String, String>();
  static String aclXmlPath = "../resources/defaultAclTemplate.xml";
  static AccessControlPolicy defaultAcp;
  static AccessControlPolicy acp;
  static AccessControlList acl;
  static String requestUri = "/seagatebucket-aj01/dir-1/abc1";
  static String requestUriAcl = "/seagatebucket-aj01/dir-1/abc1?acl";
  static String acpXmlString =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>" +
      "<AccessControlPolicy " +
      "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">" + " <Owner>" +
      "  <ID>id6</ID>" + "  <DisplayName>S3test</DisplayName>" + " </Owner>" +
      "  <Owner>" + "  <ID>Int1</ID>" + "  <DisplayName>String</DisplayName>" +
      " </Owner>" + " <AccessControlList>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id1</ID>" +
      "    <DisplayName>user1</DisplayName>" + "   </Grantee>" +
      "   <Permission>READ</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id2</ID>" +
      "    <DisplayName>user2</DisplayName>" + "   </Grantee>" +
      "   <Permission>READ_ACP</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id3</ID>" +
      "    <DisplayName>user3</DisplayName>" + "   </Grantee>" +
      "   <Permission>WRITE</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id4</ID>" +
      "    <DisplayName>user4</DisplayName>" + "   </Grantee>" +
      "   <Permission>WRITE_ACP</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id5</ID>" +
      "    <DisplayName>user5</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id6</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>READ</Permission>" + "  </Grant>" + "   <Grant>\r\n" +
      "      <Grantee " +
      "   xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
      " xsi:type=\"AmazonCustomerByEmail\">\r\n" +
      "        <EmailAddress>xyz@seagate.com</EmailAddress>\r\n" +
      "      </Grantee>\r\n" + "      <Permission>READ</Permission>\r\n" +
      "      </Grant>" + " </AccessControlList>" + "</AccessControlPolicy>";

  static String invalidAcpXmlString =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>" +
      "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">" + " <Owner>" +
      "  <ID>id6</ID>" + "  <DisplayName>S3test</DisplayName>" + " </Owner>" +
      "  <Owner>" + "  <ID>Int1</ID>" + "  <DisplayName>String</DisplayName>" +
      " </Owner>" + " <AccessControlList>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id1</ID>" +
      "    <DisplayName>user1</DisplayName>" + "   </Grantee>" +
      "   <Permission>READ</Permission>" + "  </Grant>" +
      " </AccessControlList>" + "</AccessControlPolicy>";

  @BeforeClass public static void setUpBeforeClass() throws Exception {
    acp = new AccessControlPolicy(acpXmlString);
  }

  @Before public void setUp() throws Exception {
    requestBody.put("Auth-ACL", BinaryUtil.encodeToBase64String(acpXmlString));
  }

  // READ permission should grant GET access.
  @Test public void testIsAuthorized_success_READ_For_READ_Grant()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "GET");
    requestBody.put("ClientAbsoluteUri", requestUri);
    Account account = new Account();
    account.setCanonicalId("id1");
    requestor.setAccount(account);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(true, result);
  }

  // READ permission should grant GET access for grantee type=email
  @Test public void testIsAuthorized_success_READ_For_READ_Grant_email()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "GET");
    requestBody.put("ClientAbsoluteUri", requestUri);
    Account account = new Account();
    account.setCanonicalId("id1");
    account.setEmail("xyz@seagate.com");
    requestor.setAccount(account);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(true, result);
  }

  // Write operation should be disallowed for READ grant
  @Test public void testIsAuthorized_Restrict_WRITE_For_READ_Grant()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "PUT");
    requestBody.put("ClientAbsoluteUri", requestUri);
    Account account = new Account();
    account.setCanonicalId("id1");
    requestor.setAccount(account);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(false, result);
  }

  // READ_ACP operation should be disallowed for READ grant
  @Test public void testIsAuthorized_Restrict_READ_ACP_For_READ_Grant()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "GET");
    requestBody.put("ClientAbsoluteUri", requestUriAcl);
    Account account = new Account();
    account.setCanonicalId("id1");
    requestor.setAccount(account);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(false, result);
  }

  // READ_ACP operation should be disallowed for READ grant for grantee
  // type=email
  @Test public void testIsAuthorized_restrict_READ_ACP_For_READ_Grant_email()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "GET");
    requestBody.put("ClientAbsoluteUri", requestUriAcl);
    Account account = new Account();
    account.setCanonicalId("id1");
    account.setEmail("xyz@seagate.com");
    requestor.setAccount(account);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(false, result);
  }

  // WRITE_ACP operation should be disallowed for READ grant
  @Test public void testIsAuthorized_Restrict_WRITE_ACP_For_READ_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "GET");
    requestBody.put("ClientAbsoluteUri", requestUriAcl);
    Account account = new Account();
    account.setCanonicalId("id1");
    requestor.setAccount(account);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(false, result);
  }

  // WRITE_ACP operation should be disallowed for READ grant for grantee
  // type=email
  @Test public void testIsAuthorized_restrict_WRITE_ACP_For_READ_Grant_email()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "PUT");
    requestBody.put("ClientAbsoluteUri", requestUriAcl);
    Account account = new Account();
    account.setCanonicalId("id1");
    account.setEmail("xyz@seagate.com");
    requestor.setAccount(account);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(false, result);
  }

  // READ_ACP operation should be allowed for READ_ACP grant
  @Test public void testIsAuthorized_SUCCESS_READ_ACP_For_READ_ACP_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "GET");
    requestBody.put("ClientAbsoluteUri", requestUriAcl);
    Account acc = new Account();
    acc.setCanonicalId("id2");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(true, result);
  }

  // READ operation should be disallowed for READ_ACP grant
  @Test public void testIsAuthorized_Restrict_READ_For_READ_ACP_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "GET");
    requestBody.put("ClientAbsoluteUri", requestUri);
    Account acc = new Account();
    acc.setCanonicalId("id2");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(false, result);
  }

  // WRITE operation should be disallowed for READ_ACP grant
  @Test public void testIsAuthorized_Restrict_WRITE_For_READ_ACP_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "PUT");
    requestBody.put("ClientAbsoluteUri", requestUri);
    Account acc = new Account();
    acc.setCanonicalId("id2");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(false, result);
  }

  // WRITE_ACP operation should be disallowed for READ_ACP grant
  @Test public void testIsAuthorized_Restrict_WRITE_ACP_For_READ_ACP_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "PUT");
    requestBody.put("ClientAbsoluteUri", requestUriAcl);
    Account acc = new Account();
    acc.setCanonicalId("id2");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(false, result);
  }

  // WRITE operation should be allowed for WRITE grant
  @Test public void testIsAuthorized_Success_WRITE_For_WRITE_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "PUT");
    requestBody.put("ClientAbsoluteUri", requestUri);
    Account acc = new Account();
    acc.setCanonicalId("id3");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(true, result);
  }

  // READ operation should be disallowed for WRITE grant
  @Test public void testIsAuthorized_Restrict_READ_For_WRITE_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "GET");
    requestBody.put("ClientAbsoluteUri", requestUri);
    Account acc = new Account();
    acc.setCanonicalId("id3");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(false, result);
  }

  // READ_ACP operation should be disallowed for WRITE grant
  @Test public void testIsAuthorized_Restrict_READ_ACP_For_WRITE_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "GET");
    requestBody.put("ClientAbsoluteUri", requestUriAcl);
    Account acc = new Account();
    acc.setCanonicalId("id3");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(false, result);
  }

  // WRITE_ACP operation should be disallowed for WRITE grant
  @Test public void testIsAuthorized_Restrict_WRITE_ACP_For_WRITE_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "PUT");
    requestBody.put("ClientAbsoluteUri", requestUriAcl);
    Account acc = new Account();
    acc.setCanonicalId("id3");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(false, result);
  }

  // WRITE_ACP operation should be allowed for WRITE_ACP grant
  @Test public void testIsAuthorized_Success_WRITE_ACP_For_WRITE_ACP_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "PUT");
    requestBody.put("ClientAbsoluteUri", requestUriAcl);
    Account acc = new Account();
    acc.setCanonicalId("id4");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(true, result);
  }

  // READ operation should be disallowed for WRITE_ACP grant
  @Test public void testIsAuthorized_Restrict_READ_For_WRITE_ACP_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "GET");
    requestBody.put("ClientAbsoluteUri", requestUri);
    Account acc = new Account();
    acc.setCanonicalId("id4");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(false, result);
  }

  // READ_ACP operation should be disallowed for WRITE_ACP grant
  @Test public void testIsAuthorized_Restrict_READ_ACP_For_WRITE_ACP_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "GET");
    requestBody.put("ClientAbsoluteUri", requestUriAcl);
    Account acc = new Account();
    acc.setCanonicalId("id4");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(false, result);
  }

  // WRITE operation should be disallowed for WRITE_ACP grant
  @Test public void testIsAuthorized_Restrict_WRITE_For_WRITE_ACP_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "PUT");
    requestBody.put("ClientAbsoluteUri", requestUri);
    Account acc = new Account();
    acc.setCanonicalId("id4");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(false, result);
  }

  // READ operation should be allowed for FULL_CONTROL grant
  @Test public void testIsAuthorized_SUCCESS_READ_For_FULL_CONTROL_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "GET");
    requestBody.put("ClientAbsoluteUri", requestUri);
    Account acc = new Account();
    acc.setCanonicalId("id5");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(true, result);
  }

  // READ_ACP operation should be allowed for FULL_CONTROL grant
  @Test public void testIsAuthorized_SUCCESS_READ_ACP_For_FULL_CONTROL_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "GET");
    requestBody.put("ClientAbsoluteUri", requestUriAcl);
    Account acc = new Account();
    acc.setCanonicalId("id5");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(true, result);
  }

  // WRITE operation should be allowed for FULL_CONTROL grant
  @Test public void testIsAuthorized_SUCCESS_WRITE_For_FULL_CONTROL_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "PUT");
    requestBody.put("ClientAbsoluteUri", requestUri);
    Account acc = new Account();
    acc.setCanonicalId("id5");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(true, result);
  }

  // WRITE_ACP operation should be allowed for FULL_CONTROL grant
  @Test public void testIsAuthorized_SUCCESS_WRITE_ACP_For_FULL_CONTROL_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "PUT");
    requestBody.put("ClientAbsoluteUri", requestUriAcl);
    Account acc = new Account();
    acc.setCanonicalId("id5");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(true, result);
  }

  // READ operation with HEAD HTTP method should be allowed for FULL_CONTROL
  // grant
  @Test public void testIsAuthorized_SUCCESS_READ_HEAD_For_FULL_CONTROL_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "HEAD");
    requestBody.put("ClientAbsoluteUri", requestUri);
    Account acc = new Account();
    acc.setCanonicalId("id5");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(true, result);
  }

  // READ_ACP operation with HEAD HTTP method should be allowed for FULL_CONTROL
  // grant
  @Test public void
  testIsAuthorized_SUCCESS_READ_ACP_HEAD_For_FULL_CONTROL_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "HEAD");
    requestBody.put("ClientAbsoluteUri", requestUriAcl);
    Account acc = new Account();
    acc.setCanonicalId("id5");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(true, result);
  }

  // WRITE operation with DELETE HTTP method should be allowed for FULL_CONTROL
  // grant
  @Test public void
  testIsAuthorized_SUCCESS_WRITE_DELETE_For_FULL_CONTROL_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "DELETE");
    requestBody.put("ClientAbsoluteUri", requestUri);
    Account acc = new Account();
    acc.setCanonicalId("id5");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(true, result);
  }

  // WRITE_ACP operation with DELETE HTTP method should be allowed for
  // FULL_CONTROL grant
  @Test public void
  testIsAuthorized_SUCCESS_WRITE_ACP_DELETE_For_FULL_CONTROL_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "DELETE");
    requestBody.put("ClientAbsoluteUri", requestUriAcl);
    Account acc = new Account();
    acc.setCanonicalId("id5");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(true, result);
  }

  // WRITE operation with POST HTTP method should be allowed for FULL_CONTROL
  // grant
  @Test public void testIsAuthorized_SUCCESS_WRITE_POST_For_FULL_CONTROL_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "POST");
    requestBody.put("ClientAbsoluteUri", requestUri);
    Account acc = new Account();
    acc.setCanonicalId("id5");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(true, result);
  }

  // WRITE_ACP operation with POST HTTP method should be allowed for
  // FULL_CONTROL grant
  @Test public void
  testIsAuthorized_SUCCESS_WRITE_ACP_POST_For_FULL_CONTROL_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "POST");
    requestBody.put("ClientAbsoluteUri", requestUriAcl);
    Account acc = new Account();
    acc.setCanonicalId("id5");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(true, result);
  }

  // READ operation with HEAD HTTP method should be allowed for READ grant
  @Test public void testIsAuthorized_SUCCESS_READ_HEAD_For_READ_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "HEAD");
    requestBody.put("ClientAbsoluteUri", requestUri);
    Account acc = new Account();
    acc.setCanonicalId("id1");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(true, result);
  }

  // READ_ACP operation with HEAD HTTP method should be allowed for READ_ACP
  // grant
  @Test public void testIsAuthorized_SUCCESS_READ_ACP_HEAD_For_READ_ACP_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "HEAD");
    requestBody.put("ClientAbsoluteUri", requestUriAcl);
    Account acc = new Account();
    acc.setCanonicalId("id2");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(true, result);
  }

  // WRITE operation with DELETE HTTP method should be allowed for WRITE grant
  @Test public void testIsAuthorized_SUCCESS_WRITE_DELETE_For_WRITE_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "DELETE");
    requestBody.put("ClientAbsoluteUri", requestUri);
    Account acc = new Account();
    acc.setCanonicalId("id3");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(true, result);
  }

  // WRITE_ACP operation with DELETE HTTP method should be allowed for WRITE_ACP
  // grant
  @Test public void
  testIsAuthorized_SUCCESS_WRITE_ACP_DELETE_For_WRITE_ACP_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "DELETE");
    requestBody.put("ClientAbsoluteUri", requestUriAcl);
    Account acc = new Account();
    acc.setCanonicalId("id4");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(true, result);
  }

  // WRITE operation with POST HTTP method should be allowed for WRITE grant
  @Test public void testIsAuthorized_SUCCESS_WRITE_POST_For_WRITE_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "POST");
    requestBody.put("ClientAbsoluteUri", requestUri);
    Account acc = new Account();
    acc.setCanonicalId("id3");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(true, result);
  }

  // WRITE_ACP operation with POST HTTP method should be allowed for WRITE_ACP
  // grant
  @Test public void
  testIsAuthorized_SUCCESS_WRITE_ACP_POST_For_WRITE_ACP_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "POST");
    requestBody.put("ClientAbsoluteUri", requestUriAcl);
    Account acc = new Account();
    acc.setCanonicalId("id4");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(true, result);
  }

  // WRITE operation with PUT HTTP method should be allowed for FULL_CONTROL
  // grant
  @Test public void testIsAuthorized_SUCCESS_WRITE_PUT_For_FULL_CONTROL_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "PUT");
    requestBody.put("ClientAbsoluteUri", requestUri);
    Account acc = new Account();
    acc.setCanonicalId("id5");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(true, result);
  }

  // READ operation with HEAD HTTP method should be disallowed for READ_ACP
  // grant
  @Test public void testIsAuthorized_Restrict_READ_HEAD_For_READ_ACP_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "HEAD");
    requestBody.put("ClientAbsoluteUri", requestUri);
    Account acc = new Account();
    acc.setCanonicalId("id2");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(false, result);
  }

  // READ_ACP operation with HEAD HTTP method should be disallowed for READ
  // grant
  @Test public void testIsAuthorized_Restrict_READ_ACP_HEAD_For_READ_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "HEAD");
    requestBody.put("ClientAbsoluteUri", requestUriAcl);
    Account acc = new Account();
    acc.setCanonicalId("id1");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(false, result);
  }

  // WRITE operation with DELETE HTTP method should be disallowed for READ grant
  @Test public void testIsAuthorized_Restrict_WRITE_DELETE_For_READ_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "DELETE");
    requestBody.put("ClientAbsoluteUri", requestUri);
    Account acc = new Account();
    acc.setCanonicalId("id1");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(false, result);
  }

  // WRITE_ACP operation with DELETE HTTP method should be disallowed for WRITE
  // grant
  @Test public void testIsAuthorized_Restrict_WRITE_ACP_DELETE_For_WRITE_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "DELETE");
    requestBody.put("ClientAbsoluteUri", requestUriAcl);
    Account acc = new Account();
    acc.setCanonicalId("id3");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(false, result);
  }

  // WRITE operation with POST HTTP method should be disallowed for WRITE_ACP
  // grant
  @Test public void testIsAuthorized_Restrict_WRITE_POST_For_WRITE_ACP_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "POST");
    requestBody.put("ClientAbsoluteUri", requestUri);
    Account acc = new Account();
    acc.setCanonicalId("id4");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(false, result);
  }

  // WRITE_ACP operation with POST HTTP method should be disallowed for WRITE
  // grant
  @Test public void testIsAuthorized_Restrict_WRITE_ACP_POST_For_WRITE_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "POST");
    requestBody.put("ClientAbsoluteUri", requestUriAcl);
    Account acc = new Account();
    acc.setCanonicalId("id3");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(false, result);
  }

  // WRITE operation with PUT HTTP method should be disallowed for READ grant
  @Test public void testIsAuthorized_Restrict_WRITE_POST_For_READ_GRANT()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "POST");
    requestBody.put("ClientAbsoluteUri", requestUri);
    Account acc = new Account();
    acc.setCanonicalId("id1");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(false, result);
  }

  // Throw BadRequestException for null Method
  @Test(
      expected =
          BadRequestException
              .class) public void testIsAuthorized_BadRequestException_Null_Method()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", null);
    requestBody.put("ClientAbsoluteUri", requestUri);
    Account acc = new Account();
    acc.setCanonicalId("id1");
    requestor.setAccount(acc);
    new ACLAuthorizer().isAuthorized(requestor, requestBody);
  }

  // Throw BadRequestException for empty Method
  @Test(
      expected =
          BadRequestException
              .class) public void testIsAuthorized_BadRequestException_Empty_Method()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "");
    requestBody.put("ClientAbsoluteUri", requestUri);
    Account acc = new Account();
    acc.setCanonicalId("id1");
    requestor.setAccount(acc);
    new ACLAuthorizer().isAuthorized(requestor, requestBody);
  }

  // Throw BadRequestException for invalid HTTP Method
  @Test(
      expected =
          BadRequestException
              .class) public void testIsAuthorized_BadRequestException_Invalid_Method()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "INVALID");
    requestBody.put("ClientAbsoluteUri", requestUri);
    Account acc = new Account();
    acc.setCanonicalId("id1");
    requestor.setAccount(acc);
    new ACLAuthorizer().isAuthorized(requestor, requestBody);
  }

  // Throw BadRequestException for empty / null URI
  @Test(
      expected =
          BadRequestException
              .class) public void testIsAuthorized_BadRequestException_NULL_URI()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "GET");
    requestBody.put("ClientAbsoluteUri", "");
    Account acc = new Account();
    acc.setCanonicalId("id1");
    requestor.setAccount(acc);
    new ACLAuthorizer().isAuthorized(requestor, requestBody);
  }

  // Unauthorize if user not present in ACL
  @Test public void testIsAuthorized_Restrict_Null_Grant()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "POST");
    requestBody.put("ClientAbsoluteUri", requestUri);
    Account acc = new Account();
    acc.setCanonicalId("id10");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(false, result);
  }

  /**
   * Authorize if the user requesting get-acl on a resource is its owner
   * and has only READ permission on that resource
   * @throws ParserConfigurationException
   * @throws SAXException
   * @throws IOException
   * @throws BadRequestException
   */
  @Test public void testIsAuthorized_Success_GET_ACL_For_READ_GRANT_ToOwner()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "GET");
    requestBody.put("ClientAbsoluteUri", requestUriAcl);
    Account acc = new Account();
    acc.setCanonicalId("id6");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(true, result);
  }

  /**
   * Authorize if the user requesting put-acl on a resource is its owner
   * and has only READ permission on that resource
   * @throws ParserConfigurationException
   * @throws SAXException
   * @throws IOException
   * @throws BadRequestException
   */
  @Test public void testIsAuthorized_Success_PUT_ACL_For_READ_GRANT_ToOwner()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "PUT");
    requestBody.put("ClientAbsoluteUri", requestUriAcl);
    Account acc = new Account();
    acc.setCanonicalId("id6");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(true, result);
  }

  /**
   * Authorize if the user requesting GET on a resource is its owner
   * and has READ permission on that resource
   * @throws ParserConfigurationException
   * @throws SAXException
   * @throws IOException
   * @throws BadRequestException
   */
  @Test public void testIsAuthorized_Success_GET_For_READ_GRANT_ToOwner()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "GET");
    requestBody.put("ClientAbsoluteUri", requestUri);
    requestBody.put("S3Action", "");

    Account acc = new Account();
    acc.setCanonicalId("id6");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(true, result);
  }

  /**
   * Unauthorize if the user requesting 'PUT' on a resource is its owner
   * and has only READ permission on that resource
   * @throws ParserConfigurationException
   * @throws SAXException
   * @throws IOException
   * @throws BadRequestException
   */
  @Test public void testIsAuthorized_Restrict_PUT_For_READ_GRANT_ToOwner()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "PUT");
    requestBody.put("ClientAbsoluteUri", requestUri);
    Account acc = new Account();
    acc.setCanonicalId("id6");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(false, result);
  }

  /**
   * Unauthorize if the user requesting 'DELETE' on a resource is its owner
   * and has only READ permission on that resource
   * @throws ParserConfigurationException
   * @throws SAXException
   * @throws IOException
   * @throws BadRequestException
   */
  @Test public void testIsAuthorized_Restrict_DELETE_For_READ_GRANT_ToOwner()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "DELETE");
    requestBody.put("ClientAbsoluteUri", requestUri);
    Account acc = new Account();
    acc.setCanonicalId("id6");
    requestor.setAccount(acc);
    boolean result = new ACLAuthorizer().isAuthorized(requestor, requestBody);
    assertEquals(false, result);
  }

  // Throw BadRequestException for null URI
  @Test(
      expected =
          BadRequestException
              .class) public void testIsAuthorized_BadRequestException_NULL_ACP()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "GET");
    requestBody.put("Auth-ACL", null);
    Account acc = new Account();
    acc.setCanonicalId("id1");
    requestor.setAccount(acc);
    new ACLAuthorizer().isAuthorized(requestor, requestBody);
  }

  // Throw BadRequestException for empty URI
  @Test(
      expected =
          BadRequestException
              .class) public void testIsAuthorized_BadRequestException_Empty_ACP()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "GET");
    requestBody.put("Auth-ACL", "");
    Account acc = new Account();
    acc.setCanonicalId("id1");
    requestor.setAccount(acc);
    new ACLAuthorizer().isAuthorized(requestor, requestBody);
  }

  // Throw BadRequestException for empty URI
  @Test(
      expected =
          SAXException
              .class) public void testIsAuthorized_SAXException_InvalidSpace_ACP()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "GET");
    requestBody.put("Auth-ACL", " ");
    Account acc = new Account();
    acc.setCanonicalId("id1");
    requestor.setAccount(acc);
    new ACLAuthorizer().isAuthorized(requestor, requestBody);
  }

  // Throw BadRequestException for empty URI
  @Test(expected =
            SAXException
                .class) public void testIsAuthorized_SAXException_Invalid_ACP()
      throws ParserConfigurationException,
      SAXException, IOException, BadRequestException, GrantListFullException,
      DataAccessException {
    acl = acp.getAccessControlList();
    requestBody.put("Method", "GET");
    requestBody.put("Auth-ACL",
                    BinaryUtil.encodeToBase64String(invalidAcpXmlString));
    Account acc = new Account();
    acc.setCanonicalId("id1");
    requestor.setAccount(acc);
    new ACLAuthorizer().isAuthorized(requestor, requestBody);
  }
}
