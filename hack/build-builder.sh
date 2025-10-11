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

pushd "$SCRIPT_DIR"
docker build -t cuda-builder:12.6 . --push 
popd

echo -e "Build Done/n"