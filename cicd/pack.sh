#! /bin/bash

set -eu
set -o pipefail

. "${BASH_SOURCE[0]%/*}/utils.sh"

log "=========== Create archive example contracts ===========\n\n"

asset_dir=$(realpath "build/assets")
mkdir -p $asset_dir

(
    set -x
    cd build/examples
    tar -czf $asset_dir/examples.tar.gz */*/*.abi */*/*.wasm
)
