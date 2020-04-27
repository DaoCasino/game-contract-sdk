#pragma once

#include <array>
#include <type_traits>

#include <eosio/eosio.hpp>
#include <vector>

namespace service {
using eosio::checksum256;

template <typename T, class = std::enable_if_t<std::is_unsigned<T>::value>>
T cut_to(const checksum256& input) {
    return cut_to<uint128_t>(input) % std::numeric_limits<T>::max();
}

template <>
uint128_t cut_to(const checksum256& input) {
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

struct PRNG {
    using Ptr = std::shared_ptr<PRNG>;

    virtual ~PRNG();
    virtual uint64_t next() = 0;
};

// xoshiro256++ prng algo
// http://prng.di.unimi.it/
class Xoshiro: public PRNG {
  public:
    explicit Xoshiro(const checksum256& seed) : _s(split(seed)) {}
    explicit Xoshiro(checksum256&& seed) : _s(split(std::move(seed))) {}
    explicit Xoshiro(std::array<uint64_t, 4>&& seed) : _s(std::move(seed)) {}
    explicit Xoshiro(const std::array<uint64_t, 4>& seed) : _s(seed) {}

    uint64_t next() override {
        const uint64_t result = roll_up(_s[0] + _s[3], 23) + _s[0];

        const uint64_t t = _s[1] << 17;

        _s[2] ^= _s[0];
        _s[3] ^= _s[1];
        _s[1] ^= _s[2];
        _s[0] ^= _s[3];

        _s[2] ^= t;

        _s[3] = roll_up(_s[3], 45);

        return result;
    }

  private:
    static inline uint64_t roll_up(const uint64_t value, const int count) {
        return (value << count) | (value >> (64 - count));
    }

  private:
    std::array<uint64_t, 4> _s;
};

class PseudoPRNG: public PRNG {
public:
    PseudoPRNG(const std::vector<uint64_t>& values) :_values(values), _current(_values.begin()) {}

    uint64_t next() override {
        uint64_t result = *_current++;
        if (_current == _values.end())
            _current = _values.begin();
        return result;
    }

private:
    std::vector<uint64_t> _values;
    std::vector<uint64_t>::iterator _current;
};

template <class RandomIt>
void shuffle(RandomIt first, RandomIt last, PRNG::Ptr && prng) {
    shuffle(first, last, prng);
}

template <class RandomIt>
void shuffle(RandomIt first, RandomIt last, PRNG::Ptr & prng) {
    typename std::iterator_traits<RandomIt>::difference_type i, n;
    n = last - first;
    for (i = n - 1; i > 0; --i) {
        std::swap(first[i], first[prng->next() % (i+1)]);
    }
}

} // namespace service
