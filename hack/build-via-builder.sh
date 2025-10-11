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

pushd "$SCRIPT_DIR/../"

docker run -it --rm -v ./:/app cuda-builder:12.6 /app/hack/build.sh

popd