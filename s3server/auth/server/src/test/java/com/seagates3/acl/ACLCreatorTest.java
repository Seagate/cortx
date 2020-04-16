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
 * Original author:  Shalaka Dharap
 * Original creation date: 12-Aug-2019
 */

package com.seagates3.acl;

import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;
import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.exception.InternalServerException;
import com.seagates3.model.Group;
import javax.xml.parsers.ParserConfigurationException;
import javax.xml.transform.TransformerException;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.powermock.core.classloader.annotations.PowerMockIgnore;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;
import org.xml.sax.SAXException;

import com.seagates3.exception.GrantListFullException;
import com.seagates3.model.Account;
import com.seagates3.model.Requestor;
import com.seagates3.util.BinaryUtil;

@PowerMockIgnore({"javax.management.*"}) @RunWith(PowerMockRunner.class)
    @PrepareForTest({Files.class}) public class ACLCreatorTest {

 private
  ACLCreator spyAclCreator;
 private
  Requestor requestor;
 private
  Account account1, account2;
 private
  Map<String, String> requestBody = null;
  String aclXmlPath = null;
  File xmlFile = null;

  @Before public void setup() {
    account1 = new Account();
    account1.setId("1");
    account1.setName("Acc1");
    account1.setCanonicalId("fsdfsfsfdsfd12DD");
    account2 = new Account();
    account2.setId("2");
    account2.setName("Acc2");
    account2.setCanonicalId("wwQQadgfhdfsfsfdsfd12DD");
    requestor = new Requestor();
    requestor.setAccount(account1);
    spyAclCreator = Mockito.spy(new ACLCreator());
    aclXmlPath = "../resources/defaultAclTemplate.xml";
    xmlFile = new File(aclXmlPath);
    AuthServerConfig.authResourceDir = "../resources";
  }

  /**
 *Below will test default acl creation
 *@throws Exception
 **/
  @Test public void testCreateDefaultAcl() throws Exception {
    Mockito.doReturn(
                new String(Files.readAllBytes(Paths.get(xmlFile.getPath()))))
        .when(spyAclCreator)
        .checkAndCreateDefaultAcp();
    String aclXml = spyAclCreator.createDefaultAcl(requestor);
    Assert.assertNotNull(aclXml);
    AccessControlPolicy acp = new AccessControlPolicy(aclXml);
    Assert.assertEquals(acp.getAccessControlList().getGrantList().size(), 1);
    Assert.assertEquals(acp.getAccessControlList()
                            .getGrantList()
                            .get(0)
                            .getGrantee()
                            .getCanonicalId(),
                        "fsdfsfsfdsfd12DD");
    Assert.assertEquals(
        acp.getAccessControlList().getGrantList().get(0).getPermission(),
        "FULL_CONTROL");
    Assert.assertEquals(acp.getOwner().getCanonicalId(), "fsdfsfsfdsfd12DD");
  }

  /**
     * Below will test acl creation with permission header
     *
     * @throws GrantListFullException
     * @throws IOException
     * @throws ParserConfigurationException
     * @throws SAXException
     * @throws TransformerException
  */

  @Test public void testCreateAclFromPermissionHeaders()
      throws GrantListFullException,
      IOException, ParserConfigurationException, SAXException,
      TransformerException {
    Map<String, List<Account>> accountPermissionMap = new HashMap<>();
    Map<String, List<Group>> groupPermissionMap = new HashMap<>();
    List<Account> accountList = new ArrayList<>();
    requestBody = new TreeMap<>();
    accountList.add(account1);
    accountList.add(account2);
    accountPermissionMap.put("x-amz-grant-full-control", accountList);
    Mockito.doReturn(
                new String(Files.readAllBytes(Paths.get(xmlFile.getPath()))))
        .when(spyAclCreator)
        .checkAndCreateDefaultAcp();
    String aclXml = spyAclCreator.createAclFromPermissionHeaders(
        requestor, accountPermissionMap, groupPermissionMap, requestBody);
    Assert.assertNotNull(aclXml);
    AccessControlPolicy acp = new AccessControlPolicy(aclXml);
    Assert.assertEquals(acp.getAccessControlList().getGrantList().size(), 2);
    Assert.assertEquals(acp.getAccessControlList()
                            .getGrantList()
                            .get(0)
                            .getGrantee()
                            .getCanonicalId(),
                        "fsdfsfsfdsfd12DD");
    Assert.assertEquals(
        acp.getAccessControlList().getGrantList().get(0).getPermission(),
        "FULL_CONTROL");
    Assert.assertEquals(acp.getAccessControlList()
                            .getGrantList()
                            .get(1)
                            .getGrantee()
                            .getCanonicalId(),
                        "wwQQadgfhdfsfsfdsfd12DD");
    Assert.assertEquals(
        acp.getAccessControlList().getGrantList().get(1).getPermission(),
        "FULL_CONTROL");
    Assert.assertEquals(acp.getOwner().getCanonicalId(), "fsdfsfsfdsfd12DD");
  }

  /**
     * Below test will pass acl through request body and check the response
     *
     * @throws GrantListFullException
     * @throws IOException
     * @throws ParserConfigurationException
     * @throws SAXException
     * @throws TransformerException
  */

@Test public void testCreateAclFromPermissionHeadersAclInRequestBody()
      throws GrantListFullException,
      IOException, ParserConfigurationException, SAXException,
      TransformerException {
    Map<String, List<Account>> accountPermissionMap = new HashMap<>();
    Map<String, List<Group>> groupPermissionMap = new HashMap<>();
    List<Account> accountList1 = new ArrayList<>();
    requestBody = new TreeMap<>();
    accountList1.add(account1);
    List<Account> accountList2 = new ArrayList<>();
    accountList2.add(account2);
    accountPermissionMap.put("x-amz-grant-full-control", accountList1);
    accountPermissionMap.put("x-amz-grant-write-acp", accountList2);
    String acl =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" +
        "<AccessControlPolicy" +
        " xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">" + "<Owner><ID>" +
        "b103e16d027d24270d8facf37a48b141fd88ac8f43f9f942b91ba1cf1dc33f71" +
        "</ID>" +
        "<DisplayName>kirungeb</DisplayName></Owner><AccessControlList>" +
        "<Grant>" + "<Grantee " +
        "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
        "xsi:type=\"CanonicalUser\"><ID>" +
        "b103e16d027d24270d8facf37a48b141fd88ac8f43f9f942b91ba1cf1dc33f71" +
        "</ID>" + "<DisplayName>kirungeb</DisplayName></Grantee>" +
        "<Permission>FULL_CONTROL</Permission></Grant></AccessControlList>" +
        "</AccessControlPolicy>";

    requestBody.put("Auth-ACL", BinaryUtil.encodeToBase64String(acl));
    String aclXml = spyAclCreator.createAclFromPermissionHeaders(
        requestor, accountPermissionMap, groupPermissionMap, requestBody);
    AccessControlPolicy acp = new AccessControlPolicy(aclXml);
    Assert.assertEquals(acp.getAccessControlList().getGrantList().size(), 2);
    Assert.assertEquals(acp.getAccessControlList()
                            .getGrantList()
                            .get(0)
                            .getGrantee()
                            .getCanonicalId(),
                        "fsdfsfsfdsfd12DD");
    Assert.assertEquals(
        acp.getAccessControlList().getGrantList().get(0).getPermission(),
        "FULL_CONTROL");
    Assert.assertEquals(acp.getAccessControlList()
                            .getGrantList()
                            .get(1)
                            .getGrantee()
                            .getCanonicalId(),
                        "wwQQadgfhdfsfsfdsfd12DD");
    Assert.assertEquals(
        acp.getAccessControlList().getGrantList().get(1).getPermission(),
        "WRITE_ACP");
    Assert.assertNotNull(aclXml);
    Assert.assertEquals(acp.getOwner().getDisplayName(), "Acc1");
  }

  @Test public void testCreateACLFromCannedInput_Success_private()
      throws IOException,
      ParserConfigurationException, SAXException, GrantListFullException,
      TransformerException, InternalServerException {
    String existingACL =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" " +
        "standalone=\"no\"?><AccessControlPolicy " +
        "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">\n" + " <Owner>\n" +
        "  <ID>fsdfsfsfdsfd12DD</ID>\n" +
        "  <DisplayName>Acc1</DisplayName>\n" + " </Owner>\n" +
        " <AccessControlList>\n" + "  <Grant>\n" +
        "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
        "xsi:type=\"CanonicalUser\">\n" + "    <ID>fsdfsfsfdsfd12DD</ID>\n" +
        "    <DisplayName>Acc1</DisplayName>\n" + "   </Grantee>\n" +
        "   <Permission>FULL_CONTROL</Permission>\n" + "  </Grant>\n" +
        "  <Grant>\n" +
        "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
        "xsi:type=\"CanonicalUser\">\n" + "    <ID>fsdfsfsfdsfd</ID>\n" +
        "    <DisplayName>Acc2</DisplayName>\n" + "   </Grantee>\n" +
        "   <Permission>READ</Permission>\n" + "  </Grant>\n" +
        " </AccessControlList>\n" + "</AccessControlPolicy>";
    requestBody = new TreeMap<>();
    requestBody.put("Auth-ACL", BinaryUtil.encodeToBase64String(existingACL));
    requestBody.put("x-amz-acl", "private");
    String resultACL =
        new ACLCreator().createACLFromCannedInput(requestor, requestBody);
    Assert.assertNotNull(resultACL);
    AccessControlPolicy acp = new AccessControlPolicy(resultACL);
    Assert.assertEquals("fsdfsfsfdsfd12DD", acp.getOwner().getCanonicalId());
    Assert.assertEquals("Acc1", acp.getOwner().getDisplayName());
    Assert.assertEquals(1, acp.getAccessControlList().getGrantList().size());
    Assert.assertEquals(
        "fsdfsfsfdsfd12DD",
        acp.getAccessControlList().getGrantList().get(0).grantee.canonicalId);
    Assert.assertEquals(
        "FULL_CONTROL",
        acp.getAccessControlList().getGrantList().get(0).permission);
  }

  @Test public void testCreateACLFromCannedInput_Success_public_read()
      throws IOException,
      ParserConfigurationException, SAXException, GrantListFullException,
      TransformerException, InternalServerException {
    String existingACL =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" " +
        "standalone=\"no\"?><AccessControlPolicy " +
        "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">\n" + " <Owner>\n" +
        "  <ID>fsdfsfsfdsfd12DD</ID>\n" +
        "  <DisplayName>Acc1</DisplayName>\n" + " </Owner>\n" +
        " <AccessControlList>\n" + "  <Grant>\n" +
        "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
        "xsi:type=\"CanonicalUser\">\n" + "    <ID>fsdfsfsfdsfd12DD</ID>\n" +
        "    <DisplayName>Acc1</DisplayName>\n" + "   </Grantee>\n" +
        "   <Permission>FULL_CONTROL</Permission>\n" + "  </Grant>\n" +
        "  <Grant>\n" +
        "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
        "xsi:type=\"CanonicalUser\">\n" + "    <ID>fsdfsfsfdsfd</ID>\n" +
        "    <DisplayName>Acc2</DisplayName>\n" + "   </Grantee>\n" +
        "   <Permission>READ</Permission>\n" + "  </Grant>\n" +
        " </AccessControlList>\n" + "</AccessControlPolicy>";
    requestBody = new TreeMap<>();
    requestBody.put("Auth-ACL", BinaryUtil.encodeToBase64String(existingACL));
    requestBody.put("x-amz-acl", "public-read");
    String resultACL =
        new ACLCreator().createACLFromCannedInput(requestor, requestBody);
    Assert.assertNotNull(resultACL);
    AccessControlPolicy acp = new AccessControlPolicy(resultACL);
    Assert.assertEquals("fsdfsfsfdsfd12DD", acp.getOwner().getCanonicalId());
    Assert.assertEquals("Acc1", acp.getOwner().getDisplayName());
    Assert.assertEquals(2, acp.getAccessControlList().getGrantList().size());
    for (Grant grant : acp.getAccessControlList().getGrantList()) {
      if (grant.grantee.type.equals(Grantee.Types.Group)) {
        Assert.assertEquals(Group.AllUsersURI, grant.grantee.uri);
        Assert.assertEquals("READ", grant.permission);
      } else if (Grantee.Types.CanonicalUser.equals(grant.grantee.type)) {
        Assert.assertEquals("fsdfsfsfdsfd12DD", grant.grantee.canonicalId);
        Assert.assertEquals("FULL_CONTROL", grant.permission);
      } else {
        Assert.fail();
      }
    }
  }

  @Test public void testCreateACLFromCannedInput_Success_public_read_write()
      throws IOException,
      ParserConfigurationException, SAXException, GrantListFullException,
      TransformerException, InternalServerException {
    String existingACL =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" " +
        "standalone=\"no\"?><AccessControlPolicy " +
        "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">\n" + " <Owner>\n" +
        "  <ID>fsdfsfsfdsfd12DD</ID>\n" +
        "  <DisplayName>Acc1</DisplayName>\n" + " </Owner>\n" +
        " <AccessControlList>\n" + "  <Grant>\n" +
        "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
        "xsi:type=\"CanonicalUser\">\n" + "    <ID>fsdfsfsfdsfd12DD</ID>\n" +
        "    <DisplayName>Acc1</DisplayName>\n" + "   </Grantee>\n" +
        "   <Permission>FULL_CONTROL</Permission>\n" + "  </Grant>\n" +
        "  <Grant>\n" +
        "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
        "xsi:type=\"CanonicalUser\">\n" + "    <ID>fsdfsfsfdsfd</ID>\n" +
        "    <DisplayName>Acc2</DisplayName>\n" + "   </Grantee>\n" +
        "   <Permission>READ</Permission>\n" + "  </Grant>\n" +
        " </AccessControlList>\n" + "</AccessControlPolicy>";
    requestBody = new TreeMap<>();
    requestBody.put("Auth-ACL", BinaryUtil.encodeToBase64String(existingACL));
    requestBody.put("x-amz-acl", "public-read-write");
    String resultACL =
        new ACLCreator().createACLFromCannedInput(requestor, requestBody);
    Assert.assertNotNull(resultACL);
    AccessControlPolicy acp = new AccessControlPolicy(resultACL);
    Assert.assertEquals("fsdfsfsfdsfd12DD", acp.getOwner().getCanonicalId());
    Assert.assertEquals("Acc1", acp.getOwner().getDisplayName());
    Assert.assertEquals(3, acp.getAccessControlList().getGrantList().size());
    for (Grant grant : acp.getAccessControlList().getGrantList()) {
      if (grant.grantee.type.equals(Grantee.Types.Group)) {
        Assert.assertEquals(Group.AllUsersURI, grant.grantee.uri);
        Assert.assertTrue("READ".equals(grant.permission) ||
                          "WRITE".equals(grant.permission));
      } else if (Grantee.Types.CanonicalUser.equals(grant.grantee.type)) {
        Assert.assertEquals("fsdfsfsfdsfd12DD", grant.grantee.canonicalId);
        Assert.assertEquals("FULL_CONTROL", grant.permission);
      } else {
        Assert.fail();
      }
    }
  }

  @Test public void testCreateACLFromCannedInput_Success_authenticated_read()
      throws IOException,
      ParserConfigurationException, SAXException, GrantListFullException,
      TransformerException, InternalServerException {
    String existingACL =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" " +
        "standalone=\"no\"?><AccessControlPolicy " +
        "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">\n" + " <Owner>\n" +
        "  <ID>fsdfsfsfdsfd12DD</ID>\n" +
        "  <DisplayName>Acc1</DisplayName>\n" + " </Owner>\n" +
        " <AccessControlList>\n" + "  <Grant>\n" +
        "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
        "xsi:type=\"CanonicalUser\">\n" + "    <ID>fsdfsfsfdsfd12DD</ID>\n" +
        "    <DisplayName>Acc1</DisplayName>\n" + "   </Grantee>\n" +
        "   <Permission>FULL_CONTROL</Permission>\n" + "  </Grant>\n" +
        "  <Grant>\n" +
        "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
        "xsi:type=\"CanonicalUser\">\n" + "    <ID>fsdfsfsfdsfd</ID>\n" +
        "    <DisplayName>Acc2</DisplayName>\n" + "   </Grantee>\n" +
        "   <Permission>READ</Permission>\n" + "  </Grant>\n" +
        " </AccessControlList>\n" + "</AccessControlPolicy>";
    requestBody = new TreeMap<>();
    requestBody.put("Auth-ACL", BinaryUtil.encodeToBase64String(existingACL));
    requestBody.put("x-amz-acl", "authenticated-read");
    String resultACL =
        new ACLCreator().createACLFromCannedInput(requestor, requestBody);
    Assert.assertNotNull(resultACL);
    AccessControlPolicy acp = new AccessControlPolicy(resultACL);
    Assert.assertEquals("fsdfsfsfdsfd12DD", acp.getOwner().getCanonicalId());
    Assert.assertEquals("Acc1", acp.getOwner().getDisplayName());
    Assert.assertEquals(2, acp.getAccessControlList().getGrantList().size());
    for (Grant grant : acp.getAccessControlList().getGrantList()) {
      if (grant.grantee.type.equals(Grantee.Types.Group)) {
        Assert.assertEquals(Group.AuthenticatedUsersURI, grant.grantee.uri);
        Assert.assertEquals("READ", grant.permission);
      } else if (Grantee.Types.CanonicalUser.equals(grant.grantee.type)) {
        Assert.assertEquals("fsdfsfsfdsfd12DD", grant.grantee.canonicalId);
        Assert.assertEquals("FULL_CONTROL", grant.permission);
      } else {
        Assert.fail();
      }
    }
  }

  @Test public void testCreateACLFromCannedInput_Success_private_NullACL()
      throws IOException,
      ParserConfigurationException, SAXException, GrantListFullException,
      TransformerException, InternalServerException {
    requestBody = new TreeMap<>();
    requestBody.put("x-amz-acl", "private");
    Requestor acc2Requestor = new Requestor();
    acc2Requestor.setAccount(account2);
    String resultACL =
        new ACLCreator().createACLFromCannedInput(acc2Requestor, requestBody);
    Assert.assertNotNull(resultACL);
    AccessControlPolicy acp = new AccessControlPolicy(resultACL);
    Assert.assertEquals("wwQQadgfhdfsfsfdsfd12DD",
                        acp.getOwner().getCanonicalId());
    Assert.assertEquals("Acc2", acp.getOwner().getDisplayName());
    Assert.assertEquals(1, acp.getAccessControlList().getGrantList().size());
    Assert.assertEquals(
        "wwQQadgfhdfsfsfdsfd12DD",
        acp.getAccessControlList().getGrantList().get(0).grantee.canonicalId);
    Assert.assertEquals(
        "FULL_CONTROL",
        acp.getAccessControlList().getGrantList().get(0).permission);
  }

  @Test(
      expected =
          InternalServerException
              .class) public void testCreateACLFromCannedInput_Fail_Invalid_CannedACL()
      throws IOException,
      ParserConfigurationException, SAXException, GrantListFullException,
      TransformerException, InternalServerException {
    requestBody = new TreeMap<>();
    requestBody.put("x-amz-acl", "invalid_input");
    Requestor acc2Requestor = new Requestor();
    acc2Requestor.setAccount(account2);
    new ACLCreator().createACLFromCannedInput(acc2Requestor, requestBody);
  }

  /**
       * Below will test bucket-owner-full-read scenario
       * @throws IOException
       * @throws ParserConfigurationException
       * @throws SAXException
       * @throws GrantListFullException
       * @throws TransformerException
       * @throws InternalServerException
       */
  @Test public void testCreateACLFromCannedInput_Success_bucket_owner_read()
      throws IOException,
      ParserConfigurationException, SAXException, GrantListFullException,
      TransformerException, InternalServerException {
    String authAcl =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" " +
        "standalone=\"no\"?><AccessControlPolicy " +
        "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">\n" + " <Owner>\n" +
        "  <ID>fsdfsfsfdsfd12DD</ID>\n" +
        "  <DisplayName>Acc1</DisplayName>\n" + " </Owner>\n" +
        " <AccessControlList>\n" + "  <Grant>\n" +
        "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
        "xsi:type=\"CanonicalUser\">\n" + "    <ID>fsdfsfsfdsfd12DD</ID>\n" +
        "    <DisplayName>Acc1</DisplayName>\n" + "   </Grantee>\n" +
        "   <Permission>FULL_CONTROL</Permission>\n" + "  </Grant>\n" +
        "  <Grant>\n" +
        "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
        "xsi:type=\"CanonicalUser\">\n" + "    <ID>fsdfsfsfdsfd</ID>\n" +
        "    <DisplayName>Acc2</DisplayName>\n" + "   </Grantee>\n" +
        "   <Permission>READ</Permission>\n" + "  </Grant>\n" +
        " </AccessControlList>\n" + "</AccessControlPolicy>";

    String bucketAcl =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" " +
        "standalone=\"no\"?><AccessControlPolicy " +
        "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">\n" + " <Owner>\n" +
        "  <ID>bucketownerID</ID>\n" +
        "  <DisplayName>BucketOwnerAccount</DisplayName>\n" + " </Owner>\n" +
        " <AccessControlList>\n" + "  <Grant>\n" +
        "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
        "xsi:type=\"CanonicalUser\">\n" + "    <ID>bucketownerID</ID>\n" +
        "    <DisplayName>BucketOwnerAccount</DisplayName>\n" +
        "   </Grantee>\n" + "   <Permission>FULL_CONTROL</Permission>\n" +
        "  </Grant>\n" + "  <Grant>\n" +
        "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
        "xsi:type=\"CanonicalUser\">\n" + "    <ID>fsdfsfsfdsfd</ID>\n" +
        "    <DisplayName>Acc2</DisplayName>\n" + "   </Grantee>\n" +
        "   <Permission>READ</Permission>\n" + "  </Grant>\n" +
        " </AccessControlList>\n" + "</AccessControlPolicy>";

    requestBody = new TreeMap<>();
    requestBody.put("ClientAbsoluteUri", "/bucket/object");
    requestBody.put("ClientQueryParams", "");
    requestBody.put("Auth-ACL", BinaryUtil.encodeToBase64String(authAcl));
    requestBody.put("Bucket-ACL", BinaryUtil.encodeToBase64String(bucketAcl));
    requestBody.put("x-amz-acl", "bucket-owner-read");
    String resultACL =
        new ACLCreator().createACLFromCannedInput(requestor, requestBody);
    Assert.assertNotNull(resultACL);
    AccessControlPolicy acp = new AccessControlPolicy(resultACL);
    Assert.assertEquals("fsdfsfsfdsfd12DD", acp.getOwner().getCanonicalId());
    Assert.assertEquals("Acc1", acp.getOwner().getDisplayName());
    Assert.assertEquals(2, acp.getAccessControlList().getGrantList().size());
    for (Grant grant : acp.getAccessControlList().getGrantList()) {
      if ("fsdfsfsfdsfd12DD".equals(grant.grantee.canonicalId)) {
        Assert.assertEquals("FULL_CONTROL", grant.permission);
      } else {
        Assert.assertEquals("READ", grant.permission);
      }
    }
  }

  /**
       * Below will test bucket-owner-full-control scenario
       * @throws IOException
       * @throws ParserConfigurationException
       * @throws SAXException
       * @throws GrantListFullException
       * @throws TransformerException
       * @throws InternalServerException
       */

  @Test public void
  testCreateACLFromCannedInput_Success_bucket_owner_full_control()
      throws IOException,
      ParserConfigurationException, SAXException, GrantListFullException,
      TransformerException, InternalServerException {
    String authAcl =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" " +
        "standalone=\"no\"?><AccessControlPolicy " +
        "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">\n" + " <Owner>\n" +
        "  <ID>fsdfsfsfdsfd12DD</ID>\n" +
        "  <DisplayName>Acc1</DisplayName>\n" + " </Owner>\n" +
        " <AccessControlList>\n" + "  <Grant>\n" +
        "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
        "xsi:type=\"CanonicalUser\">\n" + "    <ID>fsdfsfsfdsfd12DD</ID>\n" +
        "    <DisplayName>Acc1</DisplayName>\n" + "   </Grantee>\n" +
        "   <Permission>FULL_CONTROL</Permission>\n" + "  </Grant>\n" +
        "  <Grant>\n" +
        "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
        "xsi:type=\"CanonicalUser\">\n" + "    <ID>fsdfsfsfdsfd</ID>\n" +
        "    <DisplayName>Acc2</DisplayName>\n" + "   </Grantee>\n" +
        "   <Permission>READ</Permission>\n" + "  </Grant>\n" +
        " </AccessControlList>\n" + "</AccessControlPolicy>";
    String bucketAcl =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" " +
        "standalone=\"no\"?><AccessControlPolicy " +
        "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">\n" + " <Owner>\n" +
        "  <ID>bucketownerID</ID>\n" +
        "  <DisplayName>BucketOwnerAccount</DisplayName>\n" + " </Owner>\n" +
        " <AccessControlList>\n" + "  <Grant>\n" +
        "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
        "xsi:type=\"CanonicalUser\">\n" + "    <ID>bucketownerID</ID>\n" +
        "    <DisplayName>BucketOwnerAccount</DisplayName>\n" +
        "   </Grantee>\n" + "   <Permission>FULL_CONTROL</Permission>\n" +
        "  </Grant>\n" + "  <Grant>\n" +
        "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
        "xsi:type=\"CanonicalUser\">\n" + "    <ID>fsdfsfsfdsfd</ID>\n" +
        "    <DisplayName>Acc2</DisplayName>\n" + "   </Grantee>\n" +
        "   <Permission>READ</Permission>\n" + "  </Grant>\n" +
        " </AccessControlList>\n" + "</AccessControlPolicy>";

    requestBody = new TreeMap<>();
    requestBody.put("ClientAbsoluteUri", "/bucket/object");
    requestBody.put("ClientQueryParams", "acl");
    requestBody.put("Method", "PUT");
    requestBody.put("Auth-ACL", BinaryUtil.encodeToBase64String(authAcl));
    requestBody.put("Bucket-ACL", BinaryUtil.encodeToBase64String(bucketAcl));
    requestBody.put("x-amz-acl", "bucket-owner-full-control");
    String resultACL =
        new ACLCreator().createACLFromCannedInput(requestor, requestBody);
    Assert.assertNotNull(resultACL);
    AccessControlPolicy acp = new AccessControlPolicy(resultACL);
    Assert.assertEquals("fsdfsfsfdsfd12DD", acp.getOwner().getCanonicalId());
    Assert.assertEquals("Acc1", acp.getOwner().getDisplayName());
    Assert.assertEquals(2, acp.getAccessControlList().getGrantList().size());
    for (Grant grant : acp.getAccessControlList().getGrantList()) {
      Assert.assertEquals("FULL_CONTROL", grant.permission);
    }
  }

  /**
       * Below test will check acl creation in case BucketACL not present in
   * request,
       * and will also validate grant list for no repetition of grants
       * @throws IOException
       * @throws ParserConfigurationException
       * @throws SAXException
       * @throws GrantListFullException
       * @throws TransformerException
       * @throws InternalServerException
       */

  @Test public void
  testCreateACLFromCannedInput_bucketACL_not_present_requested_bucket_owner_read()
      throws IOException,
      ParserConfigurationException, SAXException, GrantListFullException,
      TransformerException, InternalServerException {
    String authAcl =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" " +
        "standalone=\"no\"?><AccessControlPolicy " +
        "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">\n" + " <Owner>\n" +
        "  <ID>fsdfsfsfdsfd12DD</ID>\n" +
        "  <DisplayName>Acc1</DisplayName>\n" + " </Owner>\n" +
        " <AccessControlList>\n" + "  <Grant>\n" +
        "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
        "xsi:type=\"CanonicalUser\">\n" + "    <ID>fsdfsfsfdsfd12DD</ID>\n" +
        "    <DisplayName>Acc1</DisplayName>\n" + "   </Grantee>\n" +
        "   <Permission>FULL_CONTROL</Permission>\n" + "  </Grant>\n" +
        "  <Grant>\n" +
        "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
        "xsi:type=\"CanonicalUser\">\n" + "    <ID>fsdfsfsfdsfd</ID>\n" +
        "    <DisplayName>Acc2</DisplayName>\n" + "   </Grantee>\n" +
        "   <Permission>READ</Permission>\n" + "  </Grant>\n" +
        " </AccessControlList>\n" + "</AccessControlPolicy>";

    requestBody = new TreeMap<>();
    requestBody.put("ClientAbsoluteUri", "/bucket/object?acl");
    requestBody.put("ClientQueryParams", "");
    requestBody.put("Method", "PUT");
    requestBody.put("Auth-ACL", BinaryUtil.encodeToBase64String(authAcl));
    requestBody.put("x-amz-acl", "bucket-owner-read");
    String resultACL =
        new ACLCreator().createACLFromCannedInput(requestor, requestBody);
    Assert.assertNotNull(resultACL);
    AccessControlPolicy acp = new AccessControlPolicy(resultACL);
    Assert.assertEquals("fsdfsfsfdsfd12DD", acp.getOwner().getCanonicalId());
    Assert.assertEquals("Acc1", acp.getOwner().getDisplayName());
    Assert.assertEquals(1, acp.getAccessControlList().getGrantList().size());
    Grant grant = acp.getAccessControlList().getGrantList().get(0);
    Assert.assertEquals("fsdfsfsfdsfd12DD", grant.grantee.canonicalId);
    Assert.assertEquals("FULL_CONTROL", grant.permission);
  }

  /**
   * Below will test bucket-owner-full-read scenario
   * @throws IOException
   * @throws ParserConfigurationException
   * @throws SAXException
   * @throws GrantListFullException
   * @throws TransformerException
   * @throws InternalServerException
   */
  @Test public void testCreateACLFromCannedInput_Ignore_bucket_owner_read()
      throws IOException,
      ParserConfigurationException, SAXException, GrantListFullException,
      TransformerException, InternalServerException {
    String authAcl =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" " +
        "standalone=\"no\"?><AccessControlPolicy " +
        "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">\n" + " <Owner>\n" +
        "  <ID>fsdfsfsfdsfd12DD</ID>\n" +
        "  <DisplayName>Acc1</DisplayName>\n" + " </Owner>\n" +
        " <AccessControlList>\n" + "  <Grant>\n" +
        "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
        "xsi:type=\"CanonicalUser\">\n" + "    <ID>fsdfsfsfdsfd12DD</ID>\n" +
        "    <DisplayName>Acc1</DisplayName>\n" + "   </Grantee>\n" +
        "   <Permission>FULL_CONTROL</Permission>\n" + "  </Grant>\n" +
        " </AccessControlList>\n" + "</AccessControlPolicy>";

    String bucketAcl =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" " +
        "standalone=\"no\"?><AccessControlPolicy " +
        "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">\n" + " <Owner>\n" +
        "  <ID>fsdfsfsfdsfd12DD</ID>\n" +
        "  <DisplayName>Acc1</DisplayName>\n" + " </Owner>\n" +
        " <AccessControlList>\n" + "  <Grant>\n" +
        "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
        "xsi:type=\"CanonicalUser\">\n" + "    <ID>fsdfsfsfdsfd12DD</ID>\n" +
        "    <DisplayName>Acc1</DisplayName>\n" + "   </Grantee>\n" +
        "   <Permission>FULL_CONTROL</Permission>\n" + "  </Grant>\n" +
        " </AccessControlList>\n" + "</AccessControlPolicy>";

    requestBody = new TreeMap<>();
    requestBody.put("ClientAbsoluteUri", "/bucket");
    requestBody.put("ClientQueryParams", "");
    requestBody.put("Auth-ACL", BinaryUtil.encodeToBase64String(authAcl));
    requestBody.put("Bucket-ACL", BinaryUtil.encodeToBase64String(bucketAcl));
    requestBody.put("x-amz-acl", "bucket-owner-read");
    String resultACL =
        new ACLCreator().createACLFromCannedInput(requestor, requestBody);
    Assert.assertNotNull(resultACL);
    AccessControlPolicy acp = new AccessControlPolicy(resultACL);
    Assert.assertEquals("fsdfsfsfdsfd12DD", acp.getOwner().getCanonicalId());
    Assert.assertEquals("Acc1", acp.getOwner().getDisplayName());
    Assert.assertEquals(1, acp.getAccessControlList().getGrantList().size());
    for (Grant grant : acp.getAccessControlList().getGrantList()) {
      Assert.assertEquals("fsdfsfsfdsfd12DD", grant.grantee.canonicalId);
      Assert.assertEquals("FULL_CONTROL", grant.permission);
    }
  }

  /**
   * Below will test bucket-owner-full-control scenario
   * @throws IOException
   * @throws ParserConfigurationException
   * @throws SAXException
   * @throws GrantListFullException
   * @throws TransformerException
   * @throws InternalServerException
   */
  @Test public void
  testCreateACLFromCannedInput_Ignore_bucket_owner_full_control()
      throws IOException,
      ParserConfigurationException, SAXException, GrantListFullException,
      TransformerException, InternalServerException {
    String authAcl =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" " +
        "standalone=\"no\"?><AccessControlPolicy " +
        "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">\n" + " <Owner>\n" +
        "  <ID>fsdfsfsfdsfd12DD</ID>\n" +
        "  <DisplayName>Acc1</DisplayName>\n" + " </Owner>\n" +
        " <AccessControlList>\n" + "  <Grant>\n" +
        "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
        "xsi:type=\"CanonicalUser\">\n" + "    <ID>fsdfsfsfdsfd12DD</ID>\n" +
        "    <DisplayName>Acc1</DisplayName>\n" + "   </Grantee>\n" +
        "   <Permission>FULL_CONTROL</Permission>\n" + "  </Grant>\n" +
        " </AccessControlList>\n" + "</AccessControlPolicy>";

    String bucketAcl =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" " +
        "standalone=\"no\"?><AccessControlPolicy " +
        "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">\n" + " <Owner>\n" +
        "  <ID>fsdfsfsfdsfd12DD</ID>\n" +
        "  <DisplayName>Acc1</DisplayName>\n" + " </Owner>\n" +
        " <AccessControlList>\n" + "  <Grant>\n" +
        "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
        "xsi:type=\"CanonicalUser\">\n" + "    <ID>fsdfsfsfdsfd12DD</ID>\n" +
        "    <DisplayName>Acc1</DisplayName>\n" + "   </Grantee>\n" +
        "   <Permission>FULL_CONTROL</Permission>\n" + "  </Grant>\n" +
        " </AccessControlList>\n" + "</AccessControlPolicy>";

    requestBody = new TreeMap<>();
    requestBody.put("ClientAbsoluteUri", "/bucket");
    requestBody.put("ClientQueryParams", "");
    requestBody.put("Auth-ACL", BinaryUtil.encodeToBase64String(authAcl));
    requestBody.put("Bucket-ACL", BinaryUtil.encodeToBase64String(bucketAcl));
    requestBody.put("x-amz-acl", "bucket-owner-full-control");
    String resultACL =
        new ACLCreator().createACLFromCannedInput(requestor, requestBody);
    Assert.assertNotNull(resultACL);
    AccessControlPolicy acp = new AccessControlPolicy(resultACL);
    Assert.assertEquals("fsdfsfsfdsfd12DD", acp.getOwner().getCanonicalId());
    Assert.assertEquals("Acc1", acp.getOwner().getDisplayName());
    Assert.assertEquals(1, acp.getAccessControlList().getGrantList().size());
    for (Grant grant : acp.getAccessControlList().getGrantList()) {
      Assert.assertEquals("fsdfsfsfdsfd12DD", grant.grantee.canonicalId);
      Assert.assertEquals("FULL_CONTROL", grant.permission);
    }
  }
}


