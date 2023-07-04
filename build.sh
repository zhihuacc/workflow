#!/usr/bin/env bash
echo "Building workflow ..."
mkdir obj
cmake -B obj  -D CMAKE_BUILD_TYPE=Debug
cmake --build obj
cmake --install obj
echo "--- *** ---"

echo "Building tutorial ..."
cd tutorial
mkdir obj
cmake -B obj  -D CMAKE_BUILD_TYPE=Debug
cmake --build obj
cmake --install obj
echo "--- *** ---"
