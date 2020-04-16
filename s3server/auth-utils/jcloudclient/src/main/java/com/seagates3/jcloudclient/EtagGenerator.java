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
 * Original author:  Sushant Mane <sushant.mane@seagate.com>
 * Original creation date: 22-Jul-2016
 */
package com.seagates3.jcloudclient;

import com.google.common.hash.HashCode;
import com.google.common.hash.Hashing;
import com.google.common.io.ByteSource;
import org.jclouds.io.Payload;
import org.jclouds.io.PayloadSlicer;
import org.jclouds.io.internal.BasePayloadSlicer;
import org.jclouds.io.payloads.FilePayload;

import java.io.ByteArrayOutputStream;
import java.io.File;

public class EtagGenerator {

    private File file;
    /**
     * mpuSizeMB - multipart chunk size in MB
     */
    private long mpuSizeMB;

    public EtagGenerator(File file, long mpuSizeMB) {
        this.file = file;
        this.mpuSizeMB = mpuSizeMB;
    }

    private String generateEtag() {
        FilePayload payload = new FilePayload(file);
        PayloadSlicer slicer = new BasePayloadSlicer();
        long chunkSize = mpuSizeMB * 1024L * 1024L;
        int partNumber = 0;

        ByteArrayOutputStream output = new ByteArrayOutputStream();
        for (Payload part: slicer.slice(payload, chunkSize)) {
            ByteSource source = (ByteSource) part.getRawContent();
            try {
                HashCode md5 = source.hash(Hashing.md5());
                output.write(md5.asBytes());
                partNumber++;
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
        byte[] bytes = output.toByteArray();
        String eTag = Hashing.md5().hashBytes(bytes) + "-" + partNumber;

        return eTag;
    }

    public String getEtag() {
        return generateEtag();
    }
}
