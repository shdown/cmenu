#!/bin/sh

set -e -x

files=$(find -name '*.cpp' -type f | grep -v /CMakeFiles/)

${CXX:-g++} -Wall -Wextra -std=c++17 -O3 -s -D_GLIBCXX_ASSERTIONS $files $(pkg-config --cflags --libs ncursesw) -o cmenu -lm
