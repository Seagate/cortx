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
 * Original author:  Sachitanand Shelake
 * Original creation date: 18-November-2019
 */

package com.seagates3.authserver;

import static io.netty.handler.codec.http.HttpHeaders.Names.CONNECTION;
import static io.netty.handler.codec.http.HttpHeaders.Names.CONTENT_LENGTH;
import static io.netty.handler.codec.http.HttpHeaders.Names.CONTENT_TYPE;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import io.netty.channel.ChannelFutureListener;
import io.netty.channel.ChannelHandlerContext;
import io.netty.handler.codec.http.DefaultFullHttpResponse;
import io.netty.handler.codec.http.FullHttpRequest;
import io.netty.handler.codec.http.FullHttpResponse;
import io.netty.handler.codec.http.HttpHeaders;
import io.netty.handler.codec.http.HttpResponseStatus;
import io.netty.handler.codec.http.HttpVersion;

public
class AuthServerHeadHandler {

 private
  final Logger LOGGER =
      LoggerFactory.getLogger(AuthServerHeadHandler.class.getName());

  final ChannelHandlerContext ctx;
  final FullHttpRequest httpRequest;
  final Boolean keepAlive;

 public
  AuthServerHeadHandler(ChannelHandlerContext ctx,
                        FullHttpRequest httpRequest) {
    this.ctx = ctx;
    this.httpRequest = httpRequest;
    keepAlive = HttpHeaders.isKeepAlive(httpRequest);
  }

 public
  void run() {
    if (httpRequest.getUri().startsWith("/auth/health")) {
      // Generate Auth Server health check response with Status 200
      FullHttpResponse response;
      response = new DefaultFullHttpResponse(HttpVersion.HTTP_1_1,
                                             HttpResponseStatus.OK);
      LOGGER.debug("Sending Auth server health check response with status [" +
                   response.getStatus() + "]");

      response.headers().set(CONTENT_TYPE, "text/xml");
      response.headers().set(CONTENT_LENGTH,
                             response.content().readableBytes());

      if (!keepAlive) {
        ctx.write(response).addListener(ChannelFutureListener.CLOSE);
        LOGGER.debug("Response sent successfully and Connection was closed.");
      } else {
        response.headers().set(CONNECTION, HttpHeaders.Values.KEEP_ALIVE);
        ctx.writeAndFlush(response);
        LOGGER.debug("Response sent successfully and Connection kept alive.");
      }
    }
  }
}
