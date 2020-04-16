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
 * Original author:  Siddhivinayak Shanbhag <siddhivinayak.shanbhag@seagate.com>
 * Original creation date: 07-January-2019
 */

#include <cctype>
#include <algorithm>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include <regex>
#include "s3_log.h"
#include "s3_put_tag_body.h"

S3PutTagBody::S3PutTagBody(std::string &xml, std::string &request)
    : xml_content(xml), request_id(request), is_valid(false) {

  s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");
  if (!xml.empty()) {
    parse_and_validate();
  }
}

bool S3PutTagBody::isOK() { return is_valid; }

bool S3PutTagBody::parse_and_validate() {
  /* Sample body:
  <Tagging>
   <TagSet>
     <Tag>
       <Key>Project</Key>
       <Value>Project One</Value>
     </Tag>
     <Tag>
       <Key>User</Key>
       <Value>jsmith</Value>
     </Tag>
   </TagSet>
  </Tagging>

  */
  s3_log(S3_LOG_DEBUG, request_id, "Parsing put bucket tag body\n");

  bucket_tags.clear();
  is_valid = false;

  if (xml_content.empty()) {
    s3_log(S3_LOG_WARN, request_id, "XML request body Empty.\n");
    return is_valid;
  }
  s3_log(S3_LOG_DEBUG, request_id, "Parsing xml request = %s\n",
         xml_content.c_str());
  xmlDocPtr document = xmlParseDoc((const xmlChar *)xml_content.c_str());
  if (document == NULL) {
    s3_log(S3_LOG_WARN, request_id, "XML request body Invalid.\n");
    return is_valid;
  }

  xmlNodePtr tagging_node = xmlDocGetRootElement(document);

  s3_log(S3_LOG_DEBUG, request_id, "Root Node = %s", tagging_node->name);
  // Validate the root node
  if (tagging_node == NULL ||
      xmlStrcmp(tagging_node->name, (const xmlChar *)"Tagging")) {
    s3_log(S3_LOG_WARN, request_id, "XML request body Invalid.\n");
    xmlFreeDoc(document);
    return is_valid;
  }

  // Validate child node
  xmlNodePtr tagset_node = tagging_node->xmlChildrenNode;
  if (tagset_node != NULL) {
    s3_log(S3_LOG_DEBUG, request_id, "Child Node = %s", tagset_node->name);
    if ((!xmlStrcmp(tagset_node->name, (const xmlChar *)"TagSet"))) {
      // Empty TagSet is also an valid node.
      is_valid = true;
    }
  } else {
    s3_log(S3_LOG_WARN, request_id, "XML request body Invalid.\n");
    return is_valid;
  }

  // Validate sub-child node
  xmlNodePtr tag_node = tagset_node->xmlChildrenNode;
  while (tag_node != NULL) {
    s3_log(S3_LOG_DEBUG, request_id, "Sub Child Node = %s", tag_node->name);
    if ((!xmlStrcmp(tag_node->name, (const xmlChar *)"Tag"))) {
      unsigned long key_count = xmlChildElementCount(tag_node);
      s3_log(S3_LOG_DEBUG, request_id, "Tag node children count=%ld",
             key_count);
      if (key_count != 2) {
        s3_log(S3_LOG_WARN, request_id, "XML request body Invalid.\n");
        xmlFreeDoc(document);
        is_valid = false;
        return is_valid;
      }

      is_valid = read_key_value_node(tag_node);
      if (!is_valid) {
        s3_log(S3_LOG_WARN, request_id, "XML request body Invalid.\n");
        return is_valid;
      }
      tag_node = tag_node->next;
    }
  }

  xmlFreeDoc(document);
  return is_valid;
}

bool S3PutTagBody::read_key_value_node(xmlNodePtr &tag_node) {
  // Validate key values node
  xmlNodePtr key_value_node = tag_node->xmlChildrenNode;
  std::string tag_key, tag_value;
  while (key_value_node && key_value_node->name) {
    s3_log(S3_LOG_DEBUG, request_id, "Key_Value_node = %s",
           key_value_node->name);
    xmlChar *val = xmlNodeGetContent(key_value_node);

    if (!(xmlStrcmp(key_value_node->name, (const xmlChar *)"Key"))) {
      tag_key = reinterpret_cast<char *>(val);
    } else if (!(xmlStrcmp(key_value_node->name, (const xmlChar *)"Value"))) {
      tag_value = reinterpret_cast<char *>(val);
    } else {
      s3_log(S3_LOG_WARN, request_id,
             "XML request body Invalid: unknown tag %s.\n",
             reinterpret_cast<const char *>(key_value_node->name));
      xmlFree(val);
      return false;
    }
    xmlFree(val);
    // Only a single pair of Key-Value exists within Tag Node.
    key_value_node = key_value_node->next;
  }

  if (tag_key.empty() || tag_value.empty()) {
    s3_log(S3_LOG_WARN, request_id, "XML request body Invalid: empty node.\n");
    return false;
  }
  if (bucket_tags.count(tag_key) >= 1) {
    s3_log(S3_LOG_WARN, request_id,
           "XML request body Invalid: tag duplication.\n");
    return false;
  }

  s3_log(S3_LOG_DEBUG, request_id, "Add tag %s = %s ", tag_key.c_str(),
         tag_value.c_str());
  bucket_tags[tag_key] = tag_value;

  return true;
}

static size_t utf8_len(const std::string &s) {
  return count_if(s.begin(), s.end(),
                  [](char c) { return (c & 0xC0) != 0x80; });
}

bool S3PutTagBody::validate_bucket_xml_tags(
    std::map<std::string, std::string> &bucket_tags_as_map) {
  // Apply all validation here
  // https://docs.aws.amazon.com/awsaccountbilling/latest/aboutv2//allocation-tag-restrictions.html;

  // Invalid characters in bucket tagging
  const std::string invalid_char_str = "\\!{}`^%<>[]#|~?@*/";

  // Maximum number of tags per resource: 50
  if (bucket_tags_as_map.size() > BUCKET_MAX_TAGS) {
    s3_log(S3_LOG_WARN, request_id, "XML key-value tags Invalid.\n");
    return false;
  }
  for (const auto &tagkv : bucket_tags_as_map) {
    // Key-value pairs for bucket tagging should not be empty
    if (tagkv.first.empty() || tagkv.second.empty()) {
      s3_log(S3_LOG_WARN, request_id, "XML key-value tag Invalid.\n");
      return false;
    }
    /* To encode 256 unicode chars, last char can use upto 2 bytes.
     Core reason is std::string stores utf-8 bytes and hence .length()
     returns bytes and not number of chars.
     For refrence : https://stackoverflow.com/a/31652705
     */
    // Maximum key length: 128 Unicode characters &
    // Maximum value length: 256 Unicode characters
    if (utf8_len(tagkv.first) > TAG_KEY_MAX_LENGTH ||
        utf8_len(tagkv.second) > TAG_VALUE_MAX_LENGTH) {
      s3_log(S3_LOG_WARN, request_id, "XML key-value tag Invalid.\n");
      return false;
    }
    // Allowed characters are Unicode letters, whitespace, and numbers, plus the
    // following special characters: + - = . _ : /
    // Insignificant check, as it matches all entries.
    /*
    std::regex matcher ("((\\w|\\W|\\b|\\B|-|=|.|_|:|\/)+)");
    if ( !regex_match(key,matcher) || !regex_match (value,matcher) ) {
      s3_log(S3_LOG_WARN, request_id, "XML key-value tag Invalid.\n");
      return false;
    }
    */
    // Handle invalid character(s) in Key or Value node
    std::size_t found = tagkv.first.find_first_of(invalid_char_str);
    if (std::string::npos != found) {
      s3_log(S3_LOG_WARN, request_id,
             "Tag Key[%s] contains an invalid character[%c].\n",
             tagkv.first.c_str(), tagkv.first[found]);
      return false;
    }
    found = tagkv.second.find_first_of(invalid_char_str);
    if (std::string::npos != found) {
      s3_log(S3_LOG_WARN, request_id,
             "Tag Value[%s] contains an invalid character[%c].\n",
             tagkv.second.c_str(), tagkv.second[found]);
      return false;
    }
  }
  return true;
}

static bool tag_is_allowed_char(char ch) {
  /*
    AWS defined the following restrictions for characters allowed in tags:
    "The allowed characters across services are: letters, numbers, and spaces
    representable in UTF-8, and the following characters: + - = . _ : / @."
    This can be read two ways:
    1) simple: allow all UTF-8 char, but limit ASCII chars,
    2) complex: limit ASCII chars, and do not allow UTF-8 sequences which are
    not letter/number/space. Code below implements "simple" approach, we
    do not analyze UTF-8 sequences for begin letter/number/space, and simply
    allow all UTF-8 chars.
  */
  if (!(ch & 0x80)) {
    // A character is ASCII [0..127]
    if (isalnum(ch) || isspace(ch)) {
      return true;
    }
    switch (ch) {
      case '+':
      case '-':
      case '=':
      case '.':
      case '_':
      case ':':
      case '/':
      case '@':
        return true;
    }
    return false;
  }
  /* According to UTF-8 definition
    (e.g. https://en.wikipedia.org/wiki/UTF-8#Description), valid UTF-8 bytes
     would are the following: 10xxxxxx, 110xxxxx, 1110xxxx, 11110xxx.
     The check below returns true for every valid UTF-8 sequence.
     Some invalid sequences will also return true, but checking for those
     will complicate the code, and seems to not be required at the moment.
     So we'll go with the simple check.
  */
  if ((ch & 0xC0) == 0x80 || (ch & 0xE0) == 0xC0 || (ch & 0xF0) == 0xE0 ||
      (ch & 0xF8) == 0xF0) {
    return true;
  }
  return false;
}

static bool is_valid_object_tag(const std::string &tag) {
  return std::all_of(tag.cbegin(), tag.cend(), &tag_is_allowed_char);
}

bool S3PutTagBody::validate_object_xml_tags(
    std::map<std::string, std::string> &object_tags_as_map) {
  // Apply all validation here
  // https://docs.aws.amazon.com/awsaccountbilling/latest/aboutv2//allocation-tag-restrictions.html
  // &;
  // https://docs.aws.amazon.com/AmazonS3/latest/API/RESTObjectPUTtagging.html
  std::string key, value;
  // Maximum number of tags per resource: 10
  if (object_tags_as_map.size() > OBJECT_MAX_TAGS) {
    s3_log(S3_LOG_WARN, request_id, "XML key-value tags Invalid.\n");
    return false;
  }
  for (const auto &tag : object_tags_as_map) {
    key = tag.first;
    value = tag.second;
    // Key-value pairs for bucket tagging should not be empty
    if (key.empty() || value.empty()) {
      s3_log(S3_LOG_WARN, request_id, "XML key-value tag Invalid.\n");
      return false;
    }
    /* To encode 256 unicode chars, last char can use upto 2 bytes.
     Core reason is std::string stores utf-8 bytes and hence .length()
     returns bytes and not number of chars.
     For refrence : https://stackoverflow.com/a/31652705
     */
    // Maximum key length: 128 Unicode characters &
    // Maximum value length: 256 Unicode characters
    if (utf8_len(key) > TAG_KEY_MAX_LENGTH ||
        utf8_len(value) > TAG_VALUE_MAX_LENGTH) {
      s3_log(S3_LOG_WARN, request_id, "XML key-value tag Invalid.\n");
      return false;
    }
    // Allowed characters are Unicode letters, whitespace, and numbers, plus the
    // following special characters: + - = . _ : /
    // Insignificant check, as it matches all entries.
    /*
    std::regex matcher ("((\\w|\\W|\\b|\\B|-|=|.|_|:|\/)+)");
    if ( !regex_match(key,matcher) || !regex_match (value,matcher) ) {
      s3_log(S3_LOG_WARN, request_id, "XML key-value tag Invalid.\n");
      return false;
    }
    */
    if (!is_valid_object_tag(key) || !is_valid_object_tag(value)) {
      s3_log(S3_LOG_WARN, request_id, "XML key-value tag Invalid.\n");
      return false;
    }
  }
  return true;
}

const std::map<std::string, std::string> &
S3PutTagBody::get_resource_tags_as_map() {
  return bucket_tags;
}
