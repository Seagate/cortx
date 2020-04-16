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
 * Original creation date: 19-Apr-2015
 */
package com.seagates3.perf;

import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.exception.ServerInitialisationException;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileWriter;
import java.io.IOException;
import java.io.UnsupportedEncodingException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class S3Perf {

    private static String FILE_NAME;
    private static File file;
    private static boolean PERF_ENABLED;

    private long startTime;
    private long endTime;

    private static FileWriter fw;
    private static BufferedWriter bw;

    private static final Logger logger
            = LoggerFactory.getLogger(S3Perf.class.getName());

    private static synchronized void writeToFile(String msg) {
        if (bw != null) {
            try {
                bw.write(msg);
                bw.flush();
            } catch (Exception ex) {
                logger.error(ex.getMessage());
            }
        }
    }

    public static void init() throws ServerInitialisationException {
        FILE_NAME = AuthServerConfig.getPerfLogFile();
        PERF_ENABLED = AuthServerConfig.isPerfEnabled();

        if (PERF_ENABLED) {
            try {
                file = new File(FILE_NAME);
                if (!file.exists()) {
                    file.createNewFile();
                }
                fw = new FileWriter(file.getAbsoluteFile(), true);
                bw = new BufferedWriter(fw);
            } catch (FileNotFoundException | UnsupportedEncodingException ex) {
                throw new ServerInitialisationException(ex.getMessage());
            } catch (IOException ex) {
                throw new ServerInitialisationException(ex.getMessage());
            }
        }
    }

    public static synchronized void clean() {
        try {
            if (bw != null) {
                bw.close();
            }
        } catch (IOException ex) {
        }
    }

    public void startClock() {
        startTime = System.currentTimeMillis();
    }

    public void printTime(String msg) {
        if (PERF_ENABLED) {
            String msgToPrint = msg + "," + (endTime - startTime) + " ms\n";
            S3Perf.writeToFile(msgToPrint);
        }
    }

    public void endClock() {
        endTime = System.currentTimeMillis();
    }
}
