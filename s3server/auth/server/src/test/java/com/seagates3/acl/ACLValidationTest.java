package com.seagates3.acl;

import static org.junit.Assert.assertEquals;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.powermock.api.mockito.PowerMockito;
import org.powermock.core.classloader.annotations.PowerMockIgnore;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;

import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.dao.ldap.AccountImpl;
import com.seagates3.model.Account;
import com.seagates3.response.ServerResponse;

import io.netty.handler.codec.http.HttpResponseStatus;

@RunWith(PowerMockRunner.class) @PrepareForTest({ACLValidation.class})
    @PowerMockIgnore({"javax.management.*"}) public class ACLValidationTest {

 private
  AccountImpl mockAccountImpl;

 private
  Account mockAccount;

  // valid ACL XML
  static String acpXmlString =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>" +
      "<AccessControlPolicy " +
      "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">" + " <Owner>" +
      "  <ID>id</ID>" + "  <DisplayName>owner</DisplayName>" + " </Owner>" +
      " <AccessControlList>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" +
      " </AccessControlList>" + "</AccessControlPolicy>";

  // invalid ACL XML schema string
  static String acpXmlString_SchemaError =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>" +
      "<AccessControlPolicy " +
      "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">" + " <Owner>" +
      "  <ID>id</ID>" + "  <DisplayName>owner</DisplayName>" + " </Owner>" +
      " <AccessControlList>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id1</ID>" +
      "    <DisplayName>name1</DisplayName>" + "   </Grantee>" +
      "   <Permission>READ</Permission>" + "  </Grant>" +
      " </AccessControlList>" + "</AccessControlPolicy>";

  // invalid ACL XML - multiple DisplayName
  static String acpXmlString_MultipleDisplayName =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>" +
      "<AccessControlPolicy " +
      "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">" + " <Owner>" +
      "\"  <DisplayName>owner</DisplayName>\"" + "  <ID>id</ID>" +
      "  <DisplayName>owner</DisplayName>" + " </Owner>" +
      " <AccessControlList>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <DisplayName>owner</DisplayName>" +
      "    <ID>id</ID>" + "    <DisplayName>owner</DisplayName>" +
      "   </Grantee>" + "   <Permission>FULL_CONTROL</Permission>" +
      "  </Grant>" + " </AccessControlList>" + "</AccessControlPolicy>";

  // valid ACL XML - No DisplayName
  static String acpXmlString_NoDisplayName =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>" +
      "<AccessControlPolicy " +
      "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">" + " <Owner>" +
      "  <ID>id</ID>" + " </Owner>" + " <AccessControlList>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" +
      " </AccessControlList>" + "</AccessControlPolicy>";

  // valid ACL XML
  static String acpXmlString_unorderedDisplayName =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>" +
      "<AccessControlPolicy " +
      "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">" + " <Owner>" +
      "  <DisplayName>owner</DisplayName>" + "  <ID>id</ID>" + " </Owner>" +
      " <AccessControlList>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" +
      " </AccessControlList>" + "</AccessControlPolicy>";

  // valid full ACL XML including types - CanonicalUser/Group/Email
  static String acpXmlString_Full =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n" +
      "<AccessControlPolicy " +
      "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">\r\n" +
      "   <Owner>\r\n" + "      <ID>id</ID>\r\n" +
      "		 <DisplayName>owner</DisplayName>\r\n" + "   </Owner>\r\n" +
      "   <AccessControlList>\r\n" + "      <Grant>\r\n" +
      "         <Grantee " +
      "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
      "xsi:type=\"CanonicalUser\">\r\n" +
      "			   <DisplayName>owner</DisplayName>\r\n" +
      "            <ID>id</ID>\r\n" + "         </Grantee>\r\n" +
      "         <Permission>FULL_CONTROL</Permission>\r\n" +
      "      </Grant>\r\n" + "	     <Grant>\r\n" + "         <Grantee " +
      "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
      "xsi:type=\"Group\">\r\n" + "            " +
      "<URI>http://acs.amazonaws.com/groups/global/AllUsers</URI>\r\n" +
      "         </Grantee>\r\n" + "         <Permission>READ</Permission>\r\n" +
      "      </Grant>\r\n" + "	     <Grant>\r\n" + "         <Grantee " +
      "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
      "xsi:type=\"Group\">\r\n" + "            " +
      "<URI>http://acs.amazonaws.com/groups/global/AuthenticatedUsers</" +
      "URI>\r\n" + "         </Grantee>\r\n" +
      "         <Permission>READ</Permission>\r\n" + "      </Grant>\r\n" +
      "      <Grant>\r\n" + "         <Grantee " +
      "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
      "xsi:type=\"Group\">\r\n" + "            " +
      "<URI>http://acs.amazonaws.com/groups/s3/LogDelivery</URI>\r\n" +
      "         </Grantee>\r\n" + "         <Permission>READ</Permission>\r\n" +
      "      </Grant>\r\n" + "      <Grant>\r\n" + "      <Grantee " +
      "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
      "xsi:type=\"AmazonCustomerByEmail\">\r\n" +
      "        <EmailAddress>xyz@seagate.com</EmailAddress>\r\n" +
      "      </Grantee>\r\n" + "      <Permission>WRITE_ACP</Permission>\r\n" +
      "      </Grant>" + "   </AccessControlList>\r\n" +
      "</AccessControlPolicy>";

  // invalid ACL XML Groups
  static String acpXmlString_InvalidGroup =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n" +
      "<AccessControlPolicy " +
      "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">\r\n" +
      "   <Owner>\r\n" + "      <ID>id</ID>\r\n" +
      "		 <DisplayName>owner</DisplayName>\r\n" + "   </Owner>\r\n" +
      "   <AccessControlList>\r\n" + "      <Grant>\r\n" +
      "         <Grantee " +
      "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
      "xsi:type=\"CanonicalUser\">\r\n" +
      "			   <DisplayName>owner</DisplayName>\r\n" +
      "            <ID>id</ID>\r\n" + "         </Grantee>\r\n" +
      "         <Permission>FULL_CONTROL</Permission>\r\n" +
      "      </Grant>\r\n" + "	     <Grant>\r\n" + "         <Grantee " +
      "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
      "xsi:type=\"Group\">\r\n" + "            " +
      "<URI>http://acs.amazonaws.com/groups/global/AllUsers</URI>\r\n" +
      "         </Grantee>\r\n" + "         <Permission>READ</Permission>\r\n" +
      "      </Grant>\r\n" + "	     <Grant>\r\n" + "         <Grantee " +
      "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " +
      "xsi:type=\"Group\">\r\n" + "            " +
      "<URI>http://acs.amazonaws.com/groups/global/INVALIDUsers</URI>\r\n" +
      "         </Grantee>\r\n" + "         <Permission>READ</Permission>\r\n" +
      "      </Grant>\r\n" + "   </AccessControlList>\r\n" +
      "</AccessControlPolicy>";

 private
  ACLValidation aclValidation;

  @BeforeClass public static void setUpBeforeClass() throws Exception {
    AuthServerConfig.authResourceDir = "../resources";
  }

  @Before public void setup() {
    mockAccountImpl = Mockito.mock(AccountImpl.class);
    mockAccount = Mockito.mock(Account.class);
  }

  // test for valid ACL XML
  @Test public void testValidate_defaultACP_Successful() throws Exception {

    aclValidation = new ACLValidation(acpXmlString);
    PowerMockito.mockStatic(ACLValidation.class);
    PowerMockito.when(ACLValidation.class, "checkIdExists", "id", "owner")
        .thenReturn(true);
    ServerResponse response = aclValidation.validate(null);
    assertEquals(HttpResponseStatus.OK, response.getResponseStatus());
  }

  // test for valid ACL XML consisting of Canonical user / group / email id
  @Test public void testValidate_fullACP_Successful() throws Exception {

    aclValidation = new ACLValidation(acpXmlString_Full);
    PowerMockito.mockStatic(ACLValidation.class);
    PowerMockito.when(ACLValidation.class, "checkIdExists", "id", "owner")
        .thenReturn(true);
    PowerMockito.whenNew(AccountImpl.class).withNoArguments().thenReturn(
        mockAccountImpl);
    Mockito.when(mockAccountImpl.findByEmailAddress("xyz@seagate.com"))
        .thenReturn(mockAccount);
    Mockito.when(mockAccount.exists()).thenReturn(true);
    ServerResponse response = aclValidation.validate(null);
    assertEquals(HttpResponseStatus.OK, response.getResponseStatus());
  }

  // test for invalid ACL XML consisting of email address of account which does
  // not exists
  @Test public void
  testValidate_invalidACL_AccountwithEmailAddressDoesNotExsists()
      throws Exception {
    aclValidation = new ACLValidation(acpXmlString_Full);
    PowerMockito.mockStatic(ACLValidation.class);
    PowerMockito.when(ACLValidation.class, "checkIdExists", "id", "owner")
        .thenReturn(true);
    PowerMockito.whenNew(AccountImpl.class).withNoArguments().thenReturn(
        mockAccountImpl);
    Mockito.when(mockAccountImpl.findByEmailAddress("xyz@seagate.com"))
        .thenReturn(mockAccount);
    // if account does not exists...
    Mockito.when(mockAccount.exists()).thenReturn(false);
    ServerResponse response = aclValidation.validate(null);
    assertEquals(HttpResponseStatus.BAD_REQUEST, response.getResponseStatus());
  }

  // test for valid ACL XML consisting of Canonical user / group / email id
  @Test public void testValidate_invalidACL_invalidGroup() throws Exception {

    aclValidation = new ACLValidation(acpXmlString_InvalidGroup);
    PowerMockito.mockStatic(ACLValidation.class);
    PowerMockito.when(ACLValidation.class, "checkIdExists", "id", "owner")
        .thenReturn(true);
    PowerMockito.whenNew(AccountImpl.class).withNoArguments().thenReturn(
        mockAccountImpl);
    Mockito.when(mockAccountImpl.findByEmailAddress("xyz@seagate.com"))
        .thenReturn(mockAccount);
    Mockito.when(mockAccount.exists()).thenReturn(true);
    ServerResponse response = aclValidation.validate(null);
    assertEquals(HttpResponseStatus.BAD_REQUEST, response.getResponseStatus());
  }

  // test for valid ACL XML
  @Test public void testValidate_UnorideredDisplayName_Successful()
      throws Exception {

    aclValidation = new ACLValidation(acpXmlString_unorderedDisplayName);
    PowerMockito.mockStatic(ACLValidation.class);
    PowerMockito.when(ACLValidation.class, "checkIdExists", "id", "owner")
        .thenReturn(true);
    ServerResponse response = aclValidation.validate(null);
    assertEquals(HttpResponseStatus.OK, response.getResponseStatus());
  }

  // test for valid ACL XML
  @Test public void testValidate_NoDisplayName_Successful() throws Exception {

    aclValidation = new ACLValidation(acpXmlString_NoDisplayName);
    PowerMockito.mockStatic(ACLValidation.class);
    PowerMockito.when(ACLValidation.class, "checkIdExists", "id", null)
        .thenReturn(true);
    ServerResponse response = aclValidation.validate(null);
    assertEquals(HttpResponseStatus.OK, response.getResponseStatus());
  }

  // test for invalid schema of ACL XML
  @Test public void testValidate_invalid_acl_schema() throws Exception {

    aclValidation = new ACLValidation(acpXmlString_SchemaError);
    ServerResponse response = aclValidation.validate(null);
    assertEquals(HttpResponseStatus.BAD_REQUEST, response.getResponseStatus());
  }

  // test for invalid schema of ACL XML
  @Test public void testValidate_invalid_acl_schema_multipleDisplayName()
      throws Exception {

    aclValidation = new ACLValidation(acpXmlString_MultipleDisplayName);
    ServerResponse response = aclValidation.validate(null);
    assertEquals(HttpResponseStatus.BAD_REQUEST, response.getResponseStatus());
  }

  // test for invalid id of ACL XML
  @Test public void testValidate_invalid_acl_id() throws Exception {

    aclValidation = new ACLValidation(acpXmlString);
    PowerMockito.mockStatic(ACLValidation.class);
    PowerMockito.when(ACLValidation.class, "checkIdExists", "id", "owner")
        .thenReturn(false);
    ServerResponse response = aclValidation.validate(null);
    assertEquals(HttpResponseStatus.BAD_REQUEST, response.getResponseStatus());
  }

  // test for invalid id of ACL XML
  @Test public void testValidate_invalid_acl_grantfull() throws Exception {

    aclValidation = new ACLValidation(acpXmlString_GrantOverflow);
    PowerMockito.mockStatic(ACLValidation.class);
    PowerMockito.when(ACLValidation.class, "checkIdExists", "id", "owner")
        .thenReturn(true);
    ServerResponse response = aclValidation.validate(null);
    assertEquals(HttpResponseStatus.BAD_REQUEST, response.getResponseStatus());
  }

  // ACL String with >100 grants
  static String acpXmlString_GrantOverflow =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>" +
      "<AccessControlPolicy " +
      "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">" + " <Owner>" +
      "  <ID>id</ID>" + "  <DisplayName>owner</DisplayName>" + " </Owner>" +
      " <AccessControlList>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" + "  <Grant>" +
      "   <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"" +
      " xsi:type=\"CanonicalUser\">" + "    <ID>id</ID>" +
      "    <DisplayName>owner</DisplayName>" + "   </Grantee>" +
      "   <Permission>FULL_CONTROL</Permission>" + "  </Grant>" +
      " </AccessControlList>" + "</AccessControlPolicy>";
}
