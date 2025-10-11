#! /usr/bin/env bash
set -e
set -x

get_script_dir() {
    local source="${BASH_SOURCE[0]}"
    while [[ -h "$source" ]]; do
        local dir="$(cd -P "$(dirname "$source")" && pwd)"
        source="$(readlink "$source")"
        [[ $source != /* ]] && source="$dir/$source"
    done
    cd -P "$(dirname "$source")" && pwd
}

SCRIPT_DIR=$(get_script_dir)

pushd "$SCRIPT_DIR/.."
git submodule update --init --recursive
rm -rf build;mkdir "build";
rsync -avz ./* ./build  --exclude cmake-build-debug --exclude build --exclude tests --exclude utils
cd build
cmake . -DCUDAToolkit_ROOT=/usr/local/cuda/ -DCUDAToolkit_INCLUDE_DIR=/usr/local/cuda/include/ -DCMAKE_BUILD_TYPE=Release
make -j $(nproc)
popd

echo -e "Build Done/n"