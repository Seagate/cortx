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
 * Original creation date: 15-Oct-2015
 */
package com.seagates3.saml;

import com.seagates3.exception.SAMLInitializationException;
import com.seagates3.exception.SAMLInvalidCertificateException;
import com.seagates3.exception.SAMLReponseParserException;
import com.seagates3.model.SAMLMetadataTokens;
import com.seagates3.model.SAMLProvider;
import com.seagates3.model.SAMLResponseTokens;
import com.seagates3.util.BinaryUtil;
import com.seagates3.util.DateUtil;
import java.io.ByteArrayInputStream;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.security.PublicKey;
import java.security.cert.CertificateException;
import java.security.cert.CertificateFactory;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.TimeZone;
import javax.management.InvalidAttributeValueException;
import org.joda.time.DateTime;
import org.joda.time.DateTimeZone;
import org.opensaml.DefaultBootstrap;
import org.opensaml.saml2.core.Assertion;
import org.opensaml.saml2.core.Attribute;
import org.opensaml.saml2.core.AttributeStatement;
import org.opensaml.saml2.core.Audience;
import org.opensaml.saml2.core.AudienceRestriction;
import org.opensaml.saml2.core.Conditions;
import org.opensaml.saml2.core.Response;
import org.opensaml.saml2.core.Subject;
import org.opensaml.saml2.metadata.EntityDescriptor;
import org.opensaml.saml2.metadata.IDPSSODescriptor;
import org.opensaml.saml2.metadata.KeyDescriptor;
import org.opensaml.saml2.metadata.SingleLogoutService;
import org.opensaml.saml2.metadata.SingleSignOnService;
import org.opensaml.security.SAMLSignatureProfileValidator;
import org.opensaml.xml.Configuration;
import org.opensaml.xml.ConfigurationException;
import org.opensaml.xml.XMLObject;
import org.opensaml.xml.io.Unmarshaller;
import org.opensaml.xml.io.UnmarshallerFactory;
import org.opensaml.xml.io.UnmarshallingException;
import org.opensaml.xml.parse.BasicParserPool;
import org.opensaml.xml.parse.XMLParserException;
import org.opensaml.xml.schema.XSAny;
import org.opensaml.xml.security.credential.BasicCredential;
import org.opensaml.xml.signature.KeyInfo;
import org.opensaml.xml.signature.Signature;
import org.opensaml.xml.signature.SignatureValidator;
import org.opensaml.xml.signature.X509Certificate;
import org.opensaml.xml.signature.X509Data;
import org.opensaml.xml.validation.ValidationException;
import org.w3c.dom.Document;
import org.w3c.dom.Element;

public class SAMLUtilV2 implements SAMLUtil {

    private final BasicParserPool parser;
    private final UnmarshallerFactory unmarshallerFactory;

    private final String SUCCESS = "urn:oasis:names:tc:SAML:2.0:status:Success";
    private final String IDP_SSO_DESCRIPTOR
            = "urn:oasis:names:tc:SAML:2.0:protocol";
    private final String PERSISTENT_SUBJECT_TYPE
            = "urn:oasis:names:tc:SAML:2.0:nameid-format:persistent";

    /**
     * Initialize SAML Parser and unmarshaller.
     *
     * @throws com.seagates3.exception.SAMLInitializationException
     */
    public SAMLUtilV2()
            throws SAMLInitializationException {
        try {
            DefaultBootstrap.bootstrap();
        } catch (ConfigurationException ex) {
            String msg = "Failed to intialize SAML parser.\n" + ex;
            throw new SAMLInitializationException(msg);
        }

        parser = new BasicParserPool();
        parser.setNamespaceAware(true);
        unmarshallerFactory = Configuration.getUnmarshallerFactory();
    }

    /**
     * Parse the SAML metadata and create SAMLProvider object.
     *
     * @param samlProvider SAMLProvider.
     * @param samlMetadata SAML XML metadata.
     * @throws com.seagates3.exception.SAMLInvalidCertificateException
     * @throws com.seagates3.exception.SAMLReponseParserException
     * @throws javax.management.InvalidAttributeValueException
     */
    @Override
    public void getSAMLProvider(SAMLProvider samlProvider, String samlMetadata)
            throws SAMLReponseParserException, InvalidAttributeValueException,
            SAMLInvalidCertificateException {

        SAMLMetadataTokens samlTokens = new SAMLMetadataTokens();

        EntityDescriptor entityDescriptor = getEntityDescriptor(samlMetadata);
        samlProvider.setIssuer(entityDescriptor.getEntityID());

        IDPSSODescriptor iDescriptor
                = entityDescriptor.getIDPSSODescriptor(IDP_SSO_DESCRIPTOR);

        samlProvider.setExpiry(getProviderExpiration(entityDescriptor));

        samlTokens.setSAMLKeyDescriptors(getKeyDescriptors(iDescriptor));
        samlTokens.setSingleSignInService(getSingleSignInServices(iDescriptor));
        samlTokens.setSingleSignOutService(getSingleSignOutServices(iDescriptor));
        samlProvider.setSAMLMetadataTokens(samlTokens);
    }

    /**
     * Parse the SAML response message and identify various entities like issue
     * instant, issuer etc.
     *
     * @param samlResponseMessage SAML response body.
     * @return SAMLReponseTokens
     * @throws com.seagates3.exception.SAMLReponseParserException
     * @throws com.seagates3.exception.SAMLInvalidCertificateException
     */
    @Override
    public SAMLResponseTokens parseSamlResponse(String samlResponseMessage)
            throws SAMLReponseParserException, SAMLInvalidCertificateException {

        Response response;

        try {
            response = unmarshallResponse(samlResponseMessage);
        } catch (XMLParserException | UnmarshallingException ex) {
            String msg = "Unable to parse SAML response.\n" + ex;
            throw new SAMLReponseParserException(msg);
        }

        SAMLResponseTokens samlResponseToken = new SAMLResponseTokens();
        samlResponseToken.setAudience(AUDIENCE_NAME);
        samlResponseToken.setIssuer(response.getIssuer().getValue());

        Boolean isReponseSuccess = response.getStatus().getStatusCode()
                .getValue().equals(SUCCESS);
        samlResponseToken.setIsAuthenticationSuccess(isReponseSuccess);

        /**
         * TODO- SAML response can have multiple assertions. Find out the use
         * case for having multiple assertions in the same response.
         *
         * For now, assume there is a single assertion.
         */
        Assertion assertion = response.getAssertions().get(0);
        Signature sign = assertion.getSignature();

        samlResponseToken.setResponseSignature(sign);

        if (sign.getKeyInfo() != null) {
            String signingSertificate = getSigningCertificate(sign.getKeyInfo());
            samlResponseToken.setSigningCertificate(signingSertificate);
        }

        samlResponseToken.setIssueInstant(assertion.getIssueInstant());

        Subject subject = assertion.getSubject();
        samlResponseToken.setSubject(subject.getNameID().getValue());

        String subjectType = getSubjectType(subject.getNameID().getFormat());
        samlResponseToken.setSubjectType(subjectType);

        samlResponseToken.setResponseAttributes(
                getSAMLAssertionAttributes(assertion));

        setReponseConditions(samlResponseToken, assertion);
        return samlResponseToken;
    }

    /**
     * When verifying a signature of a message it is recommended to first
     * validate the message with a SAML profile validator. This to ensure that
     * the signature follows the standard for XML signatures. Afterwards the
     * cryptography validation of the signature is done by a SignatureValidator.
     *
     * SignatureValidator validates that the signature meets security-related
     * requirements indicated by the SAML profile of XML Signature. Note that
     * this signature verification mechanism does NOT establish trust of the
     * verification credential, only that it successfully cryptographically
     * verifies the signature.
     *
     * @param signature
     * @param SigningCert
     * @return
     * @throws com.seagates3.exception.SAMLInvalidCertificateException
     */
    @Override
    public Boolean isResponseProfileValid(Signature signature,
            String SigningCert) throws SAMLInvalidCertificateException {
        SAMLSignatureProfileValidator profileValidator
                = new SAMLSignatureProfileValidator();
        try {
            profileValidator.validate(signature);
        } catch (ValidationException ex) {
            return false;
        }

        BasicCredential cred = new BasicCredential();
        cred.setPublicKey(getPublicKey(SigningCert));

        SignatureValidator sigValidator = new SignatureValidator(cred);
        try {
            sigValidator.validate(signature);
        } catch (ValidationException ex) {
            return false;
        }

        return true;
    }

    /**
     * Iterate over all the signing certificates obtained from the IDP metadata
     * and check if the response is valid.
     *
     * @param signature
     * @param samlProvider
     * @return
     * @throws com.seagates3.exception.SAMLInvalidCertificateException
     */
    @Override
    public Boolean isResponseProfileValid(Signature signature,
            SAMLProvider samlProvider) throws SAMLInvalidCertificateException {
        ArrayList<String> signingCertificates
                = samlProvider.getSAMLMetadataTokens()
                .getSAMLKeyDescriptors().get("signing");

        for (String cert : signingCertificates) {
            if (isResponseProfileValid(signature, cert)) {
                return true;
            }
        }

        return false;
    }

    /**
     * Return the Role session name
     *
     * @param responseAttributes
     * @return
     */
    @Override
    public String getRoleSessionName(
            Map<String, ArrayList<String>> responseAttributes) {
        return responseAttributes.get(ATTRIBUTE_ROLE_SESSION_NAME).get(0);
    }

    @Override
    public ArrayList<String> getRoles(
            Map<String, ArrayList<String>> responseAttributes) {
        return responseAttributes.get(ATTRIBUTE_ROLE_NAME);
    }

    @Override
    public int getExpirationDuration(SAMLResponseTokens samlResponseTokens,
            int requestDuration) {
        int duration = -1;
        DateTime currentDateTime = DateTime.now(DateTimeZone.UTC);
        if (samlResponseTokens.getNotOnOrAfter() != null) {
            duration = (int) ((samlResponseTokens.getNotOnOrAfter().getMillis()
                    - currentDateTime.getMillis()) / 1000);
        }

        if (duration == -1 || duration > maxCredsDuration) {
            duration = maxCredsDuration;
        }

        if ((requestDuration != -1)
                && (duration > requestDuration)) {
            duration = requestDuration;
        }

        return duration;
    }

    /**
     * Convert the key into java.security.PublicKey
     *
     * @param key Public Key string.
     * @return PublicKey.
     * @throws SAMLInvalidCertificateException
     */
    private PublicKey getPublicKey(String key)
            throws SAMLInvalidCertificateException {
        try {
            CertificateFactory certFactory
                    = CertificateFactory.getInstance("X.509");
            byte[] base64DecodedKey = BinaryUtil.base64DecodedBytes(key);
            InputStream in = new ByteArrayInputStream(base64DecodedKey);
            java.security.cert.X509Certificate cert
                    = (java.security.cert.X509Certificate) certFactory
                    .generateCertificate(in);
            return cert.getPublicKey();
        } catch (CertificateException ex) {
            String msg = "Exception occured while generating public key.\n" + ex;
            throw new SAMLInvalidCertificateException(msg);
        }
    }

    /**
     * Return the subject type
     */
    private String getSubjectType(String subjectType) {
        if (PERSISTENT_SUBJECT_TYPE.equals(subjectType)) {
            return "persistent";
        } else {
            return "transcient";
        }
    }

    /**
     * Return the entity descriptor object.
     */
    private EntityDescriptor getEntityDescriptor(String metadata)
            throws SAMLReponseParserException {
        InputStream inStream = new ByteArrayInputStream(metadata.getBytes(
                StandardCharsets.UTF_8));
        Document inCommonMDDoc;
        try {
            inCommonMDDoc = parser.parse(inStream);
        } catch (XMLParserException ex) {
            String msg = "SAML response has Invalid XML format.\n" + ex;
            throw new SAMLReponseParserException(msg);
        }

        Element metadataRoot = inCommonMDDoc.getDocumentElement();
        Unmarshaller unmarshaller = unmarshallerFactory.getUnmarshaller(
                metadataRoot);

        try {
            return (EntityDescriptor) unmarshaller.unmarshall(metadataRoot);
        } catch (UnmarshallingException ex) {
            String msg = "Failed to marshall SAML response." + ex;
            throw new SAMLReponseParserException(msg);
        }
    }

    /**
     * Return the expiration date of the provider.
     *
     * If expiration is not provided in the metadata, Then set expiration after
     * 100 years. (Default AWS behavior).
     */
    private String getProviderExpiration(
            EntityDescriptor entityDescriptor) {
        DateTime providerExpiry = entityDescriptor.getValidUntil();

        if (providerExpiry == null) {
            Calendar cal = Calendar.getInstance(TimeZone.getTimeZone("UTC"));
            cal.add(Calendar.YEAR, 100);
            return DateUtil.toServerResponseFormat(cal.getTime());
        } else {
            return DateUtil.toServerResponseFormat(providerExpiry.toDate());
        }
    }

    /**
     * Iterate over the metadata and generate key descriptor objects.
     */
    private Map<String, ArrayList<String>> getKeyDescriptors(
            IDPSSODescriptor iDescriptor)
            throws SAMLInvalidCertificateException {
        Map<String, ArrayList<String>> keyDescriptors = new HashMap<>();
        ArrayList<String> encryptionCertificates = new ArrayList<>();
        ArrayList<String> signingCertificates = new ArrayList<>();

        for (KeyDescriptor kDescriptor : iDescriptor.getKeyDescriptors()) {
            String keyUse = kDescriptor.getUse().toString();

            ArrayList<String> certificates = new ArrayList<>();
            for (X509Data x509Data : kDescriptor.getKeyInfo().getX509Datas()) {
                for (X509Certificate cert : x509Data.getX509Certificates()) {
                    certificates.add(cert.getValue());
                }
            }

            if (keyUse.equalsIgnoreCase("encryption")) {
                encryptionCertificates.addAll(certificates);
            } else {
                signingCertificates.addAll(certificates);
            }
        }
        keyDescriptors.put("encryption", encryptionCertificates);
        keyDescriptors.put("signing", signingCertificates);
        return keyDescriptors;
    }

    /**
     *
     * @param iDescriptor
     * @return
     */
    private Map<String, String> getSingleSignInServices(
            IDPSSODescriptor iDescriptor) {
        Map<String, String> singleSignInService = new HashMap<>();
        for (SingleSignOnService sSignOn
                : iDescriptor.getSingleSignOnServices()) {
            if (sSignOn.getBinding().contains("POST")) {
                singleSignInService.put("POST", sSignOn.getLocation());
            } else {
                singleSignInService.put("REDIRECT", sSignOn.getLocation());
            }

        }
        return singleSignInService;
    }

    /**
     *
     * @param iDescriptor
     * @return
     */
    private Map<String, String> getSingleSignOutServices(
            IDPSSODescriptor iDescriptor) {
        Map<String, String> singleSignoutService = new HashMap<>();
        for (SingleLogoutService sSignOut
                : iDescriptor.getSingleLogoutServices()) {
            if (sSignOut.getBinding().contains("POST")) {
                singleSignoutService.put("POST", sSignOut.getLocation());
            } else {
                singleSignoutService.put("REDIRECT", sSignOut.getLocation());
            }

        }
        return singleSignoutService;
    }

    /**
     * Return the certificate which was used to sign the request.
     *
     * @param keyInfo IDP signature key info.
     * @return signing certificate
     * @throws SAMLInvalidCertificateException
     */
    private String getSigningCertificate(KeyInfo keyInfo)
            throws SAMLInvalidCertificateException {
        X509Data x509Data = keyInfo.getX509Datas().get(0);
        X509Certificate cert = x509Data.getX509Certificates().get(0);

        return cert.getValue();
    }

    /**
     * Check if Seagate SP present in the list of audiences. If yes, then set
     * the NotBefore and NotOnOrAfter values for the SAML Response.
     *
     * @param samlResponseToken
     * @param assertion
     */
    private void setReponseConditions(
            SAMLResponseTokens samlResponseToken, Assertion assertion) {
        Conditions responseCondition = assertion.getConditions();
        List<AudienceRestriction> audienceRestrictions
                = responseCondition.getAudienceRestrictions();

        Boolean isSeagateAudiencePresent = false;
        for (AudienceRestriction restriction : audienceRestrictions) {
            List<Audience> audiences = restriction.getAudiences();
            for (Audience audience : audiences) {
                if (audience.getAudienceURI().equals(AUDIENCE_NAME)) {
                    isSeagateAudiencePresent = true;
                    break;
                }
            }
        }

        samlResponseToken.setIsResponseAudience(isSeagateAudiencePresent);
        if (isSeagateAudiencePresent) {
            samlResponseToken.setNotBefore(responseCondition.getNotBefore());
            samlResponseToken.setNotOnOrAfter(
                    responseCondition.getNotOnOrAfter());
        }
    }

    /**
     * Get SAML Assertion attributes from metadata.
     *
     * @param iDescriptor
     * @return
     */
    private Map<String, ArrayList<String>> getSAMLAssertionAttributes(
            Assertion assertion) {
        Map<String, ArrayList<String>> responseAttributes = new HashMap<>();
        List<AttributeStatement> attributeStatements
                = assertion.getAttributeStatements();

        String attributeName;
        ArrayList<String> attributeValues;
        for (AttributeStatement attributeStatement : attributeStatements) {
            List<Attribute> attributes = attributeStatement.getAttributes();
            for (Attribute attribute : attributes) {
                attributeName = attribute.getName();

                attributeValues = new ArrayList<>();
                List<XMLObject> xmlAttributeValues
                        = attribute.getAttributeValues();
                for (XMLObject attibuteValue : xmlAttributeValues) {
                    XSAny xs = (XSAny) attibuteValue;
                    attributeValues.add(xs.getTextContent());
                }

                responseAttributes.put(attributeName, attributeValues);
            }
        }

        return responseAttributes;
    }

    /**
     * Unmarshall the response to Opensaml response object.
     *
     * @param response
     * @return Opensaml Response object
     * @throws XMLParserException
     * @throws UnmarshallingException
     */
    private Response unmarshallResponse(String response)
            throws XMLParserException, UnmarshallingException {
        InputStream in = new ByteArrayInputStream(
                response.getBytes(StandardCharsets.UTF_8));
        Document responseDoc = parser.parse(in);
        Element responseRoot = responseDoc.getDocumentElement();

        Unmarshaller unmarshaller
                = unmarshallerFactory.getUnmarshaller(responseRoot);
        return (Response) unmarshaller.unmarshall(responseRoot);
    }
}
