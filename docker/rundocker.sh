#!/bin/bash

docker run \
  -d \
  --restart always \
  --name=nenuoj-judger \
  -v $CONFIG_FILE_PATH:/judger/config.ini \
  -v $TEST_FILES_PATH:/judger/test_files \
  -v $SPJ_FILES_PATH:/judger/spj_files \
  nenuoj-judger
