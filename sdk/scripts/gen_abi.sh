#!/bin/bash

project_root="$(realpath $(dirname "${BASH_SOURCE[0]}")/../../)"

run_in_docker="docker run --rm -v "$project_root":/build -w /build -ti daocasino/daobet-with-cdt:latest"
output="sdk/files/game.abi"
entry="examples/simple/contracts/src/simple.cpp"

$run_in_docker eosio-abigen $entry --contract=game --output=$output \
                --extra-arg="-Isdk/include" \
                --extra-arg="-Iexamples/simple/contracts/include" \
                --extra-arg="-Isdk/libs/platform-contracts/contracts/platform/include" \
                --extra-arg="-Isdk/libs/platform-contracts/contracts/events/include" \
                --extra-arg="-Isdk/libs/platform-contracts/contracts/casino/include" \
                --extra-arg="-I/usr/opt/eosio.cdt/1.6.3/include/eosiolib/capi" \
                --extra-arg="-I/usr/opt/eosio.cdt/1.6.3/include/eosiolib/core" \
                --extra-arg="-I/usr/opt/eosio.cdt/1.6.3/include/eosiolib/contracts"
