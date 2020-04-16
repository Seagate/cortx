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
 * Original creation date: 12-Dec-2015
 */
package com.seagates3.response.formatter.xml;

import java.io.StringWriter;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.Map;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;
import javax.xml.transform.Transformer;
import javax.xml.transform.TransformerConfigurationException;
import javax.xml.transform.TransformerException;
import javax.xml.transform.TransformerFactory;
import javax.xml.transform.dom.DOMSource;
import javax.xml.transform.stream.StreamResult;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.w3c.dom.Attr;
import org.w3c.dom.Document;
import org.w3c.dom.Element;

import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.formatter.AbstractResponseFormatter;

import io.netty.handler.codec.http.HttpResponseStatus;

public
class XMLResponseFormatter extends AbstractResponseFormatter {

 private
  final Logger LOGGER =
      LoggerFactory.getLogger(XMLResponseFormatter.class.getName());

  @Override public ServerResponse formatCreateResponse(
      String operation, String returnObject,
      LinkedHashMap<String, String> responseElements, String requestId) {
    Document document;
    try {
      document = createNewDoc();
    }
    catch (ParserConfigurationException e) {
      return null;
    }

    Element response = document.createElement(operation + "Response");
    Attr attribute = document.createAttribute("xmlns");
    attribute.setValue(IAM_XMLNS);
    response.setAttributeNode(attribute);
    document.appendChild(response);

    Element result = document.createElement(operation + "Result");
    response.appendChild(result);

    Element returnedObject = document.createElement(returnObject);
    result.appendChild(returnedObject);

    for (Map.Entry<String, String> entryElement : responseElements.entrySet()) {
      Element element = document.createElement(entryElement.getKey());
      element.appendChild(document.createTextNode(entryElement.getValue()));
      returnedObject.appendChild(element);
    }

    Element metadata = document.createElement("ResponseMetadata");
    response.appendChild(metadata);

    Element requestElement = document.createElement("RequestId");
    requestElement.appendChild(document.createTextNode(requestId));
    metadata.appendChild(requestElement);

    String response_body;
    try {
      response_body = docToString(document);
      ServerResponse serverResponse =
          new ServerResponse(HttpResponseStatus.CREATED, response_body);
      return serverResponse;
    }
    catch (TransformerException ex) {
      LOGGER.error("Unable to decode response body");
      LOGGER.error(ex.toString());
    }

    return null;
    }

    @Override public ServerResponse formatListResponse(
        String operation, String returnObject,
        ArrayList<LinkedHashMap<String, String>> responseElements,
        Boolean isTruncated, String requestId) {

      Document doc;
      try {
        doc = createNewDoc();
      }
      catch (ParserConfigurationException ex) {
        return null;
      }
      Element responseElement = doc.createElement(operation + "Response");
      Attr attr = doc.createAttribute("xmlns");
      attr.setValue(IAM_XMLNS);
      responseElement.setAttributeNode(attr);
      doc.appendChild(responseElement);
      Element resultElement = doc.createElement(operation + "Result");
      responseElement.appendChild(resultElement);

      Element returnObjElement = doc.createElement(returnObject);
      resultElement.appendChild(returnObjElement);

      for (HashMap<String, String> member : responseElements) {
        Element memberElement = doc.createElement("member");
        returnObjElement.appendChild(memberElement);

        for (Map.Entry<String, String> entry : member.entrySet()) {
          Element element = doc.createElement(entry.getKey());
          element.appendChild(doc.createTextNode(entry.getValue()));
          memberElement.appendChild(element);
        }
      }
      Element isTruncatedElement = doc.createElement("IsTruncated");
      isTruncatedElement.appendChild(
          doc.createTextNode(isTruncated.toString()));
      resultElement.appendChild(isTruncatedElement);
      Element responseMetaData = doc.createElement("ResponseMetadata");
      responseElement.appendChild(responseMetaData);

      Element requestIdElement = doc.createElement("RequestId");
      requestIdElement.appendChild(doc.createTextNode(requestId));
      responseMetaData.appendChild(requestIdElement);

      String responseBody;
      try {
        responseBody = docToString(doc);
        ServerResponse serverResponse =
            new ServerResponse(HttpResponseStatus.OK, responseBody);

        return serverResponse;
      }
      catch (TransformerException ex) {
      }

      return null;
    }

    @Override public ServerResponse formatDeleteResponse(String operation) {
      return success(operation);
    }

    @Override public ServerResponse formatUpdateResponse(String operation) {
      return success(operation);
    }

    @Override public ServerResponse formatChangePasswordResponse(
        String operation) {
      return success(operation);
    }

    @Override public ServerResponse formatErrorResponse(
        HttpResponseStatus httpResponseStatus, String code, String message) {
      Document doc;
      try {
        doc = createNewDoc();
      }
      catch (ParserConfigurationException ex) {
        return null;
      }

      Element root = doc.createElement("ErrorResponse");
      Attr attr = doc.createAttribute("xmlns");
      attr.setValue(IAM_XMLNS);
      root.setAttributeNode(attr);
      doc.appendChild(root);

      Element errorEle = doc.createElement("Error");
      root.appendChild(errorEle);

      Element codeEle = doc.createElement("Code");
      codeEle.appendChild(doc.createTextNode(code));
      errorEle.appendChild(codeEle);

      Element messageEle = doc.createElement("Message");
      messageEle.appendChild(doc.createTextNode(message));
      errorEle.appendChild(messageEle);

      Element requestIdEle = doc.createElement("RequestId");
      requestIdEle.appendChild(doc.createTextNode(AuthServerConfig.getReqId()));
      root.appendChild(requestIdEle);

      String responseBody;
      try {
        responseBody = docToString(doc);
        LOGGER.debug(
            "XMLResponseFormatter :: formatErrorResponse() - responseBody is " +
            "- " + " " + responseBody);
        ServerResponse serverResponse =
            new ServerResponse(httpResponseStatus, responseBody);

        return serverResponse;
      }
      catch (TransformerException ex) {
      }

      /**
       * TODO - Return a failed exception. Otherwise the client will not know
       * the
       * reason for the failure.
       */
      return null;
    }

   private
    ServerResponse success(String operation) {
      Document doc;
      try {
        doc = createNewDoc();
      }
      catch (ParserConfigurationException ex) {
        return null;
      }

      Element rootElement = doc.createElement(operation + "Response");
      Attr attr = doc.createAttribute("xmlns");
      attr.setValue(IAM_XMLNS);
      rootElement.setAttributeNode(attr);
      doc.appendChild(rootElement);

      Element responseMetaData = doc.createElement("ResponseMetadata");
      rootElement.appendChild(responseMetaData);

      Element requestId = doc.createElement("RequestId");
      requestId.appendChild(doc.createTextNode(AuthServerConfig.getReqId()));
      responseMetaData.appendChild(requestId);

      String responseBody;
      try {
        responseBody = docToString(doc);
        ServerResponse serverResponse =
            new ServerResponse(HttpResponseStatus.OK, responseBody);

        return serverResponse;
      }
      catch (TransformerException ex) {
      }

      return null;
    }

   protected
    Document createNewDoc() throws ParserConfigurationException {
      DocumentBuilderFactory docFactory = DocumentBuilderFactory.newInstance();
      DocumentBuilder docBuilder = docFactory.newDocumentBuilder();

      return docBuilder.newDocument();
    }

   protected
    String docToString(Document doc) throws TransformerConfigurationException,
        TransformerException {
      DOMSource domSource = new DOMSource(doc);
      StringWriter writer = new StringWriter();
      StreamResult result = new StreamResult(writer);
      TransformerFactory tf = TransformerFactory.newInstance();
      Transformer transformer = tf.newTransformer();
      transformer.transform(domSource, result);

      return writer.toString();
    }

   public
    ServerResponse formatResetAccountAccessKeyResponse(
        String operation, String returnObject,
        LinkedHashMap<String, String> responseElements, String requestId) {
      Document documentObj;
      try {
        documentObj = createNewDoc();
      }
      catch (ParserConfigurationException exception) {
        return null;
      }

      Element response_element =
          documentObj.createElement(operation + "Response");
      Attr attr = documentObj.createAttribute("xmlns");
      attr.setValue(IAM_XMLNS);
      response_element.setAttributeNode(attr);
      documentObj.appendChild(response_element);

      Element result_element = documentObj.createElement(operation + "Result");
      response_element.appendChild(result_element);

      Element returnobj_element = documentObj.createElement(returnObject);
      result_element.appendChild(returnobj_element);

      for (Map.Entry<String, String> entry_element :
           responseElements.entrySet()) {
        Element new_element = documentObj.createElement(entry_element.getKey());
        new_element.appendChild(
            documentObj.createTextNode(entry_element.getValue()));
        returnobj_element.appendChild(new_element);
      }

      Element response_metadata = documentObj.createElement("ResponseMetadata");
      response_element.appendChild(response_metadata);

      Element requestid_element = documentObj.createElement("RequestId");
      requestid_element.appendChild(documentObj.createTextNode(requestId));
      response_metadata.appendChild(requestid_element);

      String responseBody;
      try {
        responseBody = docToString(documentObj);
        ServerResponse serverResponse =
            new ServerResponse(HttpResponseStatus.CREATED, responseBody);

        return serverResponse;
      }
      catch (TransformerException ex) {
        LOGGER.error("Unable to decode response body");
        LOGGER.error(ex.toString());
      }

      return null;
    }

    @Override public ServerResponse formatGetResponse(
        String operation, String returnObject,
        ArrayList<LinkedHashMap<String, String>> responseElements,
        String requestId) {

      Document document_object;
      try {
        document_object = createNewDoc();
      }
      catch (ParserConfigurationException ex) {
        return null;
      }
      Element resElement =
          document_object.createElement(operation + "Response");
      Attr attrib = document_object.createAttribute("xmlns");
      attrib.setValue(IAM_XMLNS);
      resElement.setAttributeNode(attrib);
      document_object.appendChild(resElement);
      Element resultedElement =
          document_object.createElement(operation + "Result");
      resElement.appendChild(resultedElement);

      Element returnObjElement = document_object.createElement(returnObject);
      resultedElement.appendChild(returnObjElement);

      for (HashMap<String, String> member : responseElements) {
        for (Map.Entry<String, String> memberEntry : member.entrySet()) {
          Element newElement =
              document_object.createElement(memberEntry.getKey());
          newElement.appendChild(
              document_object.createTextNode(memberEntry.getValue()));
          returnObjElement.appendChild(newElement);
        }
      }
      Element respMetaData = document_object.createElement("ResponseMetadata");
      resElement.appendChild(respMetaData);

      Element request_IdElement = document_object.createElement("RequestId");
      request_IdElement.appendChild(document_object.createTextNode(requestId));
      respMetaData.appendChild(request_IdElement);

      String respBody;
      try {
        respBody = docToString(document_object);
        ServerResponse server_response =
            new ServerResponse(HttpResponseStatus.OK, respBody);
        return server_response;
      }
      catch (TransformerException excp) {
        LOGGER.error("Exception occurred - " + excp);
      }

      return null;
    }
}

