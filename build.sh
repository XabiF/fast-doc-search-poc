#!/bin/bash

clang++ source/common.cpp source/indexer.cpp source/searcher.cpp source/main.cpp -I./include -std=gnu++23 -Wall -Werror -Wpedantic -O3 -lpoppler -lpoppler-cpp -lncurses -o docsearch
