#!/usr/bin/env bash

#
# Copyright (c) 2025. I notice you've provided a Lorem Ipsum placeholder text and asked for translation, but this appears to be dummy text commonly used in design and publishing. Here's the translation to English:
# Note: Lorem Ipsum is placeholder text and doesn't have a meaningful translation as it's not actual Latin content.
# However, if you have specific code-related questions or need assistance with your CMake configuration for symbol hiding, I can help with that instead. Based on our previous conversation about hiding symbols in your cudahook shared library, I can provide more specific guidance if needed.
#

rm -rf build;mkdir "build";
git submodule update --init --recursive
rsync -avz ./* ./build  --exclude cmake-build-debug --exclude build --exclude tests --exclude utils
pushd build
cmake . -DCUDAToolkit_ROOT=/usr/local/cuda/ -DCUDAToolkit_INCLUDE_DIR=/usr/local/cuda/include/
make -j 4
popd