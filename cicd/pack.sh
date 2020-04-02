#! /bin/bash

printf "\t=========== Create archive example contracts ===========\n\n"

RED='\033[0;31m'
NC='\033[0m'

asset_dir=$(realpath "build/assets")
mkdir -p $asset_dir

(
    set -x
    cd build/examples
    tar -czf $asset_dir/examples.tar.gz */*/*.abi */*/*.wasm
)
