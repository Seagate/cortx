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
 * Original author:  Basavaraj Kirunge
 * Original creation date: 25-Mar-2019
 */

package com.seagates3.util;

import static org.junit.Assert.*;

import org.junit.Test;
import org.xml.sax.SAXException;

public class XMLValidatorUtilTest {

    String xsdPath = "../resources/AmazonS3.xsd";

    String acl = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" +
            "<AccessControlPolicy"
            + " xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
            + "<Owner><ID>"
            + "b103e16d027d24270d8facf37a48b141fd88ac8f43f9f942b91ba1cf1dc33f71"
            + "</ID>"
            + "<DisplayName>kirungeb</DisplayName></Owner><AccessControlList>"
            + "<Grant>"
            + "<Grantee "
            + "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
            + "xsi:type=\"CanonicalUser\"><ID>"
            + "b103e16d027d24270d8facf37a48b141fd88ac8f43f9f942b91ba1cf1dc33f71"
            + "</ID>"
            + "<DisplayName>kirungeb</DisplayName></Grantee>"
            + "<Permission>FULL_CONTROL</Permission></Grant></AccessControlList>"
            + "</AccessControlPolicy>";

    String acl_invalid_permission =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            + "<AccessControlPolicy"
            + " xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
            + "<Owner><ID>"
            + "b103e16d027d24270d8facf37a48b141fd88ac8f43f9f942b91ba1cf1dc33f71"
            + "</ID>"
            + "<DisplayName>kirungeb</DisplayName></Owner><AccessControlList>"
            + "<Grant>"
            + "<Grantee "
            + "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
            + "xsi:type=\"CanonicalUser\"><ID>"
            + "b103e16d027d24270d8facf37a48b141fd88ac8f43f9f942b91ba1cf1dc33f71"
            + "</ID>"
            + "<DisplayName>kirungeb</DisplayName></Grantee>"
            + "<Permission>CONTROL</Permission></Grant></AccessControlList>"
            + "</AccessControlPolicy>";

    String acl_invalid_missingid =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            + "<AccessControlPolicy"
            + " xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
            + "<Owner><ID>"
            + "b103e16d027d24270d8facf37a48b141fd88ac8f43f9f942b91ba1cf1dc33f71"
            + "</ID>"
            + "<DisplayName>kirungeb</DisplayName></Owner><AccessControlList>"
            + "<Grant>"
            + "<Grantee "
            + "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
            + "xsi:type=\"CanonicalUser\">"
            + "<DisplayName>kirungeb</DisplayName></Grantee>"
            + "<Permission>READ</Permission></Grant></AccessControlList>"
            + "</AccessControlPolicy>";

    String acl_invalid_nograntee =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            + "<AccessControlPolicy"
            + " xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
            + "<Owner><ID>"
            + "b103e16d027d24270d8facf37a48b141fd88ac8f43f9f942b91ba1cf1dc33f71"
            + "</ID>"
            + "<DisplayName>kirungeb</DisplayName></Owner><AccessControlList>"
            + "<Grant>"
            + "<Permission>READ</Permission></Grant></AccessControlList>"
            + "</AccessControlPolicy>";


    String acl_missing_owner = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" +
            "<AccessControlPolicy"
            + " xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
            + "<AccessControlList>"
            + "<Grant>"
            + "<Grantee "
            + "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
            + "xsi:type=\"CanonicalUser\"><ID>"
            + "b103e16d027d24270d8facf37a48b141fd88ac8f43f9f942b91ba1cf1dc33f71"
            + "</ID>"
            + "<DisplayName>kirungeb</DisplayName></Grantee>"
            + "<Permission>FULL_CONTROL</Permission></Grant></AccessControlList>"
            + "</AccessControlPolicy>";

    String acl_no_permission = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" +
            "<AccessControlPolicy"
            + " xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
            + "<Owner><ID>"
            + "b103e16d027d24270d8facf37a48b141fd88ac8f43f9f942b91ba1cf1dc33f71"
            + "</ID>"
            + "<DisplayName>kirungeb</DisplayName></Owner><AccessControlList>"
            + "<Grant>"
            + "<Grantee "
            + "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
            + "xsi:type=\"CanonicalUser\"><ID>"
            + "b103e16d027d24270d8facf37a48b141fd88ac8f43f9f942b91ba1cf1dc33f71"
            + "</ID>"
            + "<DisplayName>kirungeb</DisplayName></Grantee>"
            + "</Grant></AccessControlList>"
            + "</AccessControlPolicy>";

    String acl_multiple_grant = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" +
            "<AccessControlPolicy"
            + " xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
            + "<Owner><ID>"
            + "b103e16d027d24270d8facf37a48b141fd88ac8f43f9f942b91ba1cf1dc33f71"
            + "</ID>"
            + "<DisplayName>kirungeb</DisplayName></Owner><AccessControlList>"
            + "<Grant>"
            + "<Grantee "
            + "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
            + "xsi:type=\"CanonicalUser\"><ID>"
            + "b103e16d027d24270d8facf37a48b141fd88ac8f43f9f942b91ba1cf1dc33f71"
            + "</ID>"
            + "<DisplayName>kirungeb</DisplayName></Grantee>"
            + "<Permission>READ</Permission></Grant>"
            + "<Grant><Grantee "
            + "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
            + "xsi:type=\"CanonicalUser\"><ID>"
            + "24270d8facf37a48b141fd88ac8f43f9f942b91ba1cf1dc33f71b103e16d027d"
            + "</ID>"
            + "<DisplayName>myapp</DisplayName></Grantee>"
            + "<Permission>READ</Permission></Grant></AccessControlList>"
            + "</AccessControlPolicy>";

    String acl_malformed = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" +
            "<AccessControlPolicy"
            + " xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
            + "<Owner><ID>"
            + "b103e16d027d24270d8facf37a48b141fd88ac8f43f9f942b91ba1cf1dc33f71"
            + "</ID>"
            + "<DisplayName>kirungeb</DisplayName></Owner><AccessControlList>"
            + "<Grant>"
            + "<Grantee "
            + "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
            + "xsi:type=\"CanonicalUser\"><ID>"
            + "b103e16d027d24270d8facf37a48b141fd88ac8f43f9f942b91ba1cf1dc33f71"
            + "</ID>"
            + "<DisplayName>kirungeb</DisplayName></Grantee>"
            + "<Permission>READ</Permission></Grant>"
            + "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
            + "xsi:type=\"CanonicalUser\"><ID>"
            + "24270d8facf37a48b141fd88ac8f43f9f942b91ba1cf1dc33f71b103e16d027d"
            + "</ID>"
            + "<DisplayName>myapp</DisplayName></Grantee>"
            + "<Permission>READ</Permission></Grant></AccessControlList>"
            + "</AccessControlPolicy>";

    String acl_empty = "";

    String acl_null = null;

    String acl_namespace_ver = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" +
            "<AccessControlPolicy"
            + " xmlns=\"http://s3.amazonaws.com/doc/2010-03-01/\">"
            + "<Owner><ID>"
            + "b103e16d027d24270d8facf37a48b141fd88ac8f43f9f942b91ba1cf1dc33f71"
            + "</ID>"
            + "<DisplayName>kirungeb</DisplayName></Owner><AccessControlList>"
            + "<Grant>"
            + "<Grantee "
            + "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
            + "xsi:type=\"CanonicalUser\"><ID>"
            + "b103e16d027d24270d8facf37a48b141fd88ac8f43f9f942b91ba1cf1dc33f71"
            + "</ID>"
            + "<DisplayName>kirungeb</DisplayName></Grantee>"
            + "<Permission>FULL_CONTROL</Permission></Grant></AccessControlList>"
            + "</AccessControlPolicy>";

    @Test
    public void testValidACL() {
        try {
            XMLValidatorUtil xmlValidator = new XMLValidatorUtil(xsdPath);
            assertTrue(xmlValidator.validateXMLSchema(acl));
        } catch(SAXException ex) {
            fail("Failed to initialize XSD file "+ " xsdPath" + ex.getMessage());
        }
    }

    @Test
    public void testInvalidPermissionACL() {
           try {
               XMLValidatorUtil xmlValidator = new XMLValidatorUtil(xsdPath);
               assertFalse(xmlValidator.validateXMLSchema(acl_invalid_permission));
           } catch(SAXException ex) {
               fail("Failed to initialize XSD file "+ " xsdPath" + ex.getMessage());
        }
    }

    @Test
    public void testInvalidMissingID() {
           try {
               XMLValidatorUtil xmlValidator = new XMLValidatorUtil(xsdPath);
               assertFalse(xmlValidator.validateXMLSchema(acl_invalid_missingid));
           } catch(SAXException ex) {
               fail("Failed to initialize XSD file "+ " xsdPath" + ex.getMessage());
        }
    }

    @Test
    public void testInvalidNoGrantee() {
           try {
               XMLValidatorUtil xmlValidator = new XMLValidatorUtil(xsdPath);
               assertFalse(xmlValidator.validateXMLSchema(acl_invalid_nograntee));
           } catch(SAXException ex) {
               fail("Failed to initialize XSD file "+ " xsdPath" + ex.getMessage());
        }
    }

    @Test
    public void testInvalidNoOwner() {
        try {
              XMLValidatorUtil xmlValidator = new XMLValidatorUtil(xsdPath);
               assertFalse(xmlValidator.validateXMLSchema(acl_missing_owner));
           } catch(SAXException ex) {
               fail("Failed to initialize XSD file "+ " xsdPath" + ex.getMessage());
        }
    }

    @Test
    public void testInvalidNoPermission() {
           try {
               XMLValidatorUtil xmlValidator = new XMLValidatorUtil(xsdPath);
               assertFalse(xmlValidator.validateXMLSchema(acl_no_permission));
           } catch(SAXException ex) {
               fail("Failed to initialize XSD file "+ " xsdPath" + ex.getMessage());
        }
    }

    @Test
    public void testValidACLMultiple() {
           try {
               XMLValidatorUtil xmlValidator = new XMLValidatorUtil(xsdPath);
               assertTrue(xmlValidator.validateXMLSchema(acl_multiple_grant));
           } catch(SAXException ex) {
               fail("Failed to initialize XSD file "+ " xsdPath" + ex.getMessage());
        }
    }

    @Test
    public void testACLMalformed() {
           try {
               XMLValidatorUtil xmlValidator = new XMLValidatorUtil(xsdPath);
               assertFalse(xmlValidator.validateXMLSchema(acl_malformed));
           } catch(SAXException ex) {
               fail("Failed to initialize XSD file "+ " xsdPath" + ex.getMessage());
        }
    }

    @Test
    public void testEmptyACL() {
           try {
               XMLValidatorUtil xmlValidator = new XMLValidatorUtil(xsdPath);
               assertFalse(xmlValidator.validateXMLSchema(acl_empty));
           } catch(SAXException ex) {
               fail("Failed to initialize XSD file "+ " xsdPath" + ex.getMessage());
        }
    }

    @Test
    public void testInvalidVerACL() {
           try {
               XMLValidatorUtil xmlValidator = new XMLValidatorUtil(xsdPath);
               assertFalse(xmlValidator.validateXMLSchema(acl_namespace_ver));
           } catch(SAXException ex) {
               fail("Failed to initialize XSD file "+ " xsdPath" + ex.getMessage());
        }
    }

    @Test
    public void testMissingXSD() {
           try {
               XMLValidatorUtil xmlValidator = new XMLValidatorUtil("filenotfound.notfound");
               assertFalse(xmlValidator.validateXMLSchema(acl));
           } catch(SAXException ex) {
               assertTrue(ex.getMessage(), true);
        }
    }

    @Test
    public void testNullACL() {
           try {
               XMLValidatorUtil xmlValidator = new XMLValidatorUtil(xsdPath);
               assertFalse(xmlValidator.validateXMLSchema(acl_null));
           } catch(SAXException ex) {
               assertTrue(ex.getMessage(), true);
        }
    }

}
