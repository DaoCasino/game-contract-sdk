#include <openssl/rand.h>

namespace testing::random_mock {

static int stdlib_rand_seed(const void* buf, int num) {
    assert(num >= sizeof(unsigned int));
    return 1;
}

// Fill the buffer with random bytes.  For each byte in the buffer, we generate
// a random number and clamp it to the range of a byte, 0-255.
static int stdlib_rand_bytes(unsigned char* buf, int num) {
    for (int index = 0; index < num; ++index) {
        buf[index] = std::rand() % 256;
    }
    return 1;
}

static void stdlib_rand_cleanup() {}

static int stdlib_rand_add(const void* buf, int num, double add_entropy) { return 1; }

static int stdlib_rand_status() { return 1; }

// Create the table that will link OpenSSL's rand API to our functions.
RAND_METHOD stdlib_rand_meth = {
    stdlib_rand_seed, stdlib_rand_bytes, stdlib_rand_cleanup, stdlib_rand_add, stdlib_rand_bytes, stdlib_rand_status};

// This is a public-scope accessor method for our table.
RAND_METHOD* RAND_stdlib() { return &stdlib_rand_meth; }

} // namespace testing::random_mock
