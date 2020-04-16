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
 * Original creation date: 18-Feb-2016
 */
package com.seagates3.jcloudclient;

import com.google.common.collect.ImmutableSet;
import com.google.inject.Module;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Properties;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import org.apache.commons.cli.CommandLine;
import org.jclouds.ContextBuilder;
import org.jclouds.blobstore.BlobStore;
import org.jclouds.blobstore.BlobStoreContext;
import org.jclouds.blobstore.domain.Blob;
import org.jclouds.blobstore.domain.BlobMetadata;
import org.jclouds.blobstore.domain.PageSet;
import org.jclouds.blobstore.domain.StorageMetadata;
import org.jclouds.blobstore.options.ListContainerOptions;
import org.jclouds.io.Payload;
import org.jclouds.io.PayloadSlicer;
import org.jclouds.io.internal.BasePayloadSlicer;
import org.jclouds.io.payloads.FilePayload;
import org.jclouds.logging.slf4j.config.SLF4JLoggingModule;
import org.jclouds.s3.S3ApiMetadata;
import org.jclouds.s3.S3Client;
import org.jclouds.s3.domain.AccessControlList;
import org.jclouds.s3.domain.AccessControlList.CanonicalUserGrantee;
import org.jclouds.s3.domain.AccessControlList.Grant;
import org.jclouds.s3.domain.AccessControlList.Grantee;
import org.jclouds.s3.domain.AccessControlList.GroupGrantee;
import org.jclouds.s3.domain.AccessControlList.GroupGranteeURI;
import org.jclouds.s3.domain.AccessControlList.Permission;
import org.jclouds.s3.domain.ObjectMetadata;
import org.jclouds.s3.domain.ObjectMetadataBuilder;

import static org.jclouds.Constants.PROPERTY_CONNECTION_TIMEOUT;
import static org.jclouds.Constants.PROPERTY_REQUEST_TIMEOUT;
import static org.jclouds.Constants.PROPERTY_SO_TIMEOUT;
import static org.jclouds.blobstore.options.PutOptions.Builder.multipart;

public class S3JcloudAPI {

    BlobStoreContext context;
    BlobStore blobStore;
    S3Client s3client;

    private final CommandLine cmd;
    private String bucketName;
    private String keyName;

    public S3JcloudAPI(CommandLine cmd) throws IOException {
        this.cmd = cmd;
        verifyCreds();

        ContextBuilder builder = null;
        Iterable<Module> modules = ImmutableSet.<Module>of(
                new SLF4JLoggingModule());

        if (cmd.hasOption("t")) {
            throw new UnsupportedOperationException("Temporary credentials "
                    + "are not supported");
        } else {
            try {
                builder = ContextBuilder.newBuilder(new S3ApiMetadata())
                        .credentials(cmd.getOptionValue("x"), cmd.getOptionValue("y"))
                        .endpoint("https://s3.amazonaws.com")
                        .modules(modules);
            } catch (Exception ex) {
                System.out.println(ex.toString());
            }
        }

        if (!cmd.hasOption("a")) {
            String endPoint = ClientConfig.getEndPoint(ClientService.S3);
            if (!endPoint.isEmpty()) {
                builder.endpoint(endPoint);
            }
        }

        Properties properties = new Properties();
        if (cmd.hasOption("p")) {
            properties.setProperty("jclouds.s3.virtual-host-buckets", "false");
        } else {
            properties.setProperty("jclouds.s3.virtual-host-buckets", "true");
        }

        if (cmd.hasOption("m")) {
            int partSize = Integer.parseInt(cmd.getOptionValue("m")) * 1024 * 1024;
            properties.setProperty("jclouds.mpu.parts.size", String.valueOf(partSize));
        }

        if (cmd.hasOption("cli-exec-timeout")) {
            properties.setProperty(PROPERTY_CONNECTION_TIMEOUT, cmd.getOptionValue("cli-exec-timeout"));
        }
        if (cmd.hasOption("req-timeout")) {
            properties.setProperty(PROPERTY_REQUEST_TIMEOUT, cmd.getOptionValue("req-timeout"));
        }
        if (cmd.hasOption("sock-timeout")) {
            properties.setProperty(PROPERTY_SO_TIMEOUT, cmd.getOptionValue("sock-timeout"));
        }

        builder.overrides(properties);
        context = builder.buildView(BlobStoreContext.class);
        blobStore = context.getBlobStore();
        s3client = S3Client.class.cast(context.unwrapApi(S3Client.class));
    }

    public void makeBucket() {
        checkCommandLength(2);
        getBucketObjectName(cmd.getArgs()[1]);
        if (bucketName.isEmpty()) {
            printError("Incorrect command. Bucket name is required.");
        }

        try {
            blobStore.createContainerInLocation(null, bucketName);
            System.out.println("Bucket created successfully.");
        } catch (Exception e) {
            printError(e.toString());
        } finally {
            context.close();
        }
    }

    public void removeBucket() {
        checkCommandLength(2);
        getBucketObjectName(cmd.getArgs()[1]);
        if (bucketName.isEmpty()) {
            printError("Incorrect command. Bucket name is required.");
        }

        try {
            blobStore.deleteContainer(bucketName);
            System.out.println("Bucket deleted successfully.");
        } catch (Exception e) {
            printError(e.toString());
        } finally {
            context.close();
        }
    }

    public void removeBucketIfEmpty() {
        checkCommandLength(2);
        getBucketObjectName(cmd.getArgs()[1]);
        if (bucketName.isEmpty()) {
            printError("Incorrect command. Bucket name is required.");
        }

        try {
            if (blobStore.deleteContainerIfEmpty(bucketName)) {
                System.out.println("Bucket deleted successfully.");
            } else {
                System.out.println("Bucket does not exist or it is not empty.");
            }
        } catch (Exception e) {
            printError(e.toString());
        } finally {
            context.close();
        }
    }

    public void putObject() {
        checkCommandLength(3);
        String fileName = cmd.getArgs()[1];
        File file = new File(fileName);

        if (!file.exists()) {
            printError("Given file doesn't exist.");
        }

        getBucketObjectName(cmd.getArgs()[2]);

        if (bucketName.isEmpty()) {
            printError("Incorrect command. Bucket name is required.");
            printError("Incorrect command. Check jcloud client usage.");
        }

        if (keyName.isEmpty()) {
            keyName = file.getName();
        } else if (keyName.endsWith("/")) {
            keyName += file.getName();
        }

        try {
            FilePayload payload = new FilePayload(file);
            Blob blob = blobStore.blobBuilder(keyName)
                    .payload(payload)
                    .contentLength(file.length())
                    .build();

            if (cmd.hasOption("m")) {
                String eTag = blobStore.putBlob(bucketName, blob, multipart());
                verifyEtag(file, eTag);
            } else {
                blobStore.putBlob(bucketName, blob);
            }

            System.out.println("Object put successfully.");
        } catch (Exception e) {
            printError(e.toString());
        } finally {
            context.close();
        }
    }

    protected void verifyEtag(File file, String eTag) {
        if (cmd.hasOption("e") &&
                cmd.getOptionValue("e").equalsIgnoreCase("False"))
            return;

        EtagGenerator generator = new EtagGenerator(file, Long.parseLong(
                cmd.getOptionValue("m")));
        String expectedEtag = "\"" + generator.getEtag() + "\"";
        if (!eTag.equals(expectedEtag))
            printError("The two ETags (" + expectedEtag + ", " + eTag +
                    ") do not match.");
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
            Blob blob = blobStore.getBlob(bucketName, keyName);
            if (blob == null) {
              printError("No such Bucket or Object.");
            }
            InputStream inputStream = blob.getPayload().openStream();
            File file = new File(cmd.getArgs()[2]);

            OutputStream outputStream = new FileOutputStream(file);

            int read;
            byte[] bytes = new byte[1024];

            while ((read = inputStream.read(bytes)) != -1) {
                outputStream.write(bytes, 0, read);
            }

            System.out.println("Object download successfully.");
        } catch (Exception ex) {
            printError("Error occured while copying s3ObjectStream to target "
                    + "file.\n" + ex.toString());
        } finally {
            context.close();
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
            blobStore.removeBlob(bucketName, keyName);
            System.out.println("Object deleted successfully.");
        } catch (Exception e) {
            printError(e.toString());
        } finally {
            context.close();
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
        Iterable<String> objects = cmd.getArgList().subList(2, cmd.getArgList().size());

        try {
            blobStore.removeBlobs(bucketName, objects);
            System.out.println("Objects deleted successfully.");
        } catch (Exception e) {
            printError(e.toString());
        } finally {
            context.close();
        }
    }

    public void headObject() {
        checkCommandLength(2);
        getBucketObjectName(cmd.getArgs()[1]);

        if (bucketName.isEmpty() || keyName.isEmpty()) {
            printError("Provider bucket and object name.");
        }
        try {
            BlobMetadata blobMetadata = blobStore.blobMetadata(bucketName, keyName);
            if (blobMetadata == null) {
                printError("Bucket or Object does not exist.");
            } else {
                String metadata = "Bucket name- " + blobMetadata.getContainer();
                metadata += "\nObject name - " + blobMetadata.getName();
                metadata += "\nObject size - " + blobMetadata.getSize();
                metadata += "\nEtag - " + blobMetadata.getETag();
                metadata += "\nLastModified - " + blobMetadata.getLastModified();

                System.out.println(metadata);
            }
        } catch (Exception e) {
            printError(e.toString());
        } finally {
            context.close();
        }
    }

    public void countObjects() {
        checkCommandLength(2);
        getBucketObjectName(cmd.getArgs()[1]);

        if (bucketName.isEmpty()) {
            printError("Invalid command. Bucket name is required.");
        }
        try {
            long objectCount = blobStore.countBlobs(bucketName);
            System.out.println("The bucket has " + objectCount + " objects.");
        } catch (Exception e) {
            printError(e.toString());
        } finally {
            context.close();
        }
    }

    public void countDirectoryObjects() {
        checkCommandLength(3);
        getBucketObjectName(cmd.getArgs()[1]);

        if (bucketName.isEmpty()) {
            printError("Invalid command. Bucket name is required.");
        }

        if (cmd.getArgs()[2].isEmpty()) {
            System.out.println("Directory name is required.");
        }

        ListContainerOptions opts = new ListContainerOptions();
        opts.inDirectory(cmd.getArgs()[2]);

        try {
            long objectCount = blobStore.countBlobs(bucketName, opts);
            System.out.println("The directory has " + objectCount + " objects.");
        } catch (Exception e) {
            printError(e.toString());
        } finally {
            context.close();
        }
    }

    public void createDirectory() {
        checkCommandLength(2);
        getBucketObjectName(cmd.getArgs()[1]);

        if (bucketName.isEmpty() || keyName.isEmpty()) {
            printError("Provide bucket and directory name.");
        }

        try {
            blobStore.createDirectory(bucketName, keyName);
            System.out.println("Directory created.");
        } catch (Exception e) {
            printError(e.toString());
        } finally {
            context.close();
        }
    }

    public void deleteDirectory() {
        checkCommandLength(2);
        getBucketObjectName(cmd.getArgs()[1]);

        if (bucketName.isEmpty() || keyName.isEmpty()) {
            printError("Provide bucket and directory name.");
        }

        try {
            blobStore.deleteDirectory(bucketName, keyName);
            System.out.println("Directory deleted.");
        } catch (Exception e) {
            printError(e.toString());
        } finally {
            context.close();
        }
    }

    /**
     * Directory exists is not supported by AWS. We could add this feature as a
     * part of our implementation.
     */
//    public void directoryExists() {
//        checkCommandLength(3);
//        getBucketObjectName(cmd.getArgs()[1]);
//
//        if (bucketName.isEmpty()) {
//            printError("Bucket name is required.");
//        }
//
//        try {
//            if (blobStore.directoryExists(bucketName, cmd.getArgs()[2])) {
//                System.out.println("Directory exists.");
//            } else {
//                System.out.println("Directory does not exist..");
//            }
//        } catch (Exception e) {
//            printError(e.toString());
//        } finally {
//            context.close();
//        }
//    }
    public void clearBucket() {
        checkCommandLength(2);
        getBucketObjectName(cmd.getArgs()[1]);

        if (bucketName.isEmpty()) {
            printError("Incorrect command. Bucket name is required.");
        }

        try {
            blobStore.clearContainer(bucketName);
            System.out.println("Bucket cleared.");
        } catch (Exception e) {
            printError(e.toString());
        } finally {
            context.close();
        }
    }

    public void clearBucketDirectory() {
        checkCommandLength(3);
        getBucketObjectName(cmd.getArgs()[1]);

        if (bucketName.isEmpty()) {
            printError("Incorrect command. Bucket name is required.");
        }

        ListContainerOptions opts = new ListContainerOptions();
        opts.inDirectory(cmd.getArgs()[2]);

        try {
            blobStore.clearContainer(bucketName, opts);
            System.out.println("Bucket cleared.");
        } catch (Exception e) {
            printError(e.toString());
        } finally {
            context.close();
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
        PageSet<? extends StorageMetadata> list = null;
        String marker;

        if (bucketName.isEmpty()) {
            try {
                System.out.println("Buckets - ");
                list = blobStore.list();
                for (StorageMetadata resourceMd : list) {
                    System.out.println(resourceMd.getName());
                }
            } catch (Exception e) {
                printError(e.toString());
            } finally {
                context.close();
            }

            return;
        }

        System.out.println("Objects - ");
        try {
            ListContainerOptions opt = new ListContainerOptions();
            if (!keyName.isEmpty()) {
                opt.inDirectory(keyName);
            }
            do {
                list = blobStore.list(bucketName, opt);
                for (StorageMetadata resourceMd : list) {
                    System.out.println(resourceMd.getName());
                }

                marker = list.getNextMarker();
                if (marker != null) {
                    opt.afterMarker(marker);
                }
            } while (marker != null);

        } catch (Exception e) {
            printError(e.toString());
        } finally {
            context.close();
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
                if (blobStore.containerExists(bucketName)) {
                    System.out.println("Bucket " + bucketName + " exists.");
                } else {
                    System.out.println("Bucket " + bucketName
                            + " does not exist.");
                }

            } catch (Exception e) {
                printError(e.toString());
            } finally {
                context.close();
            }
        } else {
            try {
                if (blobStore.blobExists(bucketName, keyName)) {
                    System.out.println("Object " + keyName + " exists.");
                } else {
                    System.out.println("Object " + keyName
                            + " does not exist.");
                }
            } catch (Exception e) {
                printError(e.toString());
            } finally {
                context.close();
            }
        }
    }

    public void uploadPart() {
        checkCommandLength(4);
        String fileName = cmd.getArgs()[1];
        File file = new File(fileName);
        if (!file.exists())
            printError("Given file doesn't exist.");
        getBucketObjectName(cmd.getArgs()[2]);
        if (bucketName.isEmpty())
            printError("Incorrect command. Bucket name is required.");
        if (keyName.isEmpty())
            keyName = file.getName();
        else if (keyName.endsWith("/"))
            keyName += file.getName();
        int numOfParts = Integer.parseInt(cmd.getArgs()[3]);
        long partSize;
        if (cmd.getOptionValue('m') != null)
            partSize = Integer.parseInt(cmd.getOptionValue('m')) * 1024L * 1024L;
        else
            partSize = 16L * 1024L * 1024L;

        try {
            ObjectMetadata objectMetadata = ObjectMetadataBuilder.create().key(keyName).build();
            String uploadId = s3client.initiateMultipartUpload(bucketName, objectMetadata);
            System.out.println("Upload id - " + uploadId);

            int partNumber = 1;
            FilePayload payload = new FilePayload(file);
            PayloadSlicer slicer = new BasePayloadSlicer();
            for (Payload partPayload: slicer.slice(payload, partSize)) {
                String eTag;
                do {
                    System.out.println("Uploading part " + partNumber
                            + " of size " + partSize / (1024 * 1024) + "MB");
                    eTag = s3client.uploadPart(bucketName, keyName, partNumber, uploadId, partPayload);
                } while (eTag == null);
                if (partNumber++ == numOfParts)
                    break;
            }
        } catch (Exception e) {
            printError(e.getMessage());
        } finally {
            context.close();
        }
    }

    public void abortMultipartUpload() {
        checkCommandLength(3);
        getBucketObjectName(cmd.getArgs()[1]);
        String uploadID = cmd.getArgs()[2];

        if (bucketName.isEmpty())
            printError("Incorrect command. Bucket name is required.");
        if (keyName.isEmpty())
            printError("Incorrect command. Object name is required.");
        if (uploadID.isEmpty())
            printError("Incorrect command. UploadId is required.");

        try {
            s3client.abortMultipartUpload(bucketName, keyName, uploadID);
            System.out.println("Upload aborted successfully.");
        } catch (Exception e) {
            printError(e.getMessage());
        } finally {
            context.close();
        }
    }

    public void listMultipartUploads() {
        throw new UnsupportedOperationException("listMultipartUploads not Implemented");
    }

    public void listParts() {
        throw new UnsupportedOperationException("listParts not Implemented");
    }

    public void setAcl() {
        checkCommandLength(3);
        getBucketObjectName(cmd.getArgs()[1]);

        if (bucketName.isEmpty())
            printError("Incorrect command. Bucket name is required.");

        Map<String, String> requestMap = parseGrantRequesst(cmd.getArgs()[2]);
        String permission = requestMap.get("permission");
        if (permission != null && !permission.isEmpty())
            permission = validateAndGetPermission(permission);

        String action = requestMap.get("action");
        if (action.equals("grant"))
            grantAcl(requestMap.get("canonicalID"),requestMap.get("displayName"), permission);
        else if (action.equals("revoke"))
            revokeAcl(requestMap.get("canonicalID"), permission);
        else if (action.equals("acl-private"))
            setAclPrivate();
        else if (action.equals("acl-public"))
            setAclPublic();
        else
            printError("Unknown ACL action");
    }

    private String validateAndGetPermission(String permission) {
        switch (permission.toUpperCase()) {
            case "FULL_CONTROL": permission = Permission.FULL_CONTROL;
                break;
            case "READ": permission = Permission.READ;
                break;
            case "READ_ACP": permission = Permission.READ_ACP;
                break;
            case "WRITE": permission = Permission.WRITE;
                break;
            case "WRITE_ACP": permission =  Permission.WRITE_ACP;
                break;
            default:
                printError("Unknown permission. Incorrect command.");
        }

        return permission;
    }

    private void grantAcl(String canonicalID, String displayName, String permission) {
        try {
            if (canonicalID == null || canonicalID.isEmpty()) {
              printError("Invalid canonical id.");
            }
            CanonicalUserGrantee canonicalUserGrantee;
            if (displayName == null || displayName.isEmpty()) {
              canonicalUserGrantee = new CanonicalUserGrantee(canonicalID);
            } else {
              canonicalUserGrantee = new CanonicalUserGrantee(canonicalID, displayName);
            }

            if (keyName.isEmpty()) {
                AccessControlList acl = s3client.getBucketACL(bucketName);
                acl.addPermission(canonicalUserGrantee, permission);
                s3client.putBucketACL(bucketName, acl);
            } else {
                AccessControlList acl = s3client.getObjectACL(bucketName, keyName);
                acl.addPermission(canonicalUserGrantee, permission);
                s3client.putObjectACL(bucketName, keyName, acl);
            }
            System.out.println("Grant ACL successful");
        } catch (Exception e) {
            printError(e.getMessage());
        } finally {
            context.close();
        }
    }

    private void revokeAcl(String canonicalID, String permission) {
        try {
            if (canonicalID == null || canonicalID.isEmpty()) {
              printError("Invalid canonical id.");
            }
            CanonicalUserGrantee canonicalUserGrantee = new CanonicalUserGrantee(canonicalID);
            if (keyName.isEmpty()) {
                AccessControlList acl = s3client.getBucketACL(bucketName);
                acl.revokePermission(canonicalUserGrantee, permission);
                s3client.putBucketACL(bucketName, acl);
            } else {
                AccessControlList acl = s3client.getObjectACL(bucketName, keyName);
                acl.revokePermission(canonicalUserGrantee, permission);
                s3client.putObjectACL(bucketName, keyName, acl);
            }
            System.out.println("Revoke ACL successful");
        } catch (Exception e) {
            printError(e.getMessage());
        } finally {
            context.close();
        }
    }

    private void setAclPrivate() {
        try {
            if (keyName.isEmpty()) {
                AccessControlList acl = s3client.getBucketACL(bucketName);
                if (aclHasAnnonRead(acl)) {
                    acl.revokePermission(GroupGranteeURI.ALL_USERS, Permission.FULL_CONTROL);
                    acl.revokePermission(GroupGranteeURI.ALL_USERS, Permission.READ);
                    s3client.putBucketACL(bucketName, acl);
                } else {
                    System.out.println("Already private. Skipping.");
                    return;
                }
            } else {
                AccessControlList acl = s3client.getObjectACL(bucketName, keyName);
                if (aclHasAnnonRead(acl)) {
                    acl.revokePermission(GroupGranteeURI.ALL_USERS, Permission.FULL_CONTROL);
                    acl.revokePermission(GroupGranteeURI.ALL_USERS, Permission.READ);
                    s3client.putObjectACL(bucketName, keyName, acl);
                } else {
                    System.out.println("Already private. Skipping.");
                    return;
                }
            }
            System.out.println("ACL set to Private.");
        } catch (Exception e) {
            printError(e.getMessage());
        } finally {
            context.close();
        }
    }

    private void setAclPublic() {
        try {
            if (keyName.isEmpty()) {
                AccessControlList acl = s3client.getBucketACL(bucketName);
                if (!aclHasAnnonRead(acl)) {
                    acl.addPermission(getAnnonGrantee(), Permission.READ);
                    s3client.putBucketACL(bucketName, acl);
                } else {
                    System.out.println("Already public. Skipping.");
                    return;
                }
            } else {
                AccessControlList acl = s3client.getObjectACL(bucketName, keyName);
                if (!aclHasAnnonRead(acl)) {
                    acl.addPermission(getAnnonGrantee(), Permission.READ);
                    s3client.putObjectACL(bucketName, keyName, acl);
                } else {
                    System.out.println("Already public. Skipping.");
                    return;
                }
            }
            System.out.println("ACL set to Public.");
        } catch (Exception e) {
            printError(e.getMessage());
        } finally {
            context.close();
        }
    }

    public void getAcl() {
        checkCommandLength(2);
        getBucketObjectName(cmd.getArgs()[1]);

        if (bucketName.isEmpty()) {
            printError("Incorrect command. Bucket name is required.");
        }
        AccessControlList acl = new AccessControlList();
        try {
            if (keyName.isEmpty()) {
                acl = s3client.getBucketACL(bucketName);
            } else {
                acl = s3client.getObjectACL(bucketName, keyName);
            }
        } catch (Exception e) {
            printError(e.getMessage());
        } finally {
            context.close();
        }

        printAcl(acl.getGrants());
    }

    private void printAcl(List<Grant> grants) {
        System.out.println("ACL:");
        for (Grant grant: grants) {
            if (grant.getGrantee() instanceof CanonicalUserGrantee) {
                CanonicalUserGrantee canonicalGrantee = (CanonicalUserGrantee) grant.getGrantee();
                System.out.println(canonicalGrantee.getIdentifier() + ": " +
                                   grant.getPermission());
            } else {
                if (isAnnonRead(grant)) {
                    System.out.println("*anon*: " + grant.getPermission());
                }
            }
        }
    }

    private boolean aclHasAnnonRead(AccessControlList acl) {
        for (Grant grant: acl.getGrants()) {
            if (isAnnonRead(grant))
                return true;
        }

        return false;
    }

    private GroupGrantee getAnnonGrantee() {
        return new GroupGrantee(GroupGranteeURI.ALL_USERS);
    }

    private boolean isAllUsers(Grantee grantee) {
        if (grantee.equals(getAnnonGrantee()))
            return true;

        return false;
    }

    private boolean isAnnonRead(Grant grant) {
        if (isAllUsers(grant.getGrantee()) &&
                (grant.getPermission().equals(Permission.READ) ||
                grant.getPermission().equals(Permission.FULL_CONTROL)))
            return true;

        return false;
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

    public void getBucketLocation() {
        checkCommandLength(2);
        getBucketObjectName(cmd.getArgs()[1]);
        if (bucketName.isEmpty())
            printError("Incorrect command. Bucket name is required.");

        try {
            System.out.println(s3client.getBucketLocation(bucketName));
        } catch (Exception e) {
            printError(e.getMessage());
        } finally {
            context.close();
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

    private void verifyCreds() {
        if (!(cmd.hasOption("x") && cmd.hasOption("y"))) {
            printError("Provide access Key and secret key");
        }
    }

    private void printError(String msg) {
        System.err.println(msg);
        System.exit(1);
    }

    private void checkCommandLength(int length) {
        if (cmd.getArgs().length < length) {
            printError("Incorrect command");
        }
    }
}
