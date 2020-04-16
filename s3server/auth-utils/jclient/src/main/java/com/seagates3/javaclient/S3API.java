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
 * Original creation date: 12-Feb-2016
 */
package com.seagates3.javaclient;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import org.apache.commons.cli.CommandLine;
import com.amazonaws.AmazonClientException;
import com.amazonaws.AmazonServiceException;
import com.amazonaws.ClientConfiguration;
import com.amazonaws.auth.AWSCredentials;
import com.amazonaws.services.s3.AmazonS3Client;
import com.amazonaws.services.s3.S3ClientOptions;
import com.amazonaws.services.s3.model.AbortMultipartUploadRequest;
import com.amazonaws.services.s3.model.AccessControlList;
import com.amazonaws.services.s3.model.AmazonS3Exception;
import com.amazonaws.services.s3.model.Bucket;
import com.amazonaws.services.s3.model.BucketPolicy;
import com.amazonaws.services.s3.model.CanonicalGrantee;
import com.amazonaws.services.s3.model.CompleteMultipartUploadRequest;
import com.amazonaws.services.s3.model.DeleteObjectsRequest;
import com.amazonaws.services.s3.model.Grant;
import com.amazonaws.services.s3.model.Grantee;
import com.amazonaws.services.s3.model.GetObjectMetadataRequest;
import com.amazonaws.services.s3.model.GetObjectRequest;
import com.amazonaws.services.s3.model.GroupGrantee;
import com.amazonaws.services.s3.model.InitiateMultipartUploadRequest;
import com.amazonaws.services.s3.model.InitiateMultipartUploadResult;
import com.amazonaws.services.s3.model.ListMultipartUploadsRequest;
import com.amazonaws.services.s3.model.ListObjectsRequest;
import com.amazonaws.services.s3.model.ListPartsRequest;
import com.amazonaws.services.s3.model.MultipartUpload;
import com.amazonaws.services.s3.model.MultipartUploadListing;
import com.amazonaws.services.s3.model.ObjectListing;
import com.amazonaws.services.s3.model.ObjectMetadata;
import com.amazonaws.services.s3.model.PartETag;
import com.amazonaws.services.s3.model.PartListing;
import com.amazonaws.services.s3.model.PartSummary;
import com.amazonaws.services.s3.model.Permission;
import com.amazonaws.services.s3.model.PutObjectRequest;
import com.amazonaws.services.s3.model.S3Object;
import com.amazonaws.services.s3.model.S3ObjectSummary;
import com.amazonaws.services.s3.model.UploadPartRequest;

import static com.seagates3.javaclient.ClientConfig.getClientConfiguration;

public class S3API {

    private final AmazonS3Client client;
    private final CommandLine cmd;
    private String bucketName;
    private String keyName;

    public S3API(CommandLine cmd) {
        this.cmd = cmd;
        verifyCreds();

        ClientConfiguration config = getClientConfiguration();
        if (cmd.hasOption("cli-exec-timeout")) {
            config.setClientExecutionTimeout(Integer.parseInt(cmd.getOptionValue("cli-exec-timeout")));
        }
        if (cmd.hasOption("req-timeout")) {
            config.setRequestTimeout(Integer.parseInt(cmd.getOptionValue("req-timeout")));
        }
        if (cmd.hasOption("sock-timeout")) {
            config.setSocketTimeout(Integer.parseInt(cmd.getOptionValue("sock-timeout")));
        }


        AWSCredentials creds;

        if (cmd.hasOption("t")) {
            creds = ClientConfig.getCreds(cmd.getOptionValue("x"),
                    cmd.getOptionValue("y"), cmd.getOptionValue("t"));
        } else {
            creds = ClientConfig.getCreds(cmd.getOptionValue("x"),
                    cmd.getOptionValue("y"));
        }

        client = new AmazonS3Client(creds, config);

        if (!cmd.hasOption("a")) {
            String endPoint;
            try {
                endPoint = ClientConfig.getEndPoint(client.getClass());
                /*
                 * This tmp fix is to create bucket in location provided by user.
                 * This is temporary solution and should be replaced with proper fix
                 */
                if (cmd.hasOption('l')) {
                    Pattern pattern = Pattern.compile("(\\w*://s3)(\\.\\w*\\.\\w+)");
                    Matcher matcher = pattern.matcher(endPoint);
                    if (matcher.find()) {
                        endPoint = matcher.group(1) + "-" + cmd.getOptionValue('l') + matcher.group(2);
                    }
                }
                client.setEndpoint(endPoint);
            } catch (IOException ex) {
                printError(ex.toString());
            }
        }

        S3ClientOptions.Builder clientOptions = S3ClientOptions.builder();

        if (cmd.hasOption("p"))
            clientOptions.setPathStyleAccess(true);
        if (!cmd.hasOption("C"))
            clientOptions.disableChunkedEncoding();

        clientOptions.setPayloadSigningEnabled(true);
        client.setS3ClientOptions(clientOptions.build());
    }

    public void makeBucket() {
        checkCommandLength(2);
        getBucketObjectName(cmd.getArgs()[1]);
        if (bucketName.isEmpty()) {
            printError("Incorrect command. Bucket name is required.");
        }

        try {
            if (cmd.hasOption("l")) {
                client.createBucket(bucketName, cmd.getOptionValue("l"));
            } else {
                client.createBucket(bucketName);
            }
            System.out.println("Bucket created successfully.");
        } catch (AmazonClientException e) {
            printError(e.toString());
        }
    }

    public void removeBucket() {
        checkCommandLength(2);
        getBucketObjectName(cmd.getArgs()[1]);
        if (bucketName.isEmpty()) {
            printError("Incorrect command. Bucket name is required.");
        }

        try {
            client.deleteBucket(bucketName);
            System.out.println("Bucket deleted successfully.");
        } catch (AmazonClientException e) {
            printError(e.toString());
        }
    }

    public void list() {
        String s3Url;
        if (cmd.getArgs().length == 2) {
            s3Url = cmd.getArgs()[1];
        } else {
            s3Url = "s3://";
        }

        getBucketObjectName(s3Url);

        if (bucketName.isEmpty()) {
            try {
                List<Bucket> buckets = client.listBuckets();
                System.out.println("Buckets - ");
                for (Bucket bucket : buckets) {
                    System.out.println(bucket.getName());
                }
            } catch (AmazonClientException e) {
                printError(e.toString());
            }

            return;
        }

        try {

            ListObjectsRequest listObjectsRequest;
            if (keyName.isEmpty()) {
                listObjectsRequest = new ListObjectsRequest()
                        .withBucketName(bucketName);
            } else {
                listObjectsRequest = new ListObjectsRequest()
                        .withBucketName(bucketName)
                        .withPrefix(keyName);
            }

            ObjectListing listObjects;
            do {
                listObjects = client.listObjects(listObjectsRequest);
                for (S3ObjectSummary objectSummary
                        : listObjects.getObjectSummaries()) {
                    System.out.println(" - " + objectSummary.getKey() + "  "
                            + "(size = " + objectSummary.getSize()
                            + ")");
                }
                listObjectsRequest.setMarker(listObjects.getNextMarker());
            } while (listObjects.isTruncated());

        } catch (AmazonClientException e) {
            printError(e.toString());
        }
    }

    public void getObject() {
        checkCommandLength(3);
        if (cmd.getArgs()[2].isEmpty()) {
            printError("Give output file.");
        }

        getBucketObjectName(cmd.getArgs()[1]);

        if (bucketName.isEmpty() || keyName.isEmpty()) {
            printError("Incorrect command. Bucket name and key is required.");
        }

        try {
            S3Object object = client.getObject(
                    new GetObjectRequest(bucketName, keyName));
            InputStream inputStream = object.getObjectContent();
            File file = new File(cmd.getArgs()[2]);

            OutputStream outputStream = new FileOutputStream(file);

            int read;
            byte[] bytes = new byte[1024];

            while ((read = inputStream.read(bytes)) != -1) {
                outputStream.write(bytes, 0, read);
            }

            System.out.println("Object download successfully.");
        } catch (AmazonClientException e) {
            printError(e.toString());
        } catch (FileNotFoundException ex) {
            printError("Incorrect target file.\n" + ex.toString());
        } catch (IOException ex) {
            printError("Error occured while copying s3ObjectStream to target "
                    + "file.\n" + ex.toString());
        }
    }

    public void putObject() {
        checkCommandLength(3);
        String fileName = cmd.getArgs()[1];
        File file = new File(fileName);

        if (!file.exists()) {
            System.err.println("Given file doesn't exist.");
            System.exit(1);
        }

        getBucketObjectName(cmd.getArgs()[2]);

        if (bucketName.isEmpty()) {
            printError("Incorrect command. Bucket name is required.");
            printError("Incorrect command. Check java client usage.");
        }

        if (keyName.isEmpty()) {
            keyName = file.getName();
        } else if (keyName.endsWith("/")) {
            keyName += file.getName();
        }

        if (cmd.hasOption("m")) {
            multiPartUpload(file);
            return;
        }

        try {
            client.putObject(
                    new PutObjectRequest(bucketName, keyName, file));
            System.out.println("Object put successfully.");
        } catch (AmazonClientException e) {
            printError(e.toString());
        }
    }

    public void createMultipartUpload() {
        checkCommandLength(3);

        String fileName = cmd.getArgs()[1];
        File file = new File(fileName);

        if (!file.exists()) {
            printError("Given file doesn't exist.");
        }

        getBucketObjectName(cmd.getArgs()[2]);
        if (bucketName.isEmpty()) {
            printError("Incorrect command. Bucket name is required.");
            printError("Incorrect command. Check java client usage.");
        }

        if (keyName.isEmpty()) {
            keyName = file.getName();
        } else if (keyName.endsWith("/")) {
            keyName += file.getName();
        }

        initiateMPU(bucketName, keyName);
    }

    private InitiateMultipartUploadResult initiateMPU(String bucketName,
                                                      String keyName) {
        InitiateMultipartUploadRequest initRequest
                = new InitiateMultipartUploadRequest(bucketName, keyName);
        InitiateMultipartUploadResult initResponse = null;
        try {
            initResponse = client.initiateMultipartUpload(initRequest);
            System.out.println("Upload id - " + initResponse.getUploadId());
        } catch (AmazonClientException ex) {
            printError(ex.toString());
        }

        return initResponse;
    }

    public void partialPutObject() {
        checkCommandLength(4);

        String fileName = cmd.getArgs()[1];
        File file = new File(fileName);

        if (!file.exists()) {
            System.err.println("Given file doesn't exist.");
            System.exit(1);
        }

        getBucketObjectName(cmd.getArgs()[2]);

        if (bucketName.isEmpty()) {
            printError("Incorrect command. Bucket name is required.");
            printError("Incorrect command. Check java client usage.");
        }

        if (keyName.isEmpty()) {
            keyName = file.getName();
        } else if (keyName.endsWith("/")) {
            keyName += file.getName();
        }

        int noOfParts = Integer.parseInt(cmd.getArgs()[3]);
        long contentLength = file.length();
        long partSize = 16 * 1024 * 1024;
        if (cmd.hasOption("m")) {
            partSize = Integer.parseInt(cmd.getOptionValue("m")) * 1024 * 1024;
        }
        String uploadId = getOptionValue("with-upload-id");
        if (uploadId == null) {
            uploadId = initiateMPU(bucketName, keyName).getUploadId();
        }
        try {
            long filePosition = 0;
            int i = 1;
            if (cmd.hasOption("from-part")) {
                int fromPart = Integer.parseInt(getOptionValue("from-part"));
                filePosition = (fromPart - 1) * partSize;
                i = fromPart;
            }
            while (noOfParts > 0 && filePosition < contentLength) {
                partSize = Math.min(partSize, (contentLength - filePosition));
                UploadPartRequest uploadRequest = new UploadPartRequest()
                        .withBucketName(bucketName).withKey(keyName)
                        .withUploadId(uploadId)
                        .withPartNumber(i)
                        .withFileOffset(filePosition)
                        .withFile(file)
                        .withPartSize(partSize);
                System.out.println("Uploading part " + i + " of size "
                        + partSize / (1024 * 1024) + "MB");
                client.uploadPart(uploadRequest);
                filePosition += partSize;
                i++;
                noOfParts--;
            }
            System.out.println("Partial upload successful.");
        } catch (AmazonClientException e) {
            printError("Partial upload failed." + e.getMessage());
        }
    }

    public void deleteObject() {
        checkCommandLength(2);
        getBucketObjectName(cmd.getArgs()[1]);

        if (bucketName.isEmpty() || keyName.isEmpty()) {
            printError("Incorrect command. Check java client usage.");
            printError("Incorrect command. Bucket name and key are required.");
        }

        try {
            client.deleteObject(bucketName, keyName);
            System.out.println("Object deleted successfully.");
        } catch (AmazonClientException e) {
            printError(e.toString());
        }
    }

    public void deleteMultipleObjects() {
        if (cmd.getArgList().size() < 2) {
            printError("Incorrect command");
        }

        getBucketObjectName(cmd.getArgs()[1]);

        if (bucketName.isEmpty()) {
            printError("Incorrect command. Bucket name is required.");
        }
        String[] objects = cmd.getArgs();
        List<DeleteObjectsRequest.KeyVersion> objectList = new LinkedList<>();

        for (int index = 2; index < objects.length; index++) {
            objectList.add(new DeleteObjectsRequest.KeyVersion(objects[index]));
        }

        try {
            DeleteObjectsRequest request = new DeleteObjectsRequest(bucketName);
            request.withKeys(objectList);

            client.deleteObjects(request);
            System.out.println("Objects deleted successfully.");
        } catch (Exception e) {
            printError(e.toString());
        }
    }

    public void headObject() {
        checkCommandLength(2);
        getBucketObjectName(cmd.getArgs()[1]);

        if (bucketName.isEmpty() || keyName.isEmpty()) {
            printError("Provider bucket and object name.");
        }

        try {

            ObjectMetadata objectMetadata = client.getObjectMetadata(
                    new GetObjectMetadataRequest(bucketName, keyName));
            if (objectMetadata == null) {
                printError("Bucket or Object does not exist.");
            } else {
                String metadata = "Bucket name- " + bucketName;
                metadata += "\nObject name - " + keyName;
                metadata += "\nObject size - " + objectMetadata.getContentLength();
                metadata += "\nEtag - " + objectMetadata.getETag();
                metadata += "\nLastModified - " + objectMetadata.getLastModified();

                System.out.println(metadata);
            }
        } catch(AmazonS3Exception awsS3Exception) {
            if (awsS3Exception.getStatusCode() == 404)
                printError("Bucket or Object does not exist.");
            else
                printError(awsS3Exception.toString());
        } catch (Exception e) {
            printError(e.toString());
        }
    }

    private void printMultipartUploads(MultipartUploadListing uploads) {
        for (MultipartUpload upload : uploads.getMultipartUploads()) {
            System.out.println("Name - " + upload.getKey() + ", Upload id - "
                    + upload.getUploadId());
        }
    }

    private void printCommonPrefixes(MultipartUploadListing uploads) {
        for (String commonPrefix : uploads.getCommonPrefixes()) {
            System.out.println("CommonPrefix - " + commonPrefix);
        }
    }

    private String getOptionValue(String option) {
        String value = null;
        if (cmd.hasOption(option)) {
            value = cmd.getOptionValue(option);
        }

        return value;
    }

    private ListMultipartUploadsRequest createListMpuRequest(String bucketName,
                                                             Integer maxUploads,
                                                             String nextMarker,
                                                             String uploadIdMarker,
                                                             String prefix,
                                                             String delimiter) {
        ListMultipartUploadsRequest request = new ListMultipartUploadsRequest(bucketName);

        if (maxUploads > 0) {
            request.setMaxUploads(maxUploads);
        }
        if (nextMarker != null || uploadIdMarker != null) {
            if (nextMarker != null && uploadIdMarker != null) {
                request.setKeyMarker(nextMarker);
                request.setUploadIdMarker(uploadIdMarker);
            } else {
                printError("Both next-marker and upload-id-marker are required");
            }
        }
        if (prefix != null) {
            request.setPrefix(prefix);
        }
        if (delimiter != null) {
            request.setDelimiter(delimiter);
        }

        return request;
    }

    public void listMultiParts() {
        checkCommandLength(2);
        getBucketObjectName(cmd.getArgs()[1]);

        if (bucketName.isEmpty()) {
            printError("Incorrect command. Bucket name is required.");
        }

        boolean showNext = false;
        int maxUploads = 0; // S3 BUG WHEN MAX-UPLOADS = 1
        String nextMarker = getOptionValue("next-marker");
        String uploadIdMarker = getOptionValue("upload-id-marker");
        String prefix = getOptionValue("prefix");
        String delimiter = getOptionValue("delimiter");

        if (cmd.hasOption("show-next")) {
            showNext = true;
        }
        if (cmd.hasOption("max-uploads")) {
            maxUploads = Integer.parseInt(getOptionValue("max-uploads"));
        }

        try {
            MultipartUploadListing uploads;
            ListMultipartUploadsRequest request;
            System.out.println("Multipart uploads -");
            do {
                request = createListMpuRequest(bucketName, maxUploads,
                        nextMarker, uploadIdMarker, prefix, delimiter);
                uploads = client.listMultipartUploads(request);
                printMultipartUploads(uploads);
                if (!uploads.getCommonPrefixes().isEmpty()) {
                    printCommonPrefixes(uploads);
                }
                nextMarker = uploads.getNextKeyMarker();
                uploadIdMarker = uploads.getNextUploadIdMarker();
            } while (showNext && uploads.isTruncated());
        } catch (AmazonClientException ex) {
            printError(ex.toString());
        }
    }

    public void listParts() {
        checkCommandLength(3);
        getBucketObjectName(cmd.getArgs()[1]);

        if (bucketName.isEmpty() || keyName.isEmpty()) {
            printError("Incorrect command. Bucket name and key are required.");
        }

        String uploadId = cmd.getArgs()[2];
        if (uploadId.isEmpty()) {
            printError("Provide upload id.");
        }

        try {
            PartListing listParts = client.listParts(
                    new ListPartsRequest(bucketName, keyName, uploadId)
            );

            System.out.println("Object - " + keyName);
            for (PartSummary part : listParts.getParts()) {
                System.out.println("part number - " + part.getPartNumber() + ", "
                        + " Size - " + part.getSize()
                        + " Etag - " + part.getETag());
            }
        } catch (AmazonClientException ex) {
            printError(ex.toString());
        }
    }

    public void abortMultiPart() {
        checkCommandLength(3);
        getBucketObjectName(cmd.getArgs()[1]);

        if (bucketName.isEmpty() || keyName.isEmpty()) {
            printError("Incorrect command. Bucket name and key are required.");
        }

        String uploadId = cmd.getArgs()[2];
        if (uploadId.isEmpty()) {
            printError("Provide upload id.");
        }

        try {
            client.abortMultipartUpload(new AbortMultipartUploadRequest(
                    bucketName, keyName, uploadId));

            System.out.println("Upload aborted successfully.");
        } catch (AmazonClientException ex) {
            printError(ex.toString());
        }
    }

    public void exists() {
        checkCommandLength(2);
        getBucketObjectName(cmd.getArgs()[1]);

        if (bucketName.isEmpty()) {
            printError("Bucket name cannot be empty");
        }

        if (keyName.isEmpty()) {
            try {
                if (client.doesBucketExist(bucketName)) {
                    System.out.println("Bucket " + bucketName + " exists.");
                } else {
                    System.out.println("Bucket " + bucketName
                            + " does not exist.");
                }

            } catch (AmazonClientException ex) {
                printError(ex.toString());
            }
        } else {
            try {
                /**
                 * This is a work around. The latest AWS SDK has doesObjectExist
                 * API which has to be used here.
                 */
                ObjectMetadata objectMetadata = client.getObjectMetadata(
                        new GetObjectMetadataRequest(bucketName, keyName));
                if (objectMetadata == null) {
                    System.out.println("Object does not exist.");
                } else {
                    System.out.println("Object exists.");
                }
            } catch (AmazonClientException ex) {
                System.out.println("Object does not exist.");
            }
        }
    }

    public void getAcl() {
        checkCommandLength(2);
        getBucketObjectName(cmd.getArgs()[1]);

        if (bucketName.isEmpty()) {
            printError("Bucket name cannot be empty");
        }

        if (keyName.isEmpty()) {
            try {
                AccessControlList bucketAcl = client.getBucketAcl(bucketName);
                System.out.println("Owner: " + bucketAcl.getOwner().getDisplayName());
                printGrants(bucketAcl.getGrantsAsList());
            } catch (AmazonServiceException awsServiceException) {
                printAwsServiceException(awsServiceException);
            } catch (AmazonClientException awsClientException) {
                printError(awsClientException.toString());
            }
        } else {
            try {
                AccessControlList objectAcl = client.getObjectAcl(bucketName, keyName);
                printGrants(objectAcl.getGrantsAsList());
            } catch (AmazonServiceException awsServiceException) {
                printAwsServiceException(awsServiceException);
            } catch (AmazonClientException awsClientException) {
                printError(awsClientException.toString());
            }
        }
    }

    public void setAcl() {
        checkCommandLength(3);
        getBucketObjectName(cmd.getArgs()[1]);

        if (bucketName.isEmpty()) {
            printError("Incorrect command. Bucket name is required.");
        }

        Map<String, String> requestMap = parseGrantRequesst(cmd.getArgs()[2]);
        Permission permission = validateAndGetPermission(requestMap.get("permission"));
        String action = requestMap.get("action");

        if (action.equals("grant")) {
            grantAcl(requestMap.get("canonicalID"), requestMap.get("displayName"), permission);
        } else if (action.equals("revoke")) {
            revokeAcl(requestMap.get("canonicalID"), permission);
        } else if (action.equals("acl-private")) {
            setAclPrivate();
        } else if (action.equals("acl-public")) {
            setAclPublic();
        } else {
            printError("Unknown ACL action");
        }
    }

    private Map<String, String> parseGrantRequesst(String request) {
        Map<String, String> requestMap = new HashMap<>();

        if (!request.contains("=")) {
            requestMap.put("action", request);
            return requestMap;
        }

        Pattern pattern = Pattern.compile("acl-(\\w+)=(\\w+):([^:]+):?(.+)?");
        Matcher matcher = pattern.matcher(request);
        if (matcher.find() && matcher.groupCount() > 3) {
            requestMap.put("action", matcher.group(1));
            requestMap.put("permission", matcher.group(2));
            requestMap.put("canonicalID", matcher.group(3));
            if (matcher.group(4) != null) {
                requestMap.put("displayName", matcher.group(4));
            }
        } else {
            printError("Incorrect options");
        }

        return requestMap;
    }

    private Permission validateAndGetPermission(String permission) {
        if (permission == null) {
            return null;
        }

        Permission parsedPermission = Permission.parsePermission(permission);
        if (parsedPermission == null) {
            printError("Unknown permission. Incorrect command.");
        }

        return parsedPermission;
    }

    private void grantAcl(String canonicalID, String displayName, Permission permission) {
        try {
            if (canonicalID == null || canonicalID.isEmpty()) {
                printError("Invalid canonical id.");
            }

            CanonicalGrantee canonicalGrantee = new CanonicalGrantee(canonicalID);
            if (displayName != null) {
                canonicalGrantee.setDisplayName(displayName);
            }

            if (keyName.isEmpty()) {
                AccessControlList acl = client.getBucketAcl(bucketName);
                acl.grantPermission(canonicalGrantee, permission);
                client.setBucketAcl(bucketName, acl);
            } else {
                AccessControlList acl = client.getObjectAcl(bucketName, keyName);
                acl.grantPermission(canonicalGrantee, permission);
                client.setObjectAcl(bucketName, keyName, acl);
            }
            System.out.println("Grant ACL successful");
        } catch (Exception e) {
            printError(e.getMessage());
        }
    }

    private void revokeAcl(String canonicalID, Permission permission) {
        try {
            if (canonicalID == null || canonicalID.isEmpty()) {
                printError("Invalid canonical id.");
            }

            CanonicalGrantee canonicalGrantee = new CanonicalGrantee(canonicalID);
            if (keyName.isEmpty()) {
                AccessControlList acl = client.getBucketAcl(bucketName);
                acl.revokeAllPermissions(canonicalGrantee);
                client.setBucketAcl(bucketName, acl);
            } else {
                AccessControlList acl = client.getObjectAcl(bucketName, keyName);
                acl.revokeAllPermissions(canonicalGrantee);
                client.setObjectAcl(bucketName, keyName, acl);
            }
            System.out.println("Revoke ACL successful");
        } catch (Exception e) {
            printError(e.getMessage());
        }
    }

    private void setAclPrivate() {
        try {
            if (keyName.isEmpty()) {
                AccessControlList acl = client.getBucketAcl(bucketName);
                if (aclHasAnnonRead(acl)) {
                    acl.revokeAllPermissions(GroupGrantee.AllUsers);
                    client.setBucketAcl(bucketName, acl);
                    System.out.println("ACL set to Private.");
                } else {
                    System.out.println("Already private. Skipping.");
                }
            } else {
                AccessControlList acl = client.getObjectAcl(bucketName, keyName);
                if (aclHasAnnonRead(acl)) {
                    acl.revokeAllPermissions(GroupGrantee.AllUsers);
                    client.setObjectAcl(bucketName, keyName, acl);
                    System.out.println("ACL set to Private.");
                } else {
                    System.out.println("Already private. Skipping.");
                }
            }
        } catch (Exception e) {
            printError(e.getMessage());
        }
    }

    private void setAclPublic() {
        try {
            if (keyName.isEmpty()) {
                AccessControlList acl = client.getBucketAcl(bucketName);
                if (!aclHasAnnonRead(acl)) {
                    acl.grantPermission(GroupGrantee.AllUsers, Permission.Read);
                    client.setBucketAcl(bucketName, acl);
                    System.out.println("ACL set to Public.");
                } else {
                    System.out.println("Already public. Skipping.");
                }
            } else {
                AccessControlList acl = client.getObjectAcl(bucketName, keyName);
                if (!aclHasAnnonRead(acl)) {
                    acl.grantPermission(GroupGrantee.AllUsers, Permission.Read);
                    client.setObjectAcl(bucketName, keyName, acl);
                    System.out.println("ACL set to Public.");
                } else {
                    System.out.println("Already public. Skipping.");
                }
            }
        } catch (Exception e) {
            printError(e.getMessage());
        }
    }

    private boolean aclHasAnnonRead(AccessControlList acl) {
        for (Grant grant: acl.getGrantsAsList()) {
            if (isAnnonRead(grant)) {
                return true;
            }
        }

        return false;
    }

    private boolean isAnnonRead(Grant grant) {
        Permission permission = grant.getPermission();

        if (isAllUsers(grant.getGrantee()) && (permission.equals(Permission.FullControl) ||
                permission.equals(Permission.Read))) {
            return true;
        }

        return false;
    }

    private boolean isAllUsers(Grantee grantee) {
        if (grantee.equals(GroupGrantee.AllUsers)) {
            return true;
        }

        return false;
    }

    public void getBucketLocation() {
        checkCommandLength(2);
        getBucketObjectName(cmd.getArgs()[1]);

        if (bucketName.isEmpty()) {
            printError("Bucket name cannot be empty");
        }

        try {
            System.out.println(client.getBucketLocation(bucketName));
        } catch (AmazonServiceException awsServiceException) {
            printAwsServiceException(awsServiceException);
        } catch (AmazonClientException awsClientException) {
            printError(awsClientException.toString());
        }
    }

    public void putBucketPolicy() {
        checkCommandLength(3);
        getBucketObjectName(cmd.getArgs()[1]);

        if (bucketName.isEmpty()) {
            printError("Bucket name cannot be empty");
        }

        String policyText = getPolicyText(cmd.getArgs()[2]);
        try {
            client.setBucketPolicy(bucketName, policyText);
            System.out.println("Bucket policy set successfully");
        } catch (AmazonServiceException awsServiceException) {
            printAwsServiceException(awsServiceException);
        } catch (AmazonClientException awsClientException) {
            printError(awsClientException.toString());
        }
    }

    public void getBucketPolicy() {
        checkCommandLength(2);
        getBucketObjectName(cmd.getArgs()[1]);

        if (bucketName.isEmpty()) {
            printError("Bucket name cannot be empty");
        }

        try {
            BucketPolicy bucketPolicy = client.getBucketPolicy(bucketName);
            if (bucketPolicy.getPolicyText() != null) {
                System.out.println(bucketPolicy.getPolicyText());
            } else {
                System.out.println("Bucket policy not found");
            }
        } catch (AmazonServiceException awsServiceException) {
            printAwsServiceException(awsServiceException);
        } catch (AmazonClientException awsClientException) {
            printError(awsClientException.toString());
        }
    }

    public void deleteBucketPolicy() {
        checkCommandLength(2);
        getBucketObjectName(cmd.getArgs()[1]);

        if (bucketName.isEmpty()) {
            printError("Bucket name cannot be empty");
        }

        try {
            client.deleteBucketPolicy(bucketName);
            System.out.println("Successfully deleted bucket policy");
        } catch (AmazonServiceException awsServiceException) {
            printAwsServiceException(awsServiceException);
        } catch (AmazonClientException awsClientException) {
            printError(awsClientException.toString());
        }
    }

    private String getPolicyText(String policyFile) {
        String policyText = null;

        try {
            policyText = new String(Files.readAllBytes(Paths.get(policyFile)), StandardCharsets.UTF_8);
        } catch (Exception e) {
            printError(e.getMessage());
        }

        return policyText;
    }

    private void printGrants(List <Grant> grants) {
        System.out.println("ACL:");
        for (Grant grant : grants) {
            if (grant.getGrantee() instanceof CanonicalGrantee) {
                CanonicalGrantee canonicalGrantee = (CanonicalGrantee) grant.getGrantee();
                System.out.println(canonicalGrantee.getIdentifier() + ": " +
                                   grant.getPermission());
            } else {
                if (isAnnonRead(grant)) {
                    System.out.println("*anon*: " + grant.getPermission());
                }
            }
        }
    }

    private void printAwsServiceException(AmazonServiceException awsServiceException) {
        if (awsServiceException.getErrorCode().equals("NoSuchBucket")) {
            printError("No such bucket");
        } else if (awsServiceException.getErrorCode().equals("NoSuchKey"))
            printError("No such object");
        else {
            printError(awsServiceException.toString());
        }
    }

    private void getBucketObjectName(String url) {
        Pattern pattern = Pattern.compile("s3://(.*?)$");
        Matcher matcher = pattern.matcher(url);
        String token = "";
        if (matcher.find()) {
            token = matcher.group(1);
        } else {
            printError("Incorrect command. Check java client usage.");
        }

        if (token.isEmpty()) {
            bucketName = "";
            keyName = "";
        } else {
            String[] tokens = token.split("/", 2);
            bucketName = tokens[0];
            if (tokens.length == 2) {
                keyName = tokens[1];
            } else {
                keyName = "";
            }
        }
    }

    private void checkCommandLength(int length) {
        if (cmd.getArgs().length < length) {
            printError("Incorrect command");
        }
    }

    private void multiPartUpload(File file) {
        List<PartETag> partETags = new ArrayList<>();

        InitiateMultipartUploadRequest initRequest
                = new InitiateMultipartUploadRequest(bucketName, keyName);

        InitiateMultipartUploadResult initResponse = null;
        try {
            initResponse = client.initiateMultipartUpload(initRequest);
        } catch (AmazonClientException ex) {
            printError(ex.toString());
        }

        System.out.println("Upload id - " + initResponse.getUploadId());

        long contentLength = file.length();
        long partSize = Integer.parseInt(cmd.getOptionValue("m")) * 1024 * 1024;

        try {
            long filePosition = 0;
            for (int i = 1; filePosition < contentLength; i++) {
                partSize = Math.min(partSize, (contentLength - filePosition));

                UploadPartRequest uploadRequest = new UploadPartRequest()
                        .withBucketName(bucketName).withKey(keyName)
                        .withUploadId(initResponse.getUploadId())
                        .withPartNumber(i)
                        .withFileOffset(filePosition)
                        .withFile(file)
                        .withPartSize(partSize);

                System.out.println("Uploading part " + i + " of size "
                        + partSize / (1024 * 1024) + "MB");
                partETags.add(client.uploadPart(uploadRequest).getPartETag());

                filePosition += partSize;
            }

            CompleteMultipartUploadRequest compRequest
                    = new CompleteMultipartUploadRequest(bucketName, keyName,
                            initResponse.getUploadId(), partETags);

            client.completeMultipartUpload(compRequest);
            System.out.println("File successfully uploaded.");
        } catch (AmazonClientException e) {
            client.abortMultipartUpload(new AbortMultipartUploadRequest(
                    bucketName, keyName, initResponse.getUploadId()));
            printError("Multipart upload failed." + e.getMessage());
        }
    }

    private void verifyCreds() {
        if (!(cmd.hasOption("x") && cmd.hasOption("y"))) {
            printError("Provide access Key and secret key");
        }
    }

    private void printError(String msg) {
        System.err.println(msg);
        System.exit(1);
    }
}
