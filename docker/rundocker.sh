#!/bin/bash

IMAGE_NAME="nenuoj-judger"
CONTAINER_NAME="nenuoj-judger"

docker container rm -f $CONTAINER_NAME 1>/dev/null 2>&1

docker run \
  -d \
  --restart always \
  --cap-add=SYS_PTRACE \
  --name=$CONTAINER_NAME \
  -v /etc/timezone:/etc/timezone:ro \
  -v /etc/localtime:/etc/localtime:ro \
  -v $CONFIG_FILE_PATH:/judger/config.ini \
  -v $TEST_FILES_PATH:/judger/test_files \
  -v $SPJ_FILES_PATH:/judger/spj_files \
  -v $LOGS_PATH:/judger/logs \
  -p $HOSTPORT:$CONTAINERPORT \
  $IMAGE_NAME
