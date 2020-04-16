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
 * Original creation date: 15-Dec-2015
 */
package com.seagates3.response.formatter.xml;

import java.util.LinkedHashMap;

import javax.xml.parsers.ParserConfigurationException;
import javax.xml.transform.TransformerException;

import org.w3c.dom.Attr;
import org.w3c.dom.Document;
import org.w3c.dom.Element;

import com.seagates3.response.ServerResponse;

import io.netty.handler.codec.http.HttpResponseStatus;

public class SAMLProviderResponseFormatter extends XMLResponseFormatter {

    @Override
    public ServerResponse formatCreateResponse(String operation, String returnObject,
            LinkedHashMap<String, String> responseElements, String requestId) {
      Document samlDoc;
        try {
          samlDoc = createNewDoc();
        } catch (ParserConfigurationException ex) {
            return null;
        }

        Element samlRootElement = samlDoc.createElement(operation + "Response");
        Attr attr = samlDoc.createAttribute("xmlns");
        attr.setValue(IAM_XMLNS);
        samlRootElement.setAttributeNode(attr);
        samlDoc.appendChild(samlRootElement);

        Element samlCreateUserResponse =
            samlDoc.createElement(operation + "Result");
        samlRootElement.appendChild(samlCreateUserResponse);

        Element arn = samlDoc.createElement("SAMLProviderArn");
        arn.appendChild(samlDoc.createTextNode(responseElements.get("Arn")));
        samlCreateUserResponse.appendChild(arn);

        Element samlResponseMetaData =
            samlDoc.createElement("ResponseMetadata");
        samlRootElement.appendChild(samlResponseMetaData);

        Element requestIdElement = samlDoc.createElement("RequestId");
        requestIdElement.appendChild(samlDoc.createTextNode(requestId));
        samlResponseMetaData.appendChild(requestIdElement);

        String responseBody;
        try {
          responseBody = docToString(samlDoc);
          ServerResponse samlServerResponse =
              new ServerResponse(HttpResponseStatus.CREATED, responseBody);

          return samlServerResponse;
        } catch (TransformerException ex) {
        }

        return null;
    }

    @Override
    public ServerResponse formatUpdateResponse(String operation) {
        throw new UnsupportedOperationException("This method is not supported");
    }

    public ServerResponse formatUpdateResponse(String name, String requestId) {
        Document doc;
        try {
            doc = createNewDoc();
        } catch (ParserConfigurationException ex) {
            return null;
        }

        Element rootElement = doc.createElement("UpdateSAMLProviderResponse");
        Attr attr = doc.createAttribute("xmlns");
        attr.setValue(IAM_XMLNS);
        rootElement.setAttributeNode(attr);
        doc.appendChild(rootElement);

        Element updateUserResponse = doc.createElement("UpdateSAMLProviderResult");
        rootElement.appendChild(updateUserResponse);

        String arnValue = String.format("arn:seagate:iam:::%s", name);
        Element arn = doc.createElement("SAMLProviderArn");
        arn.appendChild(doc.createTextNode(arnValue));
        updateUserResponse.appendChild(arn);

        Element responseMetaData = doc.createElement("ResponseMetadata");
        rootElement.appendChild(responseMetaData);

        Element requestIdElement = doc.createElement("RequestId");
        requestIdElement.appendChild(doc.createTextNode(requestId));
        responseMetaData.appendChild(requestIdElement);

        String responseBody;
        try {
            responseBody = docToString(doc);
            ServerResponse serverResponse = new ServerResponse(HttpResponseStatus.OK,
                    responseBody);

            return serverResponse;
        } catch (TransformerException ex) {
        }

        return null;
    }

}

