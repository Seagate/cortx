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
 * Original author:  Rajesh Nambiar   <rajesh.nambiar@seagate.com>
 * Author:  Kaustubh Deorukhkar   <kaustubh.deorukhkar@seagate.com>
 * Original creation date: 10-Mar-2016
 */

#include <cstring>

#include "s3_url_encode.h"

void escape_char(char ch, std::string& destination) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%%%.2X", (int)(unsigned char)ch);
  destination.append(buf);
}

bool char_needs_url_encoding(char c) {
  if (c <= 0x20 || c >= 0x7f) return true;

  switch (c) {
    case 0x22:
    case 0x23:
    case 0x25:
    case 0x26:
    case 0x2B:
    case 0x2C:
    case 0x2F:
    case 0x3A:
    case 0x3B:
    case 0x3C:
    case 0x3E:
    case 0x3D:
    case 0x3F:
    case 0x40:
    case 0x5B:
    case 0x5D:
    case 0x5C:
    case 0x5E:
    case 0x60:
    case 0x7B:
    case 0x7D:
      return true;
  }
  return false;
}

std::string url_encode(const char* src) {
  if (src == NULL) {
    return "";
  }
  std::string encoded_string = "";
  size_t len = strlen(src);
  for (size_t i = 0; i < len; i++, src++) {
    if (char_needs_url_encoding(*src)) {
      escape_char(*src, encoded_string);
    } else {
      encoded_string.append(src, 1);
    }
  }
  return encoded_string;
}
