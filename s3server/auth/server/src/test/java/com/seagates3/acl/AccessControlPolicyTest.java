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
 * Original author:  Abhilekh Mustapure <abhilekh.mustapure@seagate.com>
 * Original creation date: 05-Apr-2019
 */

package com.seagates3.acl;

import com.seagates3.acl.AccessControlPolicy;
import com.seagates3.exception.GrantListFullException;

import org.junit.Test;
import org.xml.sax.SAXException;
import javax.xml.parsers.ParserConfigurationException;
import java.io.IOException;
import org.junit.Before;
import java.io.*;
import com.seagates3.acl.AccessControlList;
import javax.xml.transform.TransformerException;

import static org.junit.Assert.*;

public
class AccessControlPolicyTest {

  /*
   *  Set up for tests
   */

  String aclXmlPath = "../resources/defaultAclTemplate.xml";
  AccessControlPolicy accessControlPolicy;
  static AccessControlList acl;
  String acpXmlString =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>" +
      "<AccessControlPolicy " +
      "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">" + " <Owner>" +
      "  <ID>123er45678</ID>" + "  <DisplayName>S3test</DisplayName>" +
      " </Owner>" + "  <Owner>" + "  <ID>Int1</ID>" +
      "  <DisplayName>String</DisplayName>" + " </Owner>" +
      " <AccessControlList>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>Int</ID>" +
      "    <DisplayName>String</DisplayName>" + "   </Grantee>" +
      "   <Permission>String</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>Int4</ID>" +
      "    <DisplayName>Stringe</DisplayName>" + "   </Grantee>" +
      "   <Permission>String3</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>Int4</ID>" +
      "    <DisplayName>Stringe</DisplayName>" + "   </Grantee>" +
      "   <Permission>String3</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>Int4</ID>" +
      "    <DisplayName>Stringe</DisplayName>" + "   </Grantee>" +
      "   <Permission>String3</Permission>" + "  </Grant>" +
      " </AccessControlList>" + "</AccessControlPolicy>";

  @Before public void setUp() throws ParserConfigurationException, SAXException,
      IOException, GrantListFullException {
    File xmlFile = new File(aclXmlPath);
    accessControlPolicy = new AccessControlPolicy(xmlFile);
    acl = new AccessControlList();
    Grant grant;
    Grantee grantee;

    for (int i = 0; i < 10; i++) {
      grantee = new Grantee("id" + i, "abc" + i);
      grant = new Grant(grantee, "permission" + i);
      acl.addGrant(grant);
    }
  }

  @Test public void loadxml_OwnerId_Test() {
    Owner owner = accessControlPolicy.getOwner();
    assertEquals(owner.canonicalId, "Owner_ID");
  }

  @Test public void loadxml_OwnerName_Test() {
    Owner owner = accessControlPolicy.getOwner();
    assertEquals(owner.displayName, "Owner_Name");
  }

  @Test public void loadxml_GranteeName_Test() {
    AccessControlList accessControlList =
        accessControlPolicy.getAccessControlList();
    assertEquals(accessControlList.getGrantList().get(0).grantee.displayName,
                 "Grantee_Name");
  }

  @Test public void loadxml_GranteeId_Test() {
    AccessControlList accessControlList =
        accessControlPolicy.getAccessControlList();
    assertEquals(accessControlList.getGrantList().get(0).grantee.canonicalId,
                 "Grantee_ID");
  }

  @Test public void loadxml_GrantPermission_Test() {
    AccessControlList accessControlList =
        accessControlPolicy.getAccessControlList();
    assertEquals(accessControlList.getGrantList().get(0).permission,
                 "Permission");
  }

  @Test public void loadxml_GetGranteeSize_Test() {
    AccessControlList accessControlList =
        accessControlPolicy.getAccessControlList();
    assertEquals(accessControlList.getGrantList().size(), 1);
  }

  @Test public void setOwnerId_Test() {
    Owner owner = new Owner("123", "abc");
    accessControlPolicy.setOwner(owner);
    owner = accessControlPolicy.getOwner();
    assertEquals("123", owner.getCanonicalId());
  }

  @Test public void setOwnerDisplayName_Test() {
    Owner owner = new Owner("123", "abc");
    accessControlPolicy.setOwner(owner);
    owner = accessControlPolicy.getOwner();
    assertEquals("abc", owner.getDisplayName());
  }

  @Test public void setAccessControlListGranteeId_Test() {
    assertEquals(acl.getGrantList().get(1).grantee.getCanonicalId(), "id1");
  }

  @Test public void setAccessControlListGranteeName_Test() {
    assertEquals(acl.getGrantList().get(1).grantee.getDisplayName(), "abc1");
  }

  @Test public void setAccessControlListPermission__Test() {
    assertEquals(acl.getGrantList().get(1).getPermission(), "permission1");
  }

  @Test public void setAccessControlListSize__Test() {
    assertEquals(acl.getGrantList().size(), 10);
  }

  @Test public void flushXmlValuesTest() throws TransformerException,
      GrantListFullException {

    AccessControlList Acl = new AccessControlList();
    String xml = null;
    Grant grant;
    Grantee grantee;
    for (int i = 0; i < 10; i++) {
      grantee = new Grantee("id" + i, "abc" + i);
      grant = new Grant(grantee, "permission" + i);
      Acl.addGrant(grant);
    }
    accessControlPolicy.setAccessControlList(Acl);
    try {
      xml = accessControlPolicy.getXml();
    }
    catch (TransformerException ex) {
      System.out.println(ex.getMessage());
    }

    assertTrue(xml.contains("<DisplayName>abc4</DisplayName>"));
  }

  @Test public void recheckAcpXml_test() throws ParserConfigurationException,
      TransformerException, SAXException, IOException, GrantListFullException {

    AccessControlList Acl = new AccessControlList();
    Grant grant;
    Grantee grantee;
    String xml = null;
    AccessControlPolicy acp = null;
    for (int i = 0; i < 10; i++) {
      grantee = new Grantee("id" + i, "abc" + i);
      grant = new Grant(grantee, "permission" + i);
      Acl.addGrant(grant);
    }

    accessControlPolicy.setAccessControlList(Acl);
    try {
      xml = accessControlPolicy.getXml();
      acp = new AccessControlPolicy(xml);
    }
    catch (TransformerException ex) {
      System.out.println(ex.getMessage());
    }
    catch (ParserConfigurationException ex) {
      System.out.println(ex.getMessage());
    }
    catch (SAXException ex) {
      System.out.println(ex.getMessage());
    }
    catch (IOException ex) {
      System.out.println(ex.getMessage());
    }

    assertEquals(acp.accessControlList.getGrantList().size(), 10);
  }

  @Test public void checkGranteeIdForGetXmlString_test()
      throws ParserConfigurationException,
      TransformerException, SAXException, IOException, GrantListFullException {

    AccessControlList Acl = new AccessControlList();
    Grant grant;
    Grantee grantee;
    AccessControlPolicy acp = null;
    for (int i = 0; i < 10; i++) {
      grantee = new Grantee("id" + i, "abc" + i);
      grant = new Grant(grantee, "permission" + i);
      Acl.addGrant(grant);
    }

    accessControlPolicy.setAccessControlList(Acl);
    try {
      String xml = accessControlPolicy.getXml();
      acp = new AccessControlPolicy(xml);
    }
    catch (TransformerException ex) {
      System.out.println(ex.getMessage());
    }
    catch (ParserConfigurationException ex) {
      System.out.println(ex.getMessage());
    }
    catch (SAXException ex) {
      System.out.println(ex.getMessage());
    }
    catch (IOException ex) {
      System.out.println(ex.getMessage());
    }

    assertEquals(
        acp.accessControlList.getGrantList().get(0).grantee.getCanonicalId(),
        "id0");
    assertEquals(
        acp.accessControlList.getGrantList().get(5).grantee.getCanonicalId(),
        "id5");
    assertEquals(
        acp.accessControlList.getGrantList().get(9).grantee.getCanonicalId(),
        "id9");
  }

  @Test public void checkGranteeNameForGetXmlString_test()
      throws ParserConfigurationException,
      TransformerException, SAXException, IOException, GrantListFullException {

    AccessControlList Acl = new AccessControlList();
    Grant grant;
    Grantee grantee;
    AccessControlPolicy acp = null;
    for (int i = 0; i < 10; i++) {
      grantee = new Grantee("id" + i, "abc" + i);
      grant = new Grant(grantee, "permission" + i);
      Acl.addGrant(grant);
    }

    accessControlPolicy.setAccessControlList(Acl);
    try {
      String xml = accessControlPolicy.getXml();
      acp = new AccessControlPolicy(xml);
    }
    catch (TransformerException ex) {
      System.out.println(ex.getMessage());
    }
    catch (ParserConfigurationException ex) {
      System.out.println(ex.getMessage());
    }
    catch (SAXException ex) {
      System.out.println(ex.getMessage());
    }
    catch (IOException ex) {
      System.out.println(ex.getMessage());
    }

    assertEquals(
        acp.accessControlList.getGrantList().get(0).grantee.getDisplayName(),
        "abc0");
    assertEquals(
        acp.accessControlList.getGrantList().get(5).grantee.getDisplayName(),
        "abc5");
    assertEquals(
        acp.accessControlList.getGrantList().get(9).grantee.getDisplayName(),
        "abc9");
  }

  @Test public void checkGranteePermissionForGetXmlString_test()
      throws ParserConfigurationException,
      TransformerException, SAXException, IOException, GrantListFullException {

    AccessControlList Acl = new AccessControlList();
    Grant grant;
    Grantee grantee;
    AccessControlPolicy acp = null;
    for (int i = 0; i < 10; i++) {
      grantee = new Grantee("id" + i, "abc" + i);
      grant = new Grant(grantee, "permission" + i);
      Acl.addGrant(grant);
    }

    accessControlPolicy.setAccessControlList(Acl);

    try {
      String xml = accessControlPolicy.getXml();
      acp = new AccessControlPolicy(xml);
    }
    catch (TransformerException ex) {
      System.out.println(ex.getMessage());
    }
    catch (ParserConfigurationException ex) {
      System.out.println(ex.getMessage());
    }
    catch (SAXException ex) {
      System.out.println(ex.getMessage());
    }
    catch (IOException ex) {
      System.out.println(ex.getMessage());
    }

    assertEquals(acp.accessControlList.getGrantList().get(0).getPermission(),
                 "permission0");
    assertEquals(acp.accessControlList.getGrantList().get(9).getPermission(),
                 "permission9");
    assertEquals(acp.accessControlList.getGrantList().get(5).getPermission(),
                 "permission5");
  }

  @Test public void validateXmlString_Test()
      throws ParserConfigurationException,
      TransformerException, SAXException, IOException, GrantListFullException {

    AccessControlPolicy ACP = null;
    try {
      ACP = new AccessControlPolicy(acpXmlString);
    }
    catch (ParserConfigurationException ex) {
      System.out.println(ex.getMessage());
    }
    catch (SAXException ex) {
      System.out.println(ex.getMessage());
    }
    catch (IOException ex) {
      System.out.println(ex.getMessage());
    }
    try {
      assertEquals(acpXmlString, ACP.getXml());
    }
    catch (TransformerException ex) {
      System.out.println(ex.getMessage());
    }
  }

  @Test public void nodeGrantListGreaterThanLocalACLGrantList_Test()
      throws ParserConfigurationException,
      TransformerException, SAXException, IOException, GrantListFullException {

    AccessControlPolicy ACP = null;
    try {
      ACP = new AccessControlPolicy(acpXmlString);
    }
    catch (ParserConfigurationException ex) {
      System.out.println(ex.getMessage());
    }
    catch (SAXException ex) {
      System.out.println(ex.getMessage());
    }
    catch (IOException ex) {
      System.out.println(ex.getMessage());
    }

    AccessControlList Acl = new AccessControlList();
    Grant grant;
    Grantee grantee;
    for (int i = 0; i < 1; i++) {
      grantee = new Grantee("id" + i, "abc" + i);
      grant = new Grant(grantee, "permission" + i);
      Acl.addGrant(grant);
    }

    ACP.setAccessControlList(Acl);

    String acp =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>" +
        "<AccessControlPolicy " +
        "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">" + " <Owner>" +
        "  <ID>123er45678</ID>" + "  <DisplayName>S3test</DisplayName>" +
        " </Owner>" + "  <Owner>" + "  <ID>Int1</ID>" +
        "  <DisplayName>String</DisplayName>" + " </Owner>" +
        " <AccessControlList>" + "  <Grant>" +
        "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
        " xsi:type=\"CanonicalUser\">" + "    <ID>id0</ID>" +
        "    <DisplayName>abc0</DisplayName>" + "   </Grantee>" +
        "   <Permission>permission0</Permission>" + "  </Grant>   " +
        "    </AccessControlList>" + "</AccessControlPolicy>";
    try {
      assertEquals(acp, ACP.getXml());
    }
    catch (TransformerException ex) {
      System.out.println(ex.getMessage());
    }
  }
}
