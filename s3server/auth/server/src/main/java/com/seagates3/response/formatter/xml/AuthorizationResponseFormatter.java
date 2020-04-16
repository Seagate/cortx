/*
 * COPYRIGHT 2016 SEAGATE LLC
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
 * Original creation date: 27-May-2016
 */
package com.seagates3.response.formatter.xml;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.Map;

import javax.xml.parsers.ParserConfigurationException;
import javax.xml.transform.TransformerException;

import org.w3c.dom.Attr;
import org.w3c.dom.Document;
import org.w3c.dom.Element;

import com.seagates3.response.ServerResponse;
import com.seagates3.util.BinaryUtil;

import io.netty.handler.codec.http.HttpResponseStatus;

public class AuthorizationResponseFormatter extends XMLResponseFormatter {

    @Override
    public ServerResponse formatCreateResponse(String operation,
            String returnObject, LinkedHashMap<String, String> responseElements,
            String requestId) {
      throw new UnsupportedOperationException(
          "create operation not supported yet.");
    }

    @Override
    public ServerResponse formatDeleteResponse(String operation) {
      throw new UnsupportedOperationException(
          "delete operation not supported yet.");
    }

    @Override
    public ServerResponse formatUpdateResponse(String operation) {
      throw new UnsupportedOperationException(
          "update operation not supported yet.");
    }

    @Override
    public ServerResponse formatListResponse(String operation, String returnObject,
            ArrayList<LinkedHashMap<String, String>> responseElements,
            Boolean isTruncated, String requestId) {
      throw new UnsupportedOperationException(
          "list operation not supported yet.");
    }

   public
    ServerResponse authorized(LinkedHashMap<String, String> responseElements,
                              String requestId, String acp) {

      Document dcmt;
        try {
          dcmt = createNewDoc();
        } catch (ParserConfigurationException ex) {
            return null;
        }

        Element resp_element = dcmt.createElement("AuthorizeUserResponse");
        Attr attr = dcmt.createAttribute("xmlns");
        attr.setValue(IAM_XMLNS);
        resp_element.setAttributeNode(attr);
        dcmt.appendChild(resp_element);

        Element resultingElement = dcmt.createElement("AuthorizeUserResult");
        resp_element.appendChild(resultingElement);

        for (Map.Entry<String, String> entry : responseElements.entrySet()) {
          Element element = dcmt.createElement(entry.getKey());
          element.appendChild(dcmt.createTextNode(entry.getValue()));
          resultingElement.appendChild(element);
        }

        // Construct a default ACL and append as a child to resultElement
        if (acp != null)
          resultingElement.appendChild(constructDefaultACLElement(dcmt, acp));

        Element respMetaElement = dcmt.createElement("ResponseMetadata");
        resp_element.appendChild(respMetaElement);

        Element requestIdElement = dcmt.createElement("RequestId");
        requestIdElement.appendChild(dcmt.createTextNode(requestId));
        respMetaElement.appendChild(requestIdElement);

        String responseBody;
        try {
          responseBody = docToString(dcmt);
          ServerResponse finalServerResponse =
              new ServerResponse(HttpResponseStatus.OK, responseBody);

          return finalServerResponse;
        } catch (TransformerException ex) {
        }

        return null;
    }

    /**
     * Constructs an AccessControlPolicy (ACL) Element from acp -
     * XML, base64 encoded.
     * @param doc
     * @param acpXml
     * @return - the DefaultACL element
     */
   private
    Element constructDefaultACLElement(Document doc, String acpXml) {

      Element aclElement = null;

      if (acpXml != null) {
        aclElement = doc.createElement("ACL");
        aclElement.appendChild(
            doc.createTextNode(BinaryUtil.encodeToBase64String(acpXml)));
      }
      return aclElement;
    }
}

