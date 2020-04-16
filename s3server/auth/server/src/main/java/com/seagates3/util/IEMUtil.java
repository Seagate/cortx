/*
 * COPYRIGHT 2017 SEAGATE LLC
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
 * Original author: Sushant Mane <sushant.mane@seagate.com>
 * Original creation date: 03-Mar-2017
 */

package com.seagates3.util;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class IEMUtil {

  public
   enum Level {
     INFO,
     ERROR,
     WARN
   } private static Logger logger =
       LoggerFactory.getLogger(BinaryUtil.class.getName());

  public
   static final String LDAP_EX = "0040010001";
  public
   static final String UTF8_UNAVAILABLE = "0040020001";
  public
   static final String HMACSHA256_UNAVAILABLE = "0040020002";
  public
   static final String HMACSHA1_UNAVAILABLE = "0040020003";
  public
   static final String SHA256_UNAVAILABLE = "0040020004";
  public
   static final String UNPARSABLE_DATE = "0040030001";
  public
   static final String CLASS_NOT_FOUND_EX = "0040040001";
  public
   static final String NO_SUCH_METHOD_EX = "0040040002";
  public
   static final String XML_SCHEMA_VALIDATION_ERROR = "0040050001";
  public
   static final String INVALID_REST_URI_ERROR = "0040060001";

  public
   static void log(Level level, String eventCode, String eventCodeString,
                   String data) {
     String iem = generateIemString(eventCode, eventCodeString, level);
        switch (level) {
          case ERROR:
            logger.error(iem);
            break;
          case WARN:
            logger.warn(iem);
            break;
          case INFO:
            logger.info(iem);
        }
    }

   private
    static String generateIemString(String eventCode, String eventCodeString,
                                    Level level) {
      String severity = null;
      switch (level) {
        case ERROR:
          severity = "E";
          break;
        case WARN:
          severity = "W";
          break;
        case INFO:
          severity = "I";
        }
        String iem =
            String.format("IEC:%sS%s:%s", severity, eventCode, eventCodeString);
        return iem;
    }
}

