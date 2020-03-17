#pragma once

#include <eosio/eosio.hpp>
#include <eosio/transaction.hpp>


extern "C" {
    struct __attribute__((aligned (16))) capi_checksum256 {
        uint8_t hash[32];
    };

    __attribute__((eosio_wasm_import))
    int rsa_verify( const capi_checksum256* digest, const char* sig,
                   size_t siglen, const char* pub, size_t publen );
}

namespace daobet {
    bool rsa_verify(const eosio::checksum256& digest, const std::string& sig, const std::string& pubkey) {
        auto digest_data = digest.extract_as_byte_array();
        return ::rsa_verify(reinterpret_cast<const capi_checksum256*>(digest_data.data()),
                            sig.c_str(), sig.size(), pubkey.c_str(), pubkey.size());
    }
}
