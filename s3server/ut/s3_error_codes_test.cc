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
 * Original author:  Kaustubh Deorukhkar   <kaustubh.deorukhkar@seagate.com>
 * Original creation date: 1-Nov-2015
 */

#include "gtest/gtest.h"

#include "libxml/parser.h"
#include "libxml/xmlmemory.h"

#include "s3_error_codes.h"

// To use a test fixture, derive a class from testing::Test.
class S3ErrorTest : public testing::Test {
 protected:  // You should make the members protected s.t. they can be
             // accessed from sub-classes.

  // A helper function for testing xml content.
  xmlDocPtr get_parsed_doc(std::string& content) {
    xmlDocPtr document = xmlParseDoc((const xmlChar*)content.c_str());
    return document;
  }

  void is_valid_xml(std::string& xml_content) {
    xmlDocPtr document = get_parsed_doc(xml_content);
    ASSERT_FALSE(document == NULL);
    xmlFreeDoc(document);
  }

  void xmls_are_equal(std::string& first, std::string& second) {
    xmlDocPtr document_first = get_parsed_doc(first);
    ASSERT_FALSE(document_first == NULL);

    xmlChar* content_first = NULL;
    int size = 0;
    xmlDocDumpMemory(document_first, &content_first, &size);

    xmlDocPtr document_second = get_parsed_doc(second);
    ASSERT_FALSE(document_second == NULL);

    xmlChar* content_second = NULL;
    size = 0;
    xmlDocDumpMemory(document_second, &content_second, &size);

    ASSERT_TRUE(xmlStrcmp(content_first, content_second) == 0);

    xmlFree(content_first);
    xmlFreeDoc(document_first);
    xmlFree(content_second);
    xmlFreeDoc(document_second);
  }

  void has_element_with_value(std::string& xml_content,
                              std::string element_name,
                              std::string element_value) {
    xmlDocPtr document = get_parsed_doc(xml_content);
    ASSERT_FALSE(document == NULL);

    xmlNodePtr root_node = xmlDocGetRootElement(document);
    ASSERT_FALSE(root_node == NULL);

    // Now search for the element_name
    xmlNodePtr child = root_node->xmlChildrenNode;
    xmlChar* value = NULL;
    while (child != NULL) {
      if ((!xmlStrcmp(child->name, (const xmlChar*)element_name.c_str()))) {
        value = xmlNodeGetContent(child);
        ASSERT_FALSE(value == NULL);
        if (value == NULL) {
          xmlFree(value);
          xmlFreeDoc(document);
          return;
        }
        EXPECT_STREQ(element_value.c_str(), (char*)value);

        xmlFree(value);
        xmlFreeDoc(document);
        return;
      } else {
        child = child->next;
      }
    }
    FAIL() << "Element [" << element_name
           << "] is missing in the xml document [" << xml_content << "]";
  }

  // Declares the variables your tests want to use.
};

TEST_F(S3ErrorTest, HasValidHttpCodes) {
  S3Error error("BucketNotEmpty", "dummy-request-id", "SomeBucketName");
  EXPECT_EQ(409, error.get_http_status_code());
}

TEST_F(S3ErrorTest, ReturnValidErrorXml) {
  S3Error error("BucketNotEmpty", "dummy-request-id", "SomeBucketName");
  std::string xml_content = error.to_xml();
  is_valid_xml(xml_content);

  std::string expected_response =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  expected_response +=
      "<Error>\n"
      "<Code>BucketNotEmpty</Code>"
      "<Message>The bucket you tried to delete is not empty.</Message>"
      "<Resource>SomeBucketName</Resource>"
      "<RequestId>dummy-request-id</RequestId>"
      "</Error>\n";

  EXPECT_EQ(expected_response, xml_content);
  // xmls_are_equal(xml_content, expected_response);

  has_element_with_value(xml_content, "Message",
                         "The bucket you tried to delete is not empty.");
}

TEST_F(S3ErrorTest, Negative) {
  S3Error error("NegativeTestCase", "dummy-request-id", "SomeBucketName");
  std::string expected_response =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  expected_response +=
      "<Error>\n"
      "<Code>NegativeTestCase</Code>"
      "<Message>Unknown Error</Message>"
      "<Resource>SomeBucketName</Resource>"
      "<RequestId>dummy-request-id</RequestId>"
      "</Error>\n";
  EXPECT_EQ(520, error.get_http_status_code());
  EXPECT_EQ(expected_response, error.to_xml());
}
