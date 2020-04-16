package com.seagates3.response.generator;

import com.seagates3.response.ServerResponse;

import io.netty.handler.codec.http.HttpResponseStatus;

public
class BucketPolicyResponseGenerator extends PolicyResponseGenerator {

  @Override public ServerResponse noSuchPolicy() {
    String errorMessage = "The specified bucket does not have a bucket policy.";
    return formatResponse(HttpResponseStatus.NOT_FOUND, "NoSuchPolicy",
                          errorMessage);
  }
}
