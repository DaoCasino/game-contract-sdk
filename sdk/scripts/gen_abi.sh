#!/bin/bash

#TODO fix
echo "WARN: need to run from project root"

eosio-abigen examples/simple/src/simple.cpp --contract=game --output=sdk/files/game.abi \
    --extra-arg="-Isdk/include" \
    --extra-arg="-Iexamples/simple/include" \
    --extra-arg="-Isdk/libs/platform-contracts/contracts/platform/include" \
    --extra-arg="-Isdk/libs/platform-contracts/contracts/events/include" \
    --extra-arg="-Isdk/libs/platform-contracts/contracts/casino/include" \
    --extra-arg="-I/usr/opt/eosio.cdt/1.6.3/include/eosiolib/capi" \
    --extra-arg="-I/usr/opt/eosio.cdt/1.6.3/include/eosiolib/core" \
    --extra-arg="-I/usr/opt/eosio.cdt/1.6.3/include/eosiolib/contracts"
