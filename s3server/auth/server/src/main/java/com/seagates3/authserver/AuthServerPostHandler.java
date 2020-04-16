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
 * Original creation date: 23-Nov-2015
 */
package com.seagates3.authserver;

import static io.netty.handler.codec.http.HttpHeaders.Names.CONNECTION;
import static io.netty.handler.codec.http.HttpHeaders.Names.CONTENT_LENGTH;
import static io.netty.handler.codec.http.HttpHeaders.Names.CONTENT_TYPE;

import com.seagates3.controller.FaultPointsController;
import com.seagates3.controller.IAMController;
import com.seagates3.controller.SAMLWebSSOController;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.generator.FaultPointsResponseGenerator;
import com.seagates3.response.generator.ResponseGenerator;
import com.seagates3.util.BinaryUtil;

import io.netty.buffer.Unpooled;
import io.netty.channel.ChannelFutureListener;
import io.netty.channel.ChannelHandlerContext;
import io.netty.handler.codec.http.DefaultFullHttpResponse;
import io.netty.handler.codec.http.FullHttpRequest;
import io.netty.handler.codec.http.FullHttpResponse;
import io.netty.handler.codec.http.HttpHeaders;
import io.netty.handler.codec.http.HttpVersion;
import java.nio.charset.StandardCharsets;
import java.util.Map;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import com.seagates3.util.BinaryUtil;

public class AuthServerPostHandler {

    private final Logger LOGGER = LoggerFactory.getLogger(
            AuthServerHandler.class.getName());

    final ChannelHandlerContext ctx;
    final FullHttpRequest httpRequest;
    final Boolean keepAlive;

    public AuthServerPostHandler(ChannelHandlerContext ctx,
            FullHttpRequest httpRequest) {
        this.httpRequest = httpRequest;
        this.ctx = ctx;
        keepAlive = HttpHeaders.isKeepAlive(httpRequest);
    }

    public void run() {
        Map<String, String> requestBody = getHttpRequestBodyAsMap();

        // Generate request Id per request
        if (!(requestBody.get("Request_id") == null ||
              (requestBody.get("Request_id")).isEmpty())) {
          AuthServerConfig.setReqId(requestBody.get("Request_id"));
        } else {
          AuthServerConfig.setReqId(BinaryUtil.getAlphaNumericUUID());
        }

        if (httpRequest.getUri().startsWith("/saml")) {
            LOGGER.debug("Calling SAML WebSSOControler.");
            FullHttpResponse response = new SAMLWebSSOController(requestBody)
                    .samlSignIn();
            returnHTTPResponse(response);
        } else {
            ServerResponse serverResponse;
            String action = requestBody.get("Action");
            if (action == null) {
                LOGGER.debug("Request action can not be null.");
                returnHTTPResponse(new ResponseGenerator().badRequest());
                return;
            }
            LOGGER.debug("Requested action: " + action);
            if (isFiRequest(action)) {
                serverResponse = serveFiRequest(requestBody);
            } else {
                serverResponse = serveIamRequest(requestBody);
            }

            returnHTTPResponse(serverResponse);
        }
    }

    /**
     * Read the requestResponse object and send the response to the client.
     */
    private void returnHTTPResponse(ServerResponse requestResponse) {
        String responseBody = requestResponse.getResponseBody();
        FullHttpResponse response = new DefaultFullHttpResponse(HttpVersion.HTTP_1_1,
                requestResponse.getResponseStatus(),
                Unpooled.wrappedBuffer(responseBody.getBytes(StandardCharsets.UTF_8))
        );
        response.headers().set(CONTENT_TYPE, "text/xml");
        response.headers().set(CONTENT_LENGTH, response.content().readableBytes());

        returnHTTPResponse(response);
    }

    private void returnHTTPResponse(FullHttpResponse response) {

        LOGGER.info("Sending HTTP Response code [" +
                               response.getStatus() + "]");

        if (!keepAlive) {
            ctx.write(response).addListener(ChannelFutureListener.CLOSE);

            LOGGER.debug("Response sent successfully.");
            LOGGER.debug("Connection closed.");
        } else {
            response.headers().set(CONNECTION, HttpHeaders.Values.KEEP_ALIVE);
            ctx.writeAndFlush(response);

            LOGGER.debug("Response sent successfully.");
            LOGGER.debug("Connection kept alive.");
        }
    }

    private Map<String, String> getHttpRequestBodyAsMap() {
        AuthRequestDecoder decoder = new AuthRequestDecoder(httpRequest);
        return decoder.getRequestBodyAsMap();
    }

    private boolean isFiRequest(String request) {
        return request.equals("InjectFault") || request.equals("ResetFault");
    }

    private ServerResponse serveFiRequest(Map<String, String> requestBody) {
        FaultPointsController faultPointsController;
        ServerResponse serverResponse;

        if (!AuthServerConfig.isFaultInjectionEnabled()) {
            serverResponse = new FaultPointsResponseGenerator()
                    .badRequest("Fault injection is disabled. Invalid request.");
        } else {
            faultPointsController = new FaultPointsController();
            if (requestBody.get("Action").equals("InjectFault")) {
                serverResponse = faultPointsController.set(requestBody);
            } else {
                serverResponse = faultPointsController.reset(requestBody);
            }
        }

        return serverResponse;
    }

    private ServerResponse serveIamRequest(Map<String, String> requestBody) {
        IAMController iamController = new IAMController();
        return iamController.serve(httpRequest, requestBody);
    }
}
