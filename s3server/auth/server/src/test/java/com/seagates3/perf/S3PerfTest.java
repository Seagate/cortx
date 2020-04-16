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
 * Original creation date: 10-Feb-2017
 */

package com.seagates3.perf;

import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.exception.ServerInitialisationException;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.powermock.api.mockito.mockpolicies.Slf4jMockPolicy;
import org.powermock.core.classloader.annotations.MockPolicy;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileWriter;
import java.io.IOException;

import static org.mockito.Matchers.anyString;
import static org.mockito.Mockito.verify;
import static org.powermock.api.mockito.PowerMockito.mock;
import static org.powermock.api.mockito.PowerMockito.mockStatic;
import static org.powermock.api.mockito.PowerMockito.when;
import static org.powermock.api.mockito.PowerMockito.whenNew;

@RunWith(PowerMockRunner.class)
@PrepareForTest({AuthServerConfig.class, File.class, S3Perf.class})
@MockPolicy(Slf4jMockPolicy.class)
public class S3PerfTest {

    private File file;
    private FileWriter fileWriter;
    private BufferedWriter bufferedWriter;
    private S3Perf s3Perf;

    @Test
    public void initTest() throws Exception {
        initTestHelper();
        whenNew(FileWriter.class).withArguments(file, true).thenReturn(fileWriter);
        whenNew(BufferedWriter.class).withArguments(fileWriter).thenReturn(bufferedWriter);

        S3Perf.init();

        verify(file).exists();
    }

    @Test(expected = ServerInitialisationException.class)
    public void initTest_FileNotFoundException() throws Exception {
        initTestHelper();
        whenNew(FileWriter.class).withArguments(file, true).thenThrow(FileNotFoundException.class);

        S3Perf.init();
    }

    @Test(expected = ServerInitialisationException.class)
    public void initTest_IOException() throws Exception {
        initTestHelper();
        whenNew(FileWriter.class).withArguments(file, true).thenThrow(IOException.class);

        S3Perf.init();
    }

    private void initTestHelper() throws Exception {
        mockStatic(AuthServerConfig.class);

        file = mock(File.class);
        fileWriter = mock(FileWriter.class);
        bufferedWriter = mock(BufferedWriter.class);

        when(AuthServerConfig.getPerfLogFile()).thenReturn("/var/log/seagate/auth/perf.log");
        when(AuthServerConfig.isPerfEnabled()).thenReturn(true);
        whenNew(File.class).withArguments("/var/log/seagate/auth/perf.log").thenReturn(file);
        when(file.getAbsoluteFile()).thenReturn(file);
    }

    @Test
    public void cleanTest() throws Exception {
        initTestHelper();
        whenNew(FileWriter.class).withArguments(file, true).thenReturn(fileWriter);
        whenNew(BufferedWriter.class).withArguments(fileWriter).thenReturn(bufferedWriter);
        S3Perf.init();

        S3Perf.clean();

        verify(bufferedWriter).close();
    }

    @Test
    public void printTimeTest() throws Exception {
        initTestHelper();
        whenNew(FileWriter.class).withArguments(file, true).thenReturn(fileWriter);
        whenNew(BufferedWriter.class).withArguments(fileWriter).thenReturn(bufferedWriter);
        S3Perf.init();

        s3Perf = new S3Perf();
        s3Perf.startClock();
        s3Perf.endClock();
        s3Perf.printTime("SampleMessage:");

        verify(bufferedWriter).write(anyString());
        verify(bufferedWriter).flush();
    }
}