#!/bin/bash

clang++ source/common.cpp source/indexer.cpp source/searcher.cpp source/main.cpp -I./include -std=gnu++23 -Wall -Werror -Wpedantic -O0 -g -lpoppler -lpoppler-cpp -lncurses -o docsearch
