#pragma once

#include <eosio/eosio.hpp>
#include <eosio/transaction.hpp>

extern "C" {

/**
  256-bit c-style checksum definition
*/
struct __attribute__((aligned(16))) capi_checksum256 {
    uint8_t hash[32];
};

/**
   Internal RSA sign verification function declaration
   Function definition located in DAOBet's WASM VM
   link: https://github.com/DaoCasino/DAObet/blob/35b88cd1e7f840c36bdf03a06535fce631ef7c01/libraries/chain/wasm_interface.cpp#L806
*/
__attribute__((eosio_wasm_import)) int
rsa_verify(const capi_checksum256* digest, const char* sig, size_t siglen, const char* pub, size_t publen);

}


namespace daobet {

/**
   C++ version of RSA sign verification function
*/
bool rsa_verify(const eosio::checksum256& digest, const std::string& sig, const std::string& pubkey) {
    auto digest_data = digest.extract_as_byte_array();
    return ::rsa_verify(reinterpret_cast<const capi_checksum256*>(digest_data.data()),
                        sig.c_str(),
                        sig.size(),
                        pubkey.c_str(),
                        pubkey.size());
}

} // namespace daobet
