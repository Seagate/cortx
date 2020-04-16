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

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.IOException;

import javax.xml.XMLConstants;
import javax.xml.transform.stream.StreamSource;
import javax.xml.validation.SchemaFactory;
import javax.xml.validation.Validator;

import org.xml.sax.SAXException;

import javax.xml.validation.Schema;

/**
 * Utility Class to Validate XML using XSD schema file
 */
public class XMLValidatorUtil {

    private Validator validator;

    public XMLValidatorUtil(String xsdPath) throws SAXException {

        SchemaFactory factory = SchemaFactory.newInstance(XMLConstants.W3C_XML_SCHEMA_NS_URI);

        Schema schema;
        schema = factory.newSchema(new File(xsdPath));
        validator = schema.newValidator();
    }

    /**
     * Method to validate XML
     * @param xml ACL xml string
     * @return true if validation succeeds else false
     */
    public boolean validateXMLSchema(String xml) {

        if(xml == null || xml.isEmpty()) {
            IEMUtil.log(IEMUtil.Level.ERROR,
                    IEMUtil.XML_SCHEMA_VALIDATION_ERROR, "Invalid xml", null);
            return false;
        }
        try {
            validator.validate(new StreamSource(new ByteArrayInputStream(xml.getBytes())));

        } catch (SAXException | IOException e) {
           IEMUtil.log(IEMUtil.Level.ERROR, IEMUtil.XML_SCHEMA_VALIDATION_ERROR,
                    "XML Schema Validation Failed Reason:" + e.getMessage(), null);

           return false;
        }
        return true;
    }
}
