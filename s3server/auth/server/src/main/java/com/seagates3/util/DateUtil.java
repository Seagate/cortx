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
 * Original creation date: 17-Sep-2014
 */
package com.seagates3.util;

import java.text.DateFormat;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.TimeZone;

import org.joda.time.DateTime;
import org.joda.time.DateTimeZone;
import org.joda.time.format.DateTimeFormat;
import org.joda.time.format.DateTimeFormatter;

/*
 * TODO
 * replace java.util.date with org.joda.time.DateTime.
 */
public class DateUtil {

    private static final String LDAP_DATE_FORMAT = "yyyyMMddHHmmss";
    private static final String SERVER_RESPONSE_DATE_FORMAT
            = "yyyy-MM-dd'T'HH:mm:ss.SSSZ";

    public static String toLdapDate(Date date) {
        SimpleDateFormat sdf = getSimpleDateFormat(LDAP_DATE_FORMAT);
        return (sdf.format(date) + "Z");
    }

    public static String toLdapDate(String date) {
        return toLdapDate(toDate(date));
    }

    public static String toServerResponseFormat(Date date) {
        SimpleDateFormat serverResponseFormat
                = getSimpleDateFormat(SERVER_RESPONSE_DATE_FORMAT);

        return serverResponseFormat.format(date);
    }

    public static String toServerResponseFormat(DateTime date) {
        DateTimeFormatter fmt = DateTimeFormat.forPattern(
                SERVER_RESPONSE_DATE_FORMAT);
        return fmt.print(date);
    }

    public static String toServerResponseFormat(String ldapDate) {
        return toServerResponseFormat(toDate(ldapDate));
    }

    public static DateTime getCurrentDateTime() {
        return DateTime.now(DateTimeZone.UTC);
    }

     /*
     * <IEM_INLINE_DOCUMENTATION>
     *     <event_code>048003001</event_code>
     *     <application>S3 Authserver</application>
     *     <submodule>Parser</submodule>
     *     <description>Unparseable date</description>
     *     <audience>Development</audience>
     *     <details>
     *         Unparseable date.
     *         The data section of the event has following keys:
     *           time - timestamp
     *           node - node name
     *           pid - process id of Authserver
     *           file - source code filename
     *           line - line number within file where error occurred
     *     </details>
     *     <service_actions>
     *         Save authserver log files and contact development team for
     *         further investigation.
     *     </service_actions>
     * </IEM_INLINE_DOCUMENTATION>
     *
     */
    /**
     * Return the current time in UTC.
     */
    public static long getCurrentTime() {
        SimpleDateFormat dateFormatGmt = getSimpleDateFormat("yyyy-MMM-dd HH:mm:ss");
        dateFormatGmt.setTimeZone(TimeZone.getTimeZone("UTC"));

        try {
            return dateFormatGmt.parse(dateFormatGmt.format(new Date())).getTime();
        } catch (ParseException ex) {
            IEMUtil.log(IEMUtil.Level.ERROR, IEMUtil.UNPARSABLE_DATE,
                    "Unparseable date", null);
        }

        return 0;
    }

    public static DateTime toDateTime(String date) {
        DateTimeFormatter dateFormatter = getDateTimeFormat(LDAP_DATE_FORMAT);

        try {
            return dateFormatter.parseDateTime(date);
        } catch (Exception ex) {
        }

        dateFormatter = getDateTimeFormat(SERVER_RESPONSE_DATE_FORMAT);
        return dateFormatter.parseDateTime(date);

    }

    public static Date toDate(String date) {
        SimpleDateFormat sdf = getSimpleDateFormat(LDAP_DATE_FORMAT);

        try {
            return sdf.parse(date);
        } catch (ParseException e) {
        }

        sdf = getSimpleDateFormat(SERVER_RESPONSE_DATE_FORMAT);
        try {
            return sdf.parse(date);
        } catch (ParseException e) {
            IEMUtil.log(IEMUtil.Level.ERROR, IEMUtil.UNPARSABLE_DATE,
                    "Unparseable date", null);
        }

        return null;
    }

    private static DateTimeFormatter getDateTimeFormat(String pattern) {
        DateTimeFormatter dtf = DateTimeFormat.forPattern(pattern);
        return dtf.withZoneUTC();
    }

    private static SimpleDateFormat getSimpleDateFormat(String pattern) {
        SimpleDateFormat sdf = new SimpleDateFormat(pattern);
        sdf.setTimeZone(getDefatulTimeZone());

        return sdf;
    }

    /**
     * Return the default time zone (UTC).
     *
     * @return UTC Time zone.
     */
    private static TimeZone getDefatulTimeZone() {
        return TimeZone.getTimeZone("UTC");
    }
    /**
     * Return the current time in GMT.
     * Returns Date in format "Thu, 05 Jul 2018 03:55:43 GMT"
     */
    public static String getCurrentTimeGMT() {
        DateFormat df = new SimpleDateFormat("EEE, dd MMM yyyy HH:mm:ss ");
        df.setTimeZone(TimeZone.getTimeZone("GMT"));
        String d = df.format(new Date()) + "GMT";
        return d;
    }

    /**
     * Returns Date in given pattern in UTC format.
     */
   public
    static Date parseDateString(String dateString, String pattern) {

      Date date = null;
      SimpleDateFormat sdf = new SimpleDateFormat(pattern);
      sdf.setTimeZone(TimeZone.getTimeZone("UTC"));
      if (dateString == null || dateString.trim().isEmpty()) {
        return null;
      }
      try {
        date = sdf.parse(dateString);
        return date;
      }
      catch (ParseException e) {
      }
      return null;
    }
}
