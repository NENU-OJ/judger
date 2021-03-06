#!/bin/bash

IMAGE_NAME="nenuoj-judger"
CONTAINER_NAME="nenuoj-judger"

if [ ! -n "$CONFIG_FILE_PATH" ]; then
  echo "Please set environment variable CONFIG_FILE_PATH, such as /judger/config.ini"
  exit 1
fi

if [ ! -n "$TEST_FILES_PATH" ]; then
  echo "Please set environment variable TEST_FILES_PATH, such as /judger/test_files"
  exit 1
fi

if [ ! -n "$SPJ_FILES_PATH" ]; then
  echo "Please set environment variable SPJ_FILES_PATH, such as /judger/spj_files"
  exit 1
fi

if [ ! -n "$LOGS_PATH" ]; then
  echo "Please set environment variable LOGS_PATH, such as /judger/logs"
  exit 1
fi


docker container rm -f $CONTAINER_NAME 1>/dev/null 2>&1

docker run \
  -d \
  --restart always \
  --cap-add=SYS_PTRACE \
  --name=$CONTAINER_NAME \
  --net nenuoj-net \
  -v /etc/timezone:/etc/timezone:ro \
  -v /etc/localtime:/etc/localtime:ro \
  -v $CONFIG_FILE_PATH:/judger/config.ini \
  -v $TEST_FILES_PATH:/judger/test_files \
  -v $SPJ_FILES_PATH:/judger/spj_files \
  -v $LOGS_PATH:/judger/logs \
  $IMAGE_NAME
