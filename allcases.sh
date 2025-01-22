#!/bin/bash
TESTCASES=$(for i in tests/8088/v2/*.json.gz; do echo $(basename $i .json.gz); done)
./build/testdriver -f tests/8088/v2 $TESTCASES