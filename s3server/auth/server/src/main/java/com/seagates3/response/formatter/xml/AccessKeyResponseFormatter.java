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
 * Original creation date: 13-Dec-2015
 */
package com.seagates3.response.formatter.xml;

import com.seagates3.response.ServerResponse;
import io.netty.handler.codec.http.HttpResponseStatus;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.Map;
import javax.xml.parsers.ParserConfigurationException;
import javax.xml.transform.TransformerException;
import org.w3c.dom.Attr;
import org.w3c.dom.Document;
import org.w3c.dom.Element;

public class AccessKeyResponseFormatter extends XMLResponseFormatter {

    @Override
    public ServerResponse formatListResponse(String operation, String returnObject,
            ArrayList<LinkedHashMap<String, String>> responseElements,
            Boolean isTruncated, String requestId) {
        throw new UnsupportedOperationException("Not supported yet.");
    }

    public ServerResponse formatListResponse(String userName,
            ArrayList<LinkedHashMap<String, String>> responseElements,
            Boolean isTruncated, String requestId) {

        Document doc;
        try {
            doc = createNewDoc();
        } catch (ParserConfigurationException ex) {
            return null;
        }

        Element responseElement = doc.createElement("ListAccessKeysResponse");
        Attr attr = doc.createAttribute("xmlns");
        attr.setValue(IAM_XMLNS);
        responseElement.setAttributeNode(attr);
        doc.appendChild(responseElement);

        Element resultElement = doc.createElement("ListAccessKeysResult");
        responseElement.appendChild(resultElement);

        Element userNameElement = doc.createElement("UserName");
        userNameElement.appendChild(doc.createTextNode(userName));
        resultElement.appendChild(userNameElement);

        Element accessKeyMetadataEle = doc.createElement("AccessKeyMetadata");
        resultElement.appendChild(accessKeyMetadataEle);

        for (HashMap<String, String> member : responseElements) {
            Element memberElement = doc.createElement("member");
            accessKeyMetadataEle.appendChild(memberElement);

            for (Map.Entry<String, String> entry : member.entrySet()) {
                Element element = doc.createElement(entry.getKey());
                element.appendChild(doc.createTextNode(entry.getValue()));
                memberElement.appendChild(element);
            }
        }

        Element isTruncatedElement = doc.createElement("IsTruncated");
        isTruncatedElement.appendChild(doc.createTextNode(isTruncated.toString()));
        resultElement.appendChild(isTruncatedElement);

        Element responseMetaData = doc.createElement("ResponseMetadata");
        responseElement.appendChild(responseMetaData);

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
