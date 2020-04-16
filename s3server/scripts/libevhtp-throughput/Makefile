#!/usr/bin/make -f

THIS_FILE := $(lastword $(MAKEFILE_LIST))
THIS_DIR  := $(abspath $(dir $(THIS_FILE)))
BUILD_ROOT_DIR := $(abspath $(THIS_DIR)/../..)

SERVER := $(THIS_DIR)/server
CLIENT := $(THIS_DIR)/client

.PHONY: all

all: $(SERVER) $(CLIENT)

$(SERVER): $(THIS_DIR)/server.cpp
	g++ -o $@ --std=c++11 $< \
	-I $(BUILD_ROOT_DIR)/third_party/libevent/s3_dist/include \
	-I $(BUILD_ROOT_DIR)/third_party/libevhtp/s3_dist/include/evhtp \
	-L $(BUILD_ROOT_DIR)/third_party/libevent/s3_dist/lib \
	-L $(BUILD_ROOT_DIR)/third_party/libevhtp/s3_dist/lib \
	-lgflags -levhtp -levent -levent_pthreads -levent_openssl -lssl -lcrypto \
	-pthread -Wl,-rpath,/opt/seagate/s3/libevent

$(CLIENT): $(THIS_DIR)/client.cpp
	g++ -o $@ --std=c++11 -lgflags -lcurl $<

