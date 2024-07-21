#!/usr/bin/env bash
set -e

cd build/ && rm -rf *
cmake .. && make -j8