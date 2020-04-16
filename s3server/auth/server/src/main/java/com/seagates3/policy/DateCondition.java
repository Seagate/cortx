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
 * Original author:  Ajinkya Dhumal
 * Original creation date: 26-December-2019
 */

package com.seagates3.policy;

import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.regex.Pattern;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.amazonaws.util.DateUtils;
import com.seagates3.util.DateUtil;

public
class DateCondition extends PolicyCondition {

 private
  static List<Date> conditionDates;
 protected
  final Logger LOGGER = LoggerFactory.getLogger(DateCondition.class.getName());

 public
  static enum DateComparisonType {
    DateEquals,
    DateGreaterThan,
    DateGreaterThanEquals,
    DateLessThan,
    DateLessThanEquals,
    DateNotEquals,
    DateEqualsIfExists,
    DateGreaterThanIfExists,
    DateGreaterThanEqualsIfExists,
    DateLessThanIfExists,
    DateLessThanEqualsIfExists,
    DateNotEqualsIfExists;
  };

 public DateCondition(DateComparisonType type, String key,
                      List<String> values) {
    if (type == null)
      super.type = null;
    else
      super.type = type.toString();
    super.conditionKey = key;
    super.values = values;
  }

  /**
   * Checks if this Date Condition is satisfied from the request.
   * Sample condition -
   * "Condition":
   *        {"DateGreaterThan": {"aws:CurrentTime": "2020-01-01T00:00:01Z"}}
   */
  @Override public boolean isSatisfied(Map<String, String> requestBody) {

    if (this.conditionKey == null || this.values == null) return false;
    DateComparisonType enumType = DateComparisonType.valueOf(this.type);
    if (enumType == null) return false;
    // 1. Convert condition date string to date object
    conditionDates = new ArrayList<>();
    boolean keyExsists = false;
    boolean isRequestDateHeaderRequired = false;
    Date requestDate = null;
    for (String date : values) {
      Date conditionDate = null;
      if (this.conditionKey.equalsIgnoreCase("CurrentTime")) {
        conditionDate = parseCurrentTime(date);
        keyExsists = true;
        isRequestDateHeaderRequired = true;
        LOGGER.debug("conditionDate in currentTime is :" + conditionDate);

      } else if (this.conditionKey.equalsIgnoreCase("EpochTime")) {
        conditionDate = parseEpochTime(date);
        keyExsists = true;
        isRequestDateHeaderRequired = true;
        LOGGER.debug("conditionDate in EpochTime is :" + conditionDate);

      } else {
        // If any other header provided then parse that date
        String headerVal = null;
        for (Entry<String, String> entry : requestBody.entrySet()) {
          if (this.conditionKey.equalsIgnoreCase(
                  ConditionUtil.removeKeyPrefix(entry.getKey()))) {
            headerVal = entry.getValue();
            requestDate = parseDate(headerVal);
            keyExsists = true;
            break;
          }
        }
        // Assuming the value will be in ISO format similar to CurrentTime
        conditionDate = parseCurrentTime(date);
      }

      if (conditionDate == null) {
        // invalid date format found in policy
        LOGGER.debug("conditionDate object is null");
        return false;
      } else {
        conditionDates.add(conditionDate);
      }
    }

    if (isRequestDateHeaderRequired) {
      // 2. Extract request date from requestBody
      LOGGER.debug("calculating requestDate... ");
      requestDate = populateRequestDate(requestBody);
    }

    boolean result = false;

    switch (enumType) {

      case DateEquals:
        result = dateEquals(requestDate);
        break;

      case DateGreaterThan:
        result = dateGreaterThan(requestDate);
        break;

      case DateLessThan:
        result = dateLessThan(requestDate);
        break;

      case DateNotEquals:
        result = !dateEquals(requestDate);
        break;

      case DateLessThanEquals:
        if (requestDate != null) result = !dateGreaterThan(requestDate);
        break;

      case DateGreaterThanEquals:
        if (requestDate != null) result = !dateLessThan(requestDate);
        break;

      case DateEqualsIfExists:
        if (keyExsists)
          result = dateEquals(requestDate);
        else
          result = true;
        break;

      case DateGreaterThanIfExists:
        if (keyExsists)
          result = dateGreaterThan(requestDate);
        else
          result = true;
        break;

      case DateGreaterThanEqualsIfExists:
        if (keyExsists) {
          if (requestDate != null) {
            result = !dateLessThan(requestDate);
          }
        } else {
          result = true;
        }
        break;

      case DateLessThanIfExists:
        if (keyExsists)
          result = dateLessThan(requestDate);
        else
          result = true;
        break;

      case DateLessThanEqualsIfExists:
        if (keyExsists) {
          if (requestDate != null) {
            result = !dateGreaterThan(requestDate);
          }
        } else {
          result = true;
        }
        break;

      case DateNotEqualsIfExists:
        if (keyExsists)
          result = !dateEquals(requestDate);
        else
          result = true;
        break;

      default:
        result = false;
        break;
    }
    return result;
  }

  /**
   * Parse given condition date string in policy for "CurrentTime" key
   * if its valid return date object
   * if its not return null
   * @param dateString
   * @return Date
   * @return null
   */
 private
  Date parseCurrentTime(String dateString) {
    // CurrentTime needs to be in ISO8601 format. Otherwise return null.

    // Regex for date format - yyyy-MM-dd
    String regexDateYYYY_mm_dd =
        "([12]\\d{3}-(0[1-9]|1[0-2])-(0[1-9]|[12]\\d|3[01]))";

    // Regex for date format - YYYY-MM-DDThh:mmZ 1997-07-10T24:10Z
    String regexDateISO8601 =
        "([12]\\d{3}-(0[1-9]|1[0-2])-(0[1-9]|[12]\\d|3[01]))T((0|1)[0-9]|2[0-" +
        "4]):((0|1|2|3|4|5)[0-9])Z";
    // Regex for date format - YYYY-MM-DDThh:mmZ 1997-07-10T24:10+01:00
    String regexDateISO8601_tzd =
        "([12]\\d{3}-(0[1-9]|1[0-2])-(0[1-9]|[12]\\d|3[01]))T((0|1)[0-9]|2[0-" +
        "4]):((0|1|2|3|4|5)[0-9])[-+]((0|1)[0-9]|2[0-4]):((0|1|2|3|4|5)[0-9" +
        "])";

    // Regex for date format - YYYY-MM-DDThh:mm:ssTZD
    // (1997-07-10T24:10:00+01:00)
    String regexDateISO8601_hhmmsstzd =
        "([12]\\d{3}-(0[1-9]|1[0-2])-(0[1-9]|[12]\\d|3[01]))T((0|1)[0-9]|2[0-" +
        "4]" +
        "):((0|1|2|3|4|5)[0-9]):((0|1|2|3|4|5)[0-9])[-+]((0|1)[0-9]|2[0-4]):(" +
        "(0|1|2|3|4|5)[0-9])";

    // Regex for date format - YYYY-MM-DDThh:mm:ss.sTZD
    // (1997-07-10T24:10:00.45Z)
    String regexDateISO8601_decimal =
        "([12]\\d{3}-(0[1-9]|1[0-2])-(0[1-9]|[12]\\d|3[01]))T((0|1)[0-9]|2[0-" +
        "4]):((0|1|2|3|4|5)[0-9]):((0|1|2|3|4|5)[0-9]).\\d+Z";
    // Regex for date format - YYYY-MM-DDThh:mm:ss.sTZD
    // (1997-07-10T24:10:00.45+01:00)
    String regexDateISO8601_decimal_tzd =
        "([12]\\d{3}-(0[1-9]|1[0-2])-(0[1-9]|[12]\\d|3[01]))T((0|1)[0-9]|2[0-" +
        "4]" +
        "):((0|1|2|3|4|5)[0-9]):((0|1|2|3|4|5)[0-9]).\\d+[-+]((0|1)[0-9]|2[0-" +
        "4]):((0|1|2|3|4|5)[0-9])";

    // TODO Timzone implementation
    /*SimpleDateFormat sdf = null;
    if (Pattern.compile(regexDateISO8601_tzd).matcher(dateString).matches())
        sdf = new SimpleDateFormat("yyyy-MM-dd HH:mmZ");

    else if
    (Pattern.compile(regexDateISO8601_hhmmsstzd).matcher(dateString).matches())
        sdf = new SimpleDateFormat("yyyy-MM-dd HH:mm:ssZ");
    else if
    (Pattern.compile(regexDateISO8601_decimal_tzd).matcher(dateString).matches())
        sdf = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSSZ");

    try {
        Date dateres = sdf.parse(dateString);
        return dateres;
    } catch (ParseException e) {
        // do nothing
    }*/

    if (Pattern.compile(regexDateYYYY_mm_dd).matcher(dateString).matches())
      dateString = dateString.concat("T00:00:00Z");
    else if (Pattern.compile(regexDateISO8601).matcher(dateString).matches())
      dateString = dateString.replace("Z", ":00Z");
    else if (Pattern.compile(regexDateISO8601_decimal)
                 .matcher(dateString)
                 .matches())
      dateString = dateString.replaceAll(".\\d+Z", "Z");
    try {
      return DateUtils.parseISO8601Date(dateString);
    }
    catch (Exception e) {
      return null;
    }
  }

  /**
   * Parse given condition date string in policy for "EpochTIme" key
   * if its valid return date object
   * if its not return null
   * @param dateString
   * @return Date
   * @return null
   */
 private
  Date parseEpochTime(String dateString) {
    // EpochTime needs to be in long value. Otherwise return null.
    try {
      return DateUtils.parseServiceSpecificDate(dateString);
    }
    catch (Exception e) {
      return null;
    }
  }

  /**
   * Parse given request date string from requestBody header i.e.
   * X-Amz-Date/Date
   * if its valid return date object
   * if its not return null
   * @param dateString
   * @return Date
   * @return null
   */
 private
  Date populateRequestDate(Map<String, String> requestBody) {
    String requestDateString;

    if (requestBody.containsKey("X-Amz-Date")) {
      requestDateString = requestBody.get("X-Amz-Date");
    } else {
      requestDateString = requestBody.get("Date");
    }
    return parseDate(requestDateString);
  }

 private
  Date parseDate(String dateString) {
    try {
      // Parse compressed ISO8601 date format into Date object
      // i.e. yyyyMMdd'T'HHmmss'Z' e.g. 20200122T062243Z
      return DateUtils.parseCompressedISO8601Date(dateString);
    }
    catch (Exception e) {
      try {
        // If dateString is not in ISO8601 format then parse it as RFC822 format
        // date either GMT or UTC
        // i.e ""EEE, dd MMM yyyy HH:mm:ss 'GMT'"" e.g. Wed, 22 Jan 2020
        // 07:47:51
        // GMT
        return DateUtil.parseDateString(dateString,
                                        "EEE, dd MMM yyyy HH:mm:ss");
        // TODO use DateUtils from
        // return DateUtils.parseRFC822Date(requestDateString);
      }
      catch (Exception ex) {
        return null;
      }
    }
  }

  /**
     * Check if the request date is equal to any of the condition values.
     * @param requestDate
     * @return true if condition date values contains request date else return
   * false
 */
 private
  boolean dateEquals(Date requestDate) {
    if (requestDate == null) return false;
    for (Date conditionDate : conditionDates) {
      if (requestDate.equals(conditionDate)) {
        return true;
      }
    }
    return false;
  }

  /**
     * Check if the request date is before than any of the condition values.
     * @param requestDate
     * @return true if request date is before condition date values else return
   * false
*/
 private
  boolean dateLessThan(Date requestDate) {
    if (requestDate == null) return false;
    for (Date conditionDate : conditionDates) {
      if (requestDate.before(conditionDate)) {
        return true;
      }
    }
    return false;
  }

  /**
     * Check if the request date is after than any of the condition values.
     * @param requestDate
     * @return true if request Date is after condition date values
*/
 private
  boolean dateGreaterThan(Date requestDate) {
    if (requestDate == null) return false;
    for (Date conditionDate : conditionDates) {
      if (requestDate.after(conditionDate)) {
        return true;
      }
    }
    return false;
  }

  /**
   * Returns the enum value of the String
   * @param enumName
   * @return
   */
 public
  static DateComparisonType getEnum(String enumName) {
    try {
      return DateComparisonType.valueOf(enumName);
    }
    catch (Exception ex) {
      return null;
    }
  }
}
