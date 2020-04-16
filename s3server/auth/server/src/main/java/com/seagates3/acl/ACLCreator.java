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
 * Original author:  Shalaka Dharap
 * Original creation date: 08-Aug-2019
 */

package com.seagates3.acl;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.xml.parsers.ParserConfigurationException;
import javax.xml.transform.TransformerException;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.xml.sax.SAXException;

import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.exception.GrantListFullException;
import com.seagates3.exception.InternalServerException;
import com.seagates3.model.Account;
import com.seagates3.model.Group;
import com.seagates3.model.Requestor;
import com.seagates3.response.generator.AuthorizationResponseGenerator;
import com.seagates3.util.ACLPermissionUtil;
import com.seagates3.util.BinaryUtil;

public
class ACLCreator {

 private
  final Logger LOGGER = LoggerFactory.getLogger(ACLCreator.class.getName());
 protected
  static final Map<String, String> actualPermissionsMap = new HashMap<>();
  AuthorizationResponseGenerator responseGenerator =
      new AuthorizationResponseGenerator();
 private
  static String defaultACP = null;

 public
  ACLCreator() { initPermissionsMap(); }

 private
  void initPermissionsMap() {
    actualPermissionsMap.put("x-amz-grant-read", "READ");
    actualPermissionsMap.put("x-amz-grant-write", "WRITE");
    actualPermissionsMap.put("x-amz-grant-read-acp", "READ_ACP");
    actualPermissionsMap.put("x-amz-grant-write-acp", "WRITE_ACP");
    actualPermissionsMap.put("x-amz-grant-full-control", "FULL_CONTROL");
  }

  /**
   * Returns a default {@link AccessControlPolicy}
   * @param requestor
   * @return
   * @throws IOException
   * @throws ParserConfigurationException
   * @throws SAXException
   * @throws GrantListFullException
   * @throws TransformerException
   */
 public
  AccessControlPolicy initDefaultAcp(Requestor requestor) throws IOException,
      ParserConfigurationException, SAXException, GrantListFullException,
      TransformerException {

    AccessControlPolicy acp =
        new AccessControlPolicy(checkAndCreateDefaultAcp());
    if (requestor != null) {
      acp.initDefaultACL(requestor.getAccount().getCanonicalId(),
                         requestor.getAccount().getName());
    } else {
      acp.initDefaultACL("", "");
    }
    return acp;
  }

  /**
   * Returns a default ACL XML string
   *
   * @param acp
   * @param requestor
   * @return
   * @throws SAXException
   * @throws ParserConfigurationException
   * @throws IOException
   * @throws GrantListFullException
   * @throws TransformerException
   */
 public
  String createDefaultAcl(Requestor requestor) throws IOException,
      ParserConfigurationException, SAXException, GrantListFullException,
      TransformerException {
    return initDefaultAcp(requestor).getXml();
  }

  /**
   * Below method updates existing grantee list with permission headers
   * requested
   *
   * @param acp
   * @param accountPermissionMap
   * @return
   * @throws GrantListFullException
   * @throws SAXException
   * @throws ParserConfigurationException
   * @throws IOException
   * @throws TransformerException
   */
 public
  String createAclFromPermissionHeaders(
      Requestor requestor, Map<String, List<Account>> accountPermissionMap,
      Map<String, List<Group>> groupPermissionMap,
      Map<String, String> requestBody) throws GrantListFullException,
      IOException, ParserConfigurationException, SAXException,
      TransformerException {
    AccessControlPolicy acp = null;
    AccessControlList newAcl = new AccessControlList();
    acp = new AccessControlPolicy(checkAndCreateDefaultAcp());
    updateAclFromAccountMap(accountPermissionMap, newAcl);
    updateAclFromGroupMap(groupPermissionMap, newAcl);

    // Update the owner detils of resource
    AccessControlPolicy existingAcp = null;
    if (requestBody.get("Auth-ACL") != null) {
      existingAcp = new AccessControlPolicy(
          BinaryUtil.base64DecodeString(requestBody.get("Auth-ACL")));
    }
    Owner owner = getOwner(requestor, existingAcp, requestBody);
    acp.setOwner(owner);
    acp.setAccessControlList(newAcl);
    return acp.getXml();
  }

 private
  void updateAclFromAccountMap(Map<String, List<Account>> accountPermissionMap,
                               AccessControlList acl)
      throws GrantListFullException {
    for (String permission : accountPermissionMap.keySet()) {
      for (Account account : accountPermissionMap.get(permission)) {
        Grantee grantee =
            new Grantee(account.getCanonicalId(), account.getName());
        Grant grant = new Grant(grantee, actualPermissionsMap.get(permission));
        acl.addGrant(grant);
        LOGGER.info("Updated the acl for " + account.getName() +
                    " with permission - " + permission);
      }
    }
  }

 private
  void updateAclFromGroupMap(Map<String, List<Group>> groupPermissionMap,
                             AccessControlList acl)
      throws GrantListFullException {
    for (String permission : groupPermissionMap.keySet()) {
      for (Group group : groupPermissionMap.get(permission)) {
        Grantee grantee =
            new Grantee(null, null, group.getPath(), null, Grantee.Types.Group);
        Grant grant = new Grant(grantee, actualPermissionsMap.get(permission));
        acl.addGrant(grant);
        LOGGER.info("Updated the acl for " + group.getPath() +
                    " with permission - " + permission);
      }
    }
    }

   protected
    String checkAndCreateDefaultAcp() throws IOException,
        ParserConfigurationException, SAXException, GrantListFullException {
      if (defaultACP == null) {
        defaultACP = new String(
            Files.readAllBytes(Paths.get(AuthServerConfig.authResourceDir +
                                         AuthServerConfig.DEFAULT_ACL_XML)));
      }
      return defaultACP;
    }

    /**
    * Creates the ACL XML from canned ACL request
    * @param requestor
    * @param requestBody
    * @return
    * @throws TransformerException
    * @throws GrantListFullException
    * @throws SAXException
    * @throws ParserConfigurationException
    * @throws IOException
     * @throws InternalServerException
       */
   public
    String createACLFromCannedInput(Requestor requestor,
                                    Map<String, String> requestBody)
        throws IOException,
        ParserConfigurationException, SAXException, GrantListFullException,
        TransformerException, InternalServerException {

      String cannedInput = requestBody.get("x-amz-acl");
      AccessControlPolicy acp = initDefaultAcp(requestor);
      AccessControlList acl = acp.getAccessControlList();
      AccessControlPolicy existingAcp = null;
      if (requestBody.get("Auth-ACL") != null) {
        existingAcp = new AccessControlPolicy(
            BinaryUtil.base64DecodeString(requestBody.get("Auth-ACL")));
      }
      Owner bucketOwner;
      Owner owner = getOwner(requestor, existingAcp, requestBody);
      acp.setOwner(owner);
      String errorMessage = null;

      switch (cannedInput) {

        case "private":
          acl.clearGrantList();
          acl.addGrant(getOwnerGrant(requestor, existingAcp, requestBody));
          break;

        case "public-read":
          acl.clearGrantList();
          acl.addGrant(getOwnerGrant(requestor, existingAcp, requestBody));
          acl.addGrant(new Grant(new Grantee(null, null, Group.AllUsersURI,
                                             null, Grantee.Types.Group),
                                 "READ"));
          break;

        case "public-read-write":
          acl.clearGrantList();
          acl.addGrant(getOwnerGrant(requestor, existingAcp, requestBody));
          acl.addGrant(new Grant(new Grantee(null, null, Group.AllUsersURI,
                                             null, Grantee.Types.Group),
                                 "READ"));
          acl.addGrant(new Grant(new Grantee(null, null, Group.AllUsersURI,
                                             null, Grantee.Types.Group),
                                 "WRITE"));
          break;

        case "authenticated-read":
          acl.clearGrantList();
          acl.addGrant(getOwnerGrant(requestor, existingAcp, requestBody));
          acl.addGrant(
              new Grant(new Grantee(null, null, Group.AuthenticatedUsersURI,
                                    null, Grantee.Types.Group),
                        "READ"));
          break;

        case "bucket-owner-read":

          // Ignore if bucket-owner-read is applied while creating a bucket
          if (isIgnoreBucketAclUpdate(requestBody)) break;

          acl.clearGrantList();
          acl.addGrant(getOwnerGrant(requestor, existingAcp, requestBody));
          bucketOwner = getbucketOwner(requestBody);
          // if bucket owner and object owner are different then only add
          // requested
          // permission.
          // This is to avoid permission duplication.
          if (bucketOwner != null &&
              !acl.getGrantList().get(0).getGrantee().getCanonicalId().equals(
                   bucketOwner.getCanonicalId())) {
            acl.addGrant(new Grant(new Grantee(bucketOwner.getCanonicalId(),
                                               bucketOwner.getDisplayName()),
                                   "READ"));
            LOGGER.debug(
                "Bucket owner with READ permission got added successfully");
          }
          break;

        case "bucket-owner-full-control":

          // Ignore if bucket-owner-read is applied while creating a bucket
          if (isIgnoreBucketAclUpdate(requestBody)) break;

          acl.clearGrantList();
          acl.addGrant(getOwnerGrant(requestor, existingAcp, requestBody));
          bucketOwner = getbucketOwner(requestBody);
          // if bucket owner and object owner are different then only add
          // requested
          // permission.
          // This is to avoid permission duplication.
          if (bucketOwner != null &&
              !acl.getGrantList().get(0).getGrantee().getCanonicalId().equals(
                   bucketOwner.getCanonicalId())) {
            acl.addGrant(new Grant(new Grantee(bucketOwner.getCanonicalId(),
                                               bucketOwner.getDisplayName()),
                                   "FULL_CONTROL"));
            LOGGER.debug(
                "Bucket owner with FULL_CONTROL got added successfully");
          }
          break;

        case "log-delivery-write":
          errorMessage = "log-delivery-write canned input is not supported.";
          LOGGER.error(errorMessage);
          throw new InternalServerException(
              responseGenerator.operationNotSupported());

        case "aws-exec-read":
          errorMessage = "aws-exec-read canned input is not supported.";
          LOGGER.error(errorMessage);
          throw new InternalServerException(
              responseGenerator.operationNotSupported());

        default:
          throw new InternalServerException(responseGenerator.invalidArgument(
              "Invalid canned ACL input - " + cannedInput));
      }
      return acp.getXml();
    }

    /**
     * Get the {@link Owner} from {@link AccessControlPolicy} or {@link
     * Requestor}
     * @param requestor
     * @param acp
     * @param requestBody
     * @return
     * @throws TransformerException
     */
   private
    Owner getOwner(Requestor requestor, AccessControlPolicy acp,
                   Map<String, String> requestBody)
        throws TransformerException {
      Owner grant;
      // For put-acl call get the existing resource owner details

      String uri = requestBody.get("ClientAbsoluteUri");
      String queryParam = requestBody.get("ClientQueryParams");
      if (acp != null && ((uri != null || queryParam != null) &&
                          ACLPermissionUtil.isACLReadWrite(uri, queryParam) &&
                          requestBody.get("Method").equalsIgnoreCase("PUT")) ||
          requestor == null) {
        grant = acp.owner;
      } else {
        grant = new Owner(requestor.getAccount().getCanonicalId(),
                          requestor.getAccount().getName());
      }
      return grant;
    }

    /**
         * Return the bucket owner
         * @param requestBody
         * @return bucket owner
         * @throws GrantListFullException
         * @throws IOException
         * @throws SAXException
         * @throws ParserConfigurationException
         */
   private
    Owner getbucketOwner(Map<String, String> requestBody)
        throws ParserConfigurationException,
        SAXException, IOException, GrantListFullException {
      Owner owner = null;
      // Bucket-Acl will be present in request body only if the request is a
      // put-acl. For put-acl call get the existing resource owner details
      if (requestBody.get("Bucket-ACL") != null) {
        owner = new AccessControlPolicy(
            BinaryUtil.base64DecodeString(requestBody.get("Bucket-ACL")))
                    .getOwner();
      } else {
        owner = new AccessControlPolicy(
            BinaryUtil.base64DecodeString(requestBody.get("Auth-ACL")))
                    .getOwner();
      }
      return owner;
    }

    /**
     * Return the Grant of the {@link Owner}
     * @param requestor
     * @param acp
     * @param requestBody
     * @return
     * @throws ParserConfigurationException
     * @throws SAXException
     * @throws IOException
     * @throws GrantListFullException
     */
   private
    Grant getOwnerGrant(Requestor requestor, AccessControlPolicy acp,
                        Map<String, String> requestBody)
        throws ParserConfigurationException,
        SAXException, IOException, GrantListFullException {

      Grant grant;
      // Bucket-Acl will be present in request body only if the request is a
      // put-acl. For put-acl call get the existing resource owner details
      if ((acp != null && requestBody.get("Bucket-ACL") != null) ||
          requestor == null) {
        grant = new Grant(
            new Grantee(acp.getOwner().canonicalId, acp.getOwner().displayName),
            "FULL_CONTROL");
      } else {
        grant = new Grant(new Grantee(requestor.getAccount().getCanonicalId(),
                                      requestor.getAccount().getName()),
                          "FULL_CONTROL");
      }
      return grant;
    }

    /**
     * Checks if the bucket-owner-* canned ACL update should be ignored for this
     * request. The operation should be ignored if it is put-bucket.
     * @param requestBody
     * @return
     */
   private
    static boolean isIgnoreBucketAclUpdate(Map<String, String> requestBody) {

      // Check if the PUT operation is on the bucket and its not put-bucket-acl
      return (isOperationOnBucket(requestBody) &&
              !ACLPermissionUtil.isACLReadWrite(
                   requestBody.get("ClientAbsoluteUri"),
                   requestBody.get("ClientQueryParams")));
    }

    /**
     * Checks if the operations is performed on a bucket
     * @param requestBody
     * @return - true if operation is performed on bucket
     */
   private
    static boolean isOperationOnBucket(Map<String, String> requestBody) {

      String clientAbsoluteUri = requestBody.get("ClientAbsoluteUri");
      boolean result = false;

      // Check if the ClientAbsoluteUri contains only the bucket name
      if (clientAbsoluteUri != null && !clientAbsoluteUri.isEmpty()) {
        String[] arr = clientAbsoluteUri.split("/");

        if (arr.length >= 3) {
          result = false;
        } else {
          if (arr.length == 2) {
            if ((arr[0] != null && arr[0].isEmpty()) ||
                (arr[1] != null && arr[1].isEmpty()))
              result = true;
          } else if (arr.length == 1) {
            result = true;
          }
        }
      }
      return result;
    }
}


