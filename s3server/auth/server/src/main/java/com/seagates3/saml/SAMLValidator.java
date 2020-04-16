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
 * Original creation date: 20-Jan-2016
 */
package com.seagates3.saml;

import com.seagates3.exception.InvalidSAMLResponseException;
import com.seagates3.exception.SAMLInitializationException;
import com.seagates3.exception.SAMLInvalidCertificateException;
import com.seagates3.model.SAMLProvider;
import com.seagates3.model.SAMLResponseTokens;
import com.seagates3.response.generator.SAMLResponseGenerator;
import com.seagates3.util.DateUtil;
import java.util.ArrayList;
import java.util.Date;
import org.joda.time.DateTime;
import org.joda.time.DateTimeZone;

public class SAMLValidator {

    /**
     * Maximum time within which the server should receive the SAML request from
     * the time of issue.
     *
     */
    private final int maxSAMLRequestLatencyMin = 5;

    private final SAMLProvider samlProvider;
    private final SAMLUtil samlutil;

    public SAMLValidator(SAMLProvider samlProvider, SAMLUtil samlutil)
            throws SAMLInitializationException {
        this.samlProvider = samlProvider;
        this.samlutil = samlutil;
    }

    public Boolean validateSAMLResponse(SAMLResponseTokens samlResponseTokens)
            throws InvalidSAMLResponseException {
        SAMLResponseGenerator responseGenerator = new SAMLResponseGenerator();

        if (!samlResponseTokens.isAuthenticationSuccess()
                || !samlResponseTokens.isResponseAudience()) {
            throw new InvalidSAMLResponseException(
                    responseGenerator.idpRejectedClaim());
        }

        DateTime currentDateTime = DateTime.now(DateTimeZone.UTC);
        if (currentDateTime.isAfter(samlResponseTokens.getIssueInstant()
                .plusMinutes(maxSAMLRequestLatencyMin))) {
            throw new InvalidSAMLResponseException(
                    responseGenerator.expiredToken());
        }

        if (samlResponseTokens.getNotBefore() != null) {
            if (currentDateTime.isBefore(samlResponseTokens.getNotBefore())) {
                throw new InvalidSAMLResponseException(
                        responseGenerator.idpRejectedClaim());
            }
        }

        if (samlResponseTokens.getNotOnOrAfter() != null) {
            if (currentDateTime.isAfter(samlResponseTokens.getNotOnOrAfter())) {
                throw new InvalidSAMLResponseException(
                        responseGenerator.idpRejectedClaim());
            }
        }

        Date idpExpiry = DateUtil.toDate(samlProvider.getExpiry());
        if (DateTime.now().isAfter(idpExpiry.getTime())) {
            throw new InvalidSAMLResponseException(
                    responseGenerator.idpRejectedClaim());
        }

        if (!samlProvider.getIssuer().equals(samlResponseTokens.getIssuer())) {
            throw new InvalidSAMLResponseException(
                    responseGenerator.invalidIdentityToken());
        }

        /**
         * Check if the signing key is present in the IDP metadata.
         */
        Boolean isValidResponse = false;

        if (samlResponseTokens.getSigningCertificate() != null) {
            if (!isSigningCertRegistered(samlProvider,
                    samlResponseTokens.getSigningCertificate())) {
                throw new InvalidSAMLResponseException(
                        responseGenerator.invalidIdentityToken());
            }

            try {
                isValidResponse = samlutil.isResponseProfileValid(
                        samlResponseTokens.getResponseSignature(),
                        samlResponseTokens.getSigningCertificate());
            } catch (SAMLInvalidCertificateException ex) {
                throw new InvalidSAMLResponseException(
                        responseGenerator.invalidIdentityToken());
            }
        } else {
            try {
                isValidResponse = samlutil.isResponseProfileValid(
                        samlResponseTokens.getResponseSignature(), samlProvider);
            } catch (SAMLInvalidCertificateException ex) {
                throw new InvalidSAMLResponseException(
                        responseGenerator.invalidIdentityToken());
            }
        }

        if (!isValidResponse) {
            throw new InvalidSAMLResponseException(
                    responseGenerator.invalidIdentityToken());
        }

        return true;
    }

    /**
     * Check if the signing certificate from SAML response is present in IDP
     * SAMl Metadata.
     *
     * @param samlProvider SAMlProvider
     * @param cert Signing certificate.
     * @return True if certificate is present in SAML metadata.
     */
    private Boolean isSigningCertRegistered(SAMLProvider samlProvider,
            String signingCert) {
        ArrayList<String> signingCertificates
                = samlProvider.getSAMLMetadataTokens()
                .getSAMLKeyDescriptors().get("signing");

        return signingCertificates.contains(signingCert);
    }

}
