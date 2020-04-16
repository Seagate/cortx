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
 * Original creation date: 26-Jan-2016
 */
package com.seagates3.response.formatter.xml;

import java.util.LinkedHashMap;
import java.util.Map;

import javax.xml.parsers.ParserConfigurationException;
import javax.xml.transform.TransformerException;

import org.w3c.dom.Attr;
import org.w3c.dom.Document;
import org.w3c.dom.Element;

import com.seagates3.response.ServerResponse;

import io.netty.handler.codec.http.HttpResponseStatus;

public class SAMLSessionResponseFormatter extends XMLResponseFormatter {

    @Override
    public ServerResponse formatCreateResponse(String operation, String returnObject,
            LinkedHashMap<String, String> responseElements, String requestId) {
      throw new UnsupportedOperationException("create Not supported yet.");
    }

    public ServerResponse formatCreateReponse(
            LinkedHashMap<String, String> credentials,
            LinkedHashMap<String, String> userDetails,
            String requestId) {

       Document saml_session_doc;
        try {
          saml_session_doc = createNewDoc();
        } catch (ParserConfigurationException ex) {
            return null;
        }

        Element samlResponseElement =
            saml_session_doc.createElement("SessionTokenResponse");
        Attr attr = saml_session_doc.createAttribute("xmlns");
        attr.setValue(IAM_XMLNS);
        samlResponseElement.setAttributeNode(attr);
        saml_session_doc.appendChild(samlResponseElement);

        Element samlResultElement =
            saml_session_doc.createElement("SessionTokenResult");
        samlResponseElement.appendChild(samlResultElement);

        Element credElement = saml_session_doc.createElement("Credentials");
        samlResultElement.appendChild(credElement);
        for (Map.Entry<String, String> credentialEntry :
             credentials.entrySet()) {
          Element element =
              saml_session_doc.createElement(credentialEntry.getKey());
          element.appendChild(
              saml_session_doc.createTextNode(credentialEntry.getValue()));
            credElement.appendChild(element);
        }

        Element userElement = saml_session_doc.createElement("User");
        samlResultElement.appendChild(userElement);
        for (Map.Entry<String, String> entry : userDetails.entrySet()) {
          Element element = saml_session_doc.createElement(entry.getKey());
          element.appendChild(
              saml_session_doc.createTextNode(entry.getValue()));
            userElement.appendChild(element);
        }

        Element samlResponseMetadataElement =
            saml_session_doc.createElement("ResponseMetadata");
        samlResponseElement.appendChild(samlResponseMetadataElement);

        Element samlRequestIdElement =
            saml_session_doc.createElement("RequestId");
        samlRequestIdElement.appendChild(
            saml_session_doc.createTextNode(requestId));
        samlResponseMetadataElement.appendChild(samlRequestIdElement);

        String responseBody;
        try {
          responseBody = docToString(saml_session_doc);
          ServerResponse saml_session_server_response =
              new ServerResponse(HttpResponseStatus.OK, responseBody);

          return saml_session_server_response;
        } catch (TransformerException ex) {
        }

        return null;
    }
}

