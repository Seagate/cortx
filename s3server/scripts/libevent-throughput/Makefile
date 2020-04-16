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
	-L $(BUILD_ROOT_DIR)/third_party/libevent/s3_dist/lib \
	-lgflags -levent \
	-Wl,-rpath,/opt/seagate/s3/libevent

$(CLIENT): $(THIS_DIR)/client.cpp
	g++ -o $@ --std=c++11 -lgflags $<

