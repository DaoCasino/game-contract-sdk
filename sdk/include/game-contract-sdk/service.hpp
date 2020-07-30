#pragma once

#include <array>
#include <type_traits>

#include <eosio/eosio.hpp>
#include <intx/intx.hpp>
#include <vector>

#include <game-contract-sdk/rsa.hpp>


namespace service {
using eosio::checksum256;

// ===================================================================
// Utility functions for operations with different types of PRNG seed
// ===================================================================
template <typename T,
          class = std::enable_if_t<std::is_unsigned<T>::value>>
T cut_to(const checksum256& input) {
    return cut_to<uint128_t>(input) % std::numeric_limits<T>::max();
}

template <> uint128_t cut_to(const checksum256& input) {
    const auto& parts = input.get_array();
    const uint128_t left = parts[0] % std::numeric_limits<uint64_t>::max();
    const uint128_t right = parts[1] % std::numeric_limits<uint64_t>::max();
    // it's not fair way(don't save original distribution), but more simpler
    return (left << (sizeof(uint64_t) * 8)) | right;
}

std::array<uint64_t, 4> split(const checksum256& raw) {
    const auto& parts = raw.get_array();
    return std::array<uint64_t, 4>{
        uint64_t(parts[0] >> 64),
        uint64_t(parts[0]),
        uint64_t(parts[1] >> 64),
        uint64_t(parts[1]),
    };
}


// ===================================================================
// Different PRNG implementations
// ===================================================================

/* Generic PRNG interface */
struct PRNG {
    using Ptr = std::shared_ptr<PRNG>;

    virtual ~PRNG() {};
    virtual uint64_t next(uint64_t from, uint64_t to) = 0;
    uint64_t next() {
        return next(0u, UINT64_MAX);
    };
};

/**
   Implementation of generator based on sha256 mixing with rejection scheme
   details: https://github.com/DaoCasino/PRNG/blob/master/PRNG.pdf
*/
class ShaMixWithRejection : public PRNG {
  public:
    constexpr static intx::uint256 UINT256_MAX = ~intx::uint256(0);

  public:
    explicit ShaMixWithRejection(const checksum256& seed) : _s(to_intx(seed)) {}
    explicit ShaMixWithRejection(checksum256&& seed) : _s(to_intx(seed)) {}

    uint64_t next(uint64_t from, uint64_t to) override {
        eosio::check(to > from, "invalid random range");
        eosio::check(_cur_iter < UINT32_MAX, "too many next() calls");

        const intx::uint256 delta(to - from);
        const intx::uint256 cut_threshold = UINT256_MAX / delta * delta;

        auto lucky_as_hash = mix_bytes();
        auto lucky = to_intx(lucky_as_hash);

        while (lucky >= cut_threshold) {
            auto lucky_bytes = lucky_as_hash.extract_as_byte_array();
            lucky_as_hash = eosio::sha256(reinterpret_cast<const char*>(lucky_bytes.data()), 32);
            lucky = to_intx(lucky_as_hash);
        }

        return uint64_t(lucky % delta + from);
    }

  private:
    // 32bytes of seed and 4 of counter
    checksum256 mix_bytes() {
        static_assert(sizeof(_s) == 32, "invalid `_s` size, should be 32bytes");
        std::array<uint8_t, 36> arr;
        std::memcpy(arr.data(), &_s, sizeof(_s));
        std::memcpy(arr.data() + 32, &_cur_iter, sizeof(_cur_iter));

        _cur_iter++;

        return eosio::sha256(reinterpret_cast<const char*>(arr.data()), arr.size());
    }

    static intx::uint256 to_intx(const checksum256& hash) {
        auto parts = hash.get_array();
        return intx::uint256(
            intx::uint128(uint64_t(parts[0] >> 64), uint64_t(parts[0])),
            intx::uint128(uint64_t(parts[1] >> 64), uint64_t(parts[1]))
        );
    }


  private:
    const intx::uint256 _s;
    uint32_t _cur_iter { 0u };
};

/**
   Mocked implementation of PRNG
   Uses only for testing purposes
*/
class PseudoPRNG : public PRNG {
  public:
    PseudoPRNG(const std::vector<uint64_t>& values) : _values(values), _current(_values.begin()) {}

    uint64_t next(uint64_t from, uint64_t to) override {
        eosio::check(to > from, "invalid random range");

        const uint64_t result = *_current++;
        if (_current == _values.end())
            _current = _values.begin();
        return result;
    }

  private:
    std::vector<uint64_t> _values;
    std::vector<uint64_t>::iterator _current;
};


// ===================================================================
// Shuffler functions
// ===================================================================

/* Shuffler overload for r-value prng */
template <class RandomIt>
void shuffle(RandomIt first, RandomIt last, PRNG::Ptr&& prng) { shuffle(first, last, prng); }

/* Standard shuffler implementation proposed by https://en.cppreference.com/w/cpp/algorithm/random_shuffle */
template <class RandomIt>
void shuffle(RandomIt first, RandomIt last, PRNG::Ptr& prng) {
    typename std::iterator_traits<RandomIt>::difference_type i, n;
    n = last - first;
    for (i = n - 1; i > 0; --i) {
        std::swap(first[i], first[prng->next() % (i + 1)]);
    }
}


// ===================================================================
// SIGNICIDE functions
// ===================================================================

/**
   Signidice sign check and calculate new 256-bit digest.
   Params:
    - prev_digest - signing digest
    - sign - rsa sign
    - pub_key - base64 encoded 2048-bit RSA public key
   Returns - new 256-bit digest calculated from sign
*/
checksum256 signidice(const checksum256& prev_digest, const std::string& sign, const std::string& rsa_key) {
    eosio::check(daobet::rsa_verify(prev_digest, sign, rsa_key), "invalid rsa signature");

    return eosio::sha256(sign.data(), sign.size());
}

} // namespace service
