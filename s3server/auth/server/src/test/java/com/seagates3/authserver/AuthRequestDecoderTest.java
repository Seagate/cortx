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
 * Original creation date: 09-Feb-2017
 */

package com.seagates3.authserver;

import io.netty.buffer.ByteBuf;
import io.netty.buffer.ByteBufUtil;
import io.netty.buffer.Unpooled;
import io.netty.handler.codec.http.DefaultFullHttpRequest;
import io.netty.handler.codec.http.FullHttpRequest;
import io.netty.handler.codec.http.HttpMethod;
import io.netty.handler.codec.http.HttpVersion;
import io.netty.handler.codec.http.multipart.Attribute;
import io.netty.handler.codec.http.multipart.InterfaceHttpData;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.powermock.api.mockito.mockpolicies.Slf4jMockPolicy;
import org.powermock.core.classloader.annotations.MockPolicy;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;
import org.powermock.reflect.internal.WhiteboxImpl;

import java.io.IOException;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

import static org.junit.Assert.*;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

@RunWith(PowerMockRunner.class)
@PrepareForTest({AuthRequestDecoder.class})
@MockPolicy(Slf4jMockPolicy.class)
public class AuthRequestDecoderTest {

    private FullHttpRequest fullHttpRequest;
    private AuthRequestDecoder authRequestDecoder;

    @Before
    public void setUp() throws Exception {
        fullHttpRequest = new DefaultFullHttpRequest(
                HttpVersion.HTTP_1_1, HttpMethod.POST, "/", getRequestBodyAsByteBuf());

        authRequestDecoder = new AuthRequestDecoder(fullHttpRequest);
    }

    @Test
    public void getRequestBodyAsMapTest() {
        Map<String, String> result = authRequestDecoder.getRequestBodyAsMap();

        assertEquals(getExpectedBody(), result);
    }

    @Test
    public void parseRequestBodyTest() throws Exception {
        List<InterfaceHttpData> datas = mock(List.class);
        Attribute data = mock(Attribute.class);
        Iterator iterator = mock(Iterator.class);

        when(iterator.hasNext()).thenReturn(Boolean.TRUE).thenReturn(Boolean.FALSE);
        when(iterator.next()).thenReturn(data);
        when(datas.iterator()).thenReturn(iterator);
        when(data.getHttpDataType()).thenReturn(InterfaceHttpData.HttpDataType.Attribute);
        when(data.getValue()).thenThrow(IOException.class);

        Map<String, String> result = WhiteboxImpl.invokeMethod(authRequestDecoder,
                "parseRequestBody", datas);

        assertNotNull(result);
        assertTrue(result.isEmpty());
    }

    private ByteBuf getRequestBodyAsByteBuf() {
        String params = "Action=AuthenticateUser&" +
                "x-amz-meta-ics.simpleencryption.status=enabled&" +
                "x-amz-meta-ics.meta-version=1&" +
                "x-amz-meta-ics.meta-version=2&" +
                "samlAssertion=assertion";
        ByteBuf byteBuf = Unpooled.buffer(params.length());
        ByteBufUtil.writeUtf8(byteBuf, params);

        return byteBuf;
    }

    private Map<String, String> getExpectedBody() {
        Map<String, String> expectedRequestBody = new TreeMap<>(String.CASE_INSENSITIVE_ORDER);
        expectedRequestBody.put("Action", "AuthenticateUser");
        expectedRequestBody.put("samlAssertion", "assertion");
        expectedRequestBody.put("x-amz-meta-ics.meta-version", "1,2");
        expectedRequestBody.put("x-amz-meta-ics.simpleencryption.status", "enabled");

        return expectedRequestBody;
    }
}